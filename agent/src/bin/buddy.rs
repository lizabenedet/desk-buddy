//! Console entry point.

use std::process::ExitCode;
use std::sync::{Arc, Mutex};

use clap::{Parser, Subcommand};
use desk_buddy_agent::config::{self, Config};
use desk_buddy_agent::status::Status;
use desk_buddy_agent::{autostart, hook, link, logging};
use tokio::sync::watch;

#[derive(Parser)]
#[command(name = "buddy", version, about = "Agente do PC para o desk-buddy")]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    /// Roda o agente aqui, com logs no terminal (Ctrl+C para sair).
    Run,
    /// Usado pelos hooks do Claude Code: lê o evento do stdin, não imprime nada.
    Hook,
    /// Salva o token mostrado na tela "Sistema" do buddy.
    Pair {
        token: String,
        /// Endereço do buddy (ex.: ws://127.0.0.1:8765/ws para o simulador).
        #[arg(long)]
        url: Option<String>,
    },
    /// Mostra o trecho de settings.json que liga os hooks do Claude Code.
    Hooks,
    /// Liga ou desliga o início automático com o Windows (buddy-tray.exe).
    Autostart {
        #[arg(value_parser = ["on", "off"])]
        state: String,
    },
}

fn main() -> ExitCode {
    let cli = Cli::parse();
    match cli.command {
        Command::Hook => {
            hook::run();
            ExitCode::SUCCESS // always: a failing hook must never bother Claude
        }
        Command::Run => run(),
        Command::Pair { token, url } => pair(token, url),
        Command::Hooks => {
            println!("{}", hooks_snippet());
            ExitCode::SUCCESS
        }
        Command::Autostart { state } => match autostart::set(state == "on") {
            Ok(()) => {
                println!("Início automático: {}", if autostart::is_enabled() { "ligado" } else { "desligado" });
                ExitCode::SUCCESS
            }
            Err(e) => fail(&format!("não consegui alterar o início automático: {e}")),
        },
    }
}

fn fail(msg: &str) -> ExitCode {
    eprintln!("{msg}");
    ExitCode::FAILURE
}

fn run() -> ExitCode {
    logging::to_console();
    let config = match Config::load() {
        Ok(c) => c,
        Err(e) => return fail(&format!("{}: {e}", config::path().display())),
    };
    if config.token.is_empty() {
        log::warn!("sem token: rode `buddy pair <TOKEN>` com o token da tela Sistema do buddy");
    }
    let rt = tokio::runtime::Runtime::new().expect("tokio runtime");
    rt.block_on(async {
        let status = Arc::new(Mutex::new(Status::default()));
        let (tx, rx) = watch::channel(false);
        let agent = tokio::spawn(link::run(config, status, rx));
        let _ = tokio::signal::ctrl_c().await;
        let _ = tx.send(true);
        let _ = agent.await;
    });
    ExitCode::SUCCESS
}

fn pair(token: String, url: Option<String>) -> ExitCode {
    let mut config = match Config::load() {
        Ok(c) => c,
        Err(e) => return fail(&format!("{}: {e}", config::path().display())),
    };
    config.token = token.trim().to_uppercase();
    if let Some(url) = url {
        config.url = url;
    }
    match config.save() {
        Ok(()) => {
            println!("Salvo em {}", config::path().display());
            println!("Buddy: {}  token: {}", config.url, config.token);
            ExitCode::SUCCESS
        }
        Err(e) => fail(&format!("não consegui salvar: {e}")),
    }
}

/// Hooks block for ~/.claude/settings.json pointing at this executable.
fn hooks_snippet() -> String {
    let exe = std::env::current_exe().map(|p| p.display().to_string()).unwrap_or_else(|_| "buddy".into());
    let command = format!("\"{exe}\" hook");
    let entry = serde_json::json!([{ "hooks": [{ "type": "command", "command": command, "timeout": 5 }] }]);
    let events = ["SessionStart", "UserPromptSubmit", "Notification", "Stop", "SessionEnd"];
    let hooks: serde_json::Map<String, serde_json::Value> =
        events.iter().map(|e| (e.to_string(), entry.clone())).collect();
    serde_json::to_string_pretty(&serde_json::json!({ "hooks": hooks })).unwrap()
}
