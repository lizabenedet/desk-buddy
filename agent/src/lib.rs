//! desk-buddy PC agent. Two binaries share this library:
//! `buddy` (console: hook, pair, run...) and `buddy-tray` (no console window).

pub mod autostart;
pub mod config;
pub mod hook;
pub mod link;
pub mod logging;
pub mod metrics;
pub mod protocol;
pub mod status;
pub mod tray;
