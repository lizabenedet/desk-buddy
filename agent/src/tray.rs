//! Tray icon: the agent runs on a background tokio runtime while the main
//! thread pumps Windows messages (the tray needs a UI thread).

use std::sync::{Arc, Mutex};
use std::time::Duration;

use log::{error, info};
use tokio::sync::watch;
use tray_icon::menu::{CheckMenuItem, Menu, MenuEvent, MenuItem, PredefinedMenuItem};
use tray_icon::{Icon, TrayIconBuilder};
use windows_sys::Win32::UI::WindowsAndMessaging::{
    DispatchMessageW, MSG, MsgWaitForMultipleObjects, PM_REMOVE, PeekMessageW, QS_ALLINPUT, TranslateMessage,
};

use crate::config::{self, Config};
use crate::status::{SharedStatus, Status};
use crate::{autostart, link};

const REFRESH: Duration = Duration::from_secs(1);
const MAX_SOURCE_LINES: usize = 4;

pub fn run(config: Config) {
    let status: SharedStatus = Arc::new(Mutex::new(Status::default()));
    let (shutdown_tx, shutdown_rx) = watch::channel(false);

    let agent = {
        let status = status.clone();
        std::thread::spawn(move || {
            let rt = tokio::runtime::Runtime::new().expect("tokio runtime");
            rt.block_on(link::run(config, status, shutdown_rx));
        })
    };

    if let Err(e) = pump(&status) {
        error!("tray failed: {e}");
    }
    info!("exiting");
    let _ = shutdown_tx.send(true);
    let _ = agent.join();
}

fn pump(status: &SharedStatus) -> Result<(), Box<dyn std::error::Error>> {
    let menu = Menu::new();
    let connection = MenuItem::new("Buddy: conectando...", false, None);
    let sources: Vec<MenuItem> = (0..MAX_SOURCE_LINES).map(|_| MenuItem::new("", false, None)).collect();
    let open_folder = MenuItem::new("Abrir pasta de configuração", true, None);
    let startup = CheckMenuItem::new("Iniciar com o Windows", true, autostart::is_enabled(), None);
    let quit = MenuItem::new("Sair", true, None);

    menu.append(&connection)?;
    for item in &sources {
        menu.append(item)?;
    }
    menu.append_items(&[&PredefinedMenuItem::separator(), &open_folder, &startup, &PredefinedMenuItem::separator(), &quit])?;

    let tray = TrayIconBuilder::new()
        .with_menu(Box::new(menu))
        .with_tooltip("desk-buddy")
        .with_icon(icon())
        .build()?;

    let mut shown = (String::new(), Vec::<String>::new());
    loop {
        // Wake for window messages or once a second to refresh the status lines.
        unsafe { MsgWaitForMultipleObjects(0, std::ptr::null(), 0, REFRESH.as_millis() as u32, QS_ALLINPUT) };
        let mut msg: MSG = unsafe { std::mem::zeroed() };
        while unsafe { PeekMessageW(&mut msg, std::ptr::null_mut(), 0, 0, PM_REMOVE) } != 0 {
            unsafe {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        while let Ok(event) = MenuEvent::receiver().try_recv() {
            if event.id == quit.id() {
                return Ok(());
            } else if event.id == startup.id() {
                if let Err(e) = autostart::set(startup.is_checked()) {
                    error!("autostart: {e}");
                }
                startup.set_checked(autostart::is_enabled());
            } else if event.id == open_folder.id() {
                let _ = std::fs::create_dir_all(config::dir());
                let _ = std::process::Command::new("explorer").arg(config::dir()).spawn();
            }
        }

        let s = status.lock().unwrap().clone();
        let current = (s.connection_line(), s.source_lines());
        if current != shown {
            connection.set_text(&current.0);
            for (i, item) in sources.iter().enumerate() {
                item.set_text(current.1.get(i).map(String::as_str).unwrap_or(""));
            }
            let _ = tray.set_tooltip(Some(format!("desk-buddy\n{}", current.0)));
            shown = current;
        }
    }
}

/// 32x32 version of the buddy's face: two rounded cyan eyes on a dark disc.
fn icon() -> Icon {
    const N: usize = 32;
    let mut rgba = vec![0u8; N * N * 4];
    let in_rounded = |x: f32, y: f32, x0: f32, y0: f32, x1: f32, y1: f32, r: f32| {
        let cx = x.clamp(x0 + r, x1 - r);
        let cy = y.clamp(y0 + r, y1 - r);
        x >= x0 && x <= x1 && y >= y0 && y <= y1 && (x - cx).powi(2) + (y - cy).powi(2) <= r * r
    };
    for py in 0..N {
        for px in 0..N {
            let (x, y) = (px as f32 + 0.5, py as f32 + 0.5);
            let color = if in_rounded(x, y, 7.0, 9.0, 13.0, 21.0, 2.5) || in_rounded(x, y, 19.0, 9.0, 25.0, 21.0, 2.5) {
                [0x40, 0xE0, 0xFF, 0xFF]
            } else if (x - 16.0).powi(2) + (y - 16.0).powi(2) <= 15.5 * 15.5 {
                [0x14, 0x18, 0x1E, 0xFF]
            } else {
                [0, 0, 0, 0]
            };
            rgba[(py * N + px) * 4..][..4].copy_from_slice(&color);
        }
    }
    Icon::from_rgba(rgba, N as u32, N as u32).expect("valid icon")
}
