//! The agent's main loop: keeps one WebSocket connection to the buddy,
//! streams metrics, forwards hook events and reconnects with backoff.

use std::time::{Duration, Instant};

use futures_util::{SinkExt, StreamExt};
use log::{info, warn};
use tokio::io::{AsyncBufReadExt, AsyncReadExt, BufReader};
use tokio::net::TcpListener;
use tokio::sync::{mpsc, watch};
use tokio::time::{interval, sleep, timeout};
use tokio_tungstenite::tungstenite::Message;

use crate::config::{Config, HOOK_PORT};
use crate::hook::HookEvent;
use crate::metrics::Collector;
use crate::protocol::{Incoming, Metrics, Outgoing, VERSION};
use crate::status::SharedStatus;

const CONNECT_TIMEOUT: Duration = Duration::from_secs(5);
const BACKOFF_MIN: Duration = Duration::from_secs(1);
const BACKOFF_MAX: Duration = Duration::from_secs(30);
/// Wrong token: no point hammering the buddy.
const BAD_TOKEN_RETRY: Duration = Duration::from_secs(60);
/// Hook events older than this when we (re)connect are stale.
const HOOK_MAX_AGE: Duration = Duration::from_secs(30);

struct QueuedHook {
    at: Instant,
    message: Outgoing,
}

/// Runs until `shutdown` turns true. Closes the connection cleanly on the way out.
pub async fn run(config: Config, status: SharedStatus, mut shutdown: watch::Receiver<bool>) {
    let (hook_tx, mut hook_rx) = mpsc::channel(32);
    let (metrics_tx, metrics_rx) = watch::channel(Metrics::default());

    tokio::spawn(listen_hooks(hook_tx));
    tokio::spawn(sample_metrics(config.clone(), metrics_tx, status.clone()));

    let mut backoff = BACKOFF_MIN;
    loop {
        let result = tokio::select! {
            r = session(&config, &mut hook_rx, metrics_rx.clone(), &status, shutdown.clone()) => r,
            _ = shutdown.changed() => return,
        };
        set_connected(&status, false, None);
        let wait = match result {
            Ok(Ended::Shutdown) => return,
            Ok(Ended::Closed) => {
                // We were connected: try again right away, from a fresh backoff.
                info!("connection closed by the buddy");
                backoff = BACKOFF_MIN;
                BACKOFF_MIN
            }
            Err(Failure::BadToken) => {
                warn!("buddy rejected the token; run `buddy pair <TOKEN>`");
                set_connected(&status, false, Some("token errado".into()));
                BAD_TOKEN_RETRY
            }
            Err(Failure::Other(e)) => {
                warn!("{e}");
                set_connected(&status, false, Some(e));
                let wait = backoff;
                backoff = (backoff * 2).min(BACKOFF_MAX);
                wait
            }
        };
        tokio::select! {
            _ = sleep(wait) => {}
            _ = shutdown.changed() => return,
        }
    }
}

enum Ended {
    Closed,
    Shutdown,
}

enum Failure {
    BadToken,
    Other(String),
}

impl<E: std::fmt::Display> From<E> for Failure {
    fn from(e: E) -> Self {
        Failure::Other(e.to_string())
    }
}

