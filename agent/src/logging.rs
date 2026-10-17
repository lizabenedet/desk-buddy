use std::fs::File;

use simplelog::{ColorChoice, Config, LevelFilter, TermLogger, TerminalMode, WriteLogger};

use crate::config;

pub fn to_console() {
    let _ = TermLogger::init(LevelFilter::Info, Config::default(), TerminalMode::Mixed, ColorChoice::Auto);
}

/// %APPDATA%\desk-buddy\agent.log, recreated on every start.
pub fn to_file() {
    let _ = std::fs::create_dir_all(config::dir());
    if let Ok(file) = File::create(config::dir().join("agent.log")) {
        let _ = WriteLogger::init(LevelFilter::Info, Config::default(), file);
    }
}
