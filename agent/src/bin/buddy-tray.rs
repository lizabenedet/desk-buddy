// Agent in the tray, no console window. This is what starts with Windows.
#![windows_subsystem = "windows"]

use desk_buddy_agent::{config::Config, logging, tray};

fn main() {
    logging::to_file();
    let config = Config::load().unwrap_or_else(|e| {
        log::error!("config: {e}; using defaults");
        Config::default()
    });
    tray::run(config);
}