async fn session(
    config: &Config,
    hooks: &mut mpsc::Receiver<QueuedHook>,
    mut metrics: watch::Receiver<Metrics>,
    status: &SharedStatus,
    mut shutdown: watch::Receiver<bool>,
) -> Result<Ended, Failure> {
    let (mut ws, _) = timeout(CONNECT_TIMEOUT, tokio_tungstenite::connect_async(config.url.as_str()))
        .await
        .map_err(|_| Failure::Other(format!("sem resposta de {}", config.url)))?
        .map_err(|e| Failure::Other(format!("{}: {e}", config.url)))?;

    let hello = Outgoing::Hello { version: VERSION, token: config.token.clone() };
    ws.send(Message::text(hello.to_json())).await?;
    let reply = timeout(CONNECT_TIMEOUT, ws.next()).await.map_err(|_| "o buddy não respondeu ao hello")?;
    match reply {
        Some(Ok(Message::Text(text))) => match serde_json::from_str::<Incoming>(&text) {
            Ok(Incoming::Welcome { .. }) => {}
            Ok(Incoming::Error { reason }) if reason == "bad_token" => return Err(Failure::BadToken),
            Ok(Incoming::Error { reason }) => return Err(Failure::Other(format!("buddy recusou: {reason}"))),
            Err(e) => return Err(Failure::Other(format!("resposta inesperada: {e}"))),
        },
        _ => return Err(Failure::Other("conexão fechada no hello".into())),
    }
    info!("connected to {}", config.url);
    set_connected(status, true, None);

    // Hooks that piled up while disconnected: only recent ones still matter.
    while let Ok(q) = hooks.try_recv() {
        if q.at.elapsed() < HOOK_MAX_AGE {
            ws.send(Message::text(q.message.to_json())).await?;
        }
    }

    let mut tick = interval(Duration::from_millis(config.metrics_interval_ms.max(500)));
    loop {
        tokio::select! {
            incoming = ws.next() => match incoming {
                None | Some(Err(_)) | Some(Ok(Message::Close(_))) => return Ok(Ended::Closed),
                Some(Ok(Message::Text(text))) => {
                    if let Ok(Incoming::Error { reason }) = serde_json::from_str::<Incoming>(&text) {
                        warn!("buddy reported: {reason}");
                    }
                }
                Some(Ok(_)) => {}
            },
            Some(q) = hooks.recv() => {
                ws.send(Message::text(q.message.to_json())).await?;
            }
            _ = tick.tick() => {
                // Metrics double as the keep-alive the buddy expects.
                let m = metrics.borrow_and_update().clone();
                ws.send(Message::text(Outgoing::Metrics(m).to_json())).await?;
            }
            _ = shutdown.changed() => {
                let _ = ws.close(None).await;
                return Ok(Ended::Shutdown);
            }
        }
    }
}

fn set_connected(status: &SharedStatus, connected: bool, error: Option<String>) {
    let mut s = status.lock().unwrap();
    s.connected = connected;
    if connected || error.is_some() {
        s.last_error = error;
    }
}

/// Accepts one JSON line per connection from `buddy hook` processes.
async fn listen_hooks(tx: mpsc::Sender<QueuedHook>) {
    let listener = match TcpListener::bind(("127.0.0.1", HOOK_PORT)).await {
        Ok(l) => l,
        Err(e) => {
            warn!("can't listen for hooks on 127.0.0.1:{HOOK_PORT}: {e} (another agent running?)");
            return;
        }
    };
    loop {
        let Ok((stream, _)) = listener.accept().await else { continue };
        let tx = tx.clone();
        tokio::spawn(async move {
            let mut line = String::new();
            let mut reader = BufReader::new(stream).take(4096);
            if timeout(Duration::from_secs(2), reader.read_line(&mut line)).await.is_err() {
                return;
            }
            match serde_json::from_str::<HookEvent>(line.trim()) {
                Ok(ev) => {
                    info!("hook: {:?} {}", ev.event, ev.project.as_deref().unwrap_or(""));
                    let _ = tx.send(QueuedHook { at: Instant::now(), message: ev.into_message() }).await;
                }
                Err(e) => warn!("bad hook line: {e}"),
            }
        });
    }
}

/// Samples every source on a blocking thread and publishes the latest reading.
async fn sample_metrics(config: Config, tx: watch::Sender<Metrics>, status: SharedStatus) {
    let mut collector = Some(Collector::new(&config.lhm_url));
    let mut tick = interval(Duration::from_millis(config.metrics_interval_ms.max(500)));
    loop {
        tick.tick().await;
        let mut c = collector.take().expect("collector is put back every round");
        let (c, metrics, sources) = tokio::task::spawn_blocking(move || {
            let (m, s) = c.sample();
            (c, m, s)
        })
        .await
        .expect("sampling doesn't panic");
        collector = Some(c);
        status.lock().unwrap().sources = sources;
        if tx.send(metrics).is_err() {
            return;
        }
    }
}
