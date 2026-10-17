//! "Start with Windows": a value under HKCU\...\Run pointing at buddy-tray.exe.

use std::io;
use std::path::PathBuf;

use winreg::RegKey;
use winreg::enums::HKEY_CURRENT_USER;

const RUN_KEY: &str = r"Software\Microsoft\Windows\CurrentVersion\Run";
const VALUE: &str = "desk-buddy";
pub const TRAY_EXE: &str = "buddy-tray.exe";

/// buddy-tray.exe next to whichever of our binaries is running.
pub fn tray_exe() -> io::Result<PathBuf> {
    let me = std::env::current_exe()?;
    Ok(me.with_file_name(TRAY_EXE))
}

fn command() -> io::Result<String> {
    Ok(format!("\"{}\"", tray_exe()?.display()))
}

pub fn is_enabled() -> bool {
    let Ok(key) = RegKey::predef(HKEY_CURRENT_USER).open_subkey(RUN_KEY) else { return false };
    let current: Result<String, _> = key.get_value(VALUE);
    matches!((current, command()), (Ok(c), Ok(expected)) if c == expected)
}

pub fn set(enabled: bool) -> io::Result<()> {
    let (key, _) = RegKey::predef(HKEY_CURRENT_USER).create_subkey(RUN_KEY)?;
    if enabled {
        key.set_value(VALUE, &command()?)
    } else {
        match key.delete_value(VALUE) {
            Err(e) if e.kind() == io::ErrorKind::NotFound => Ok(()),
            other => other,
        }
    }
}
