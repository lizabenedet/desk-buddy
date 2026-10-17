//! Agent settings, stored as TOML in %APPDATA%\desk-buddy\config.toml.

use std::path::PathBuf;
use std::{fs, io};

use serde::{Deserialize, Serialize};

pub const DEFAULT_URL: &str = "ws://buddy.local/ws";
pub const DEFAULT_LHM_URL: &str = "http://localhost:8085/data.json";
/// Port `buddy hook` uses to hand events to the running agent (localhost only).
pub const HOOK_PORT: u16 = 47321;

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Config {
    /// WebSocket address of the buddy (or the simulator: ws://127.0.0.1:8765/ws).
    pub url: String,
    /// Pairing token shown on the buddy's "Sistema" screen.
    pub token: String,
    /// LibreHardwareMonitor web server; GPU and temperatures come from here.
    pub lhm_url: String,
    pub metrics_interval_ms: u64,
}

impl Default for Config {
    fn default() -> Self {
        Config {
            url: DEFAULT_URL.to_string(),
            token: String::new(),
            lhm_url: DEFAULT_LHM_URL.to_string(),
            metrics_interval_ms: 2000,
        }
    }
}

/// `DESK_BUDDY_CONFIG_DIR` overrides it (e.g. a separate config for the simulator).
pub fn dir() -> PathBuf {
    if let Some(d) = std::env::var_os("DESK_BUDDY_CONFIG_DIR") {
        return PathBuf::from(d);
    }
    dirs::config_dir().unwrap_or_else(|| PathBuf::from(".")).join("desk-buddy")
}

pub fn path() -> PathBuf {
    dir().join("config.toml")
}

impl Config {
    /// Missing file = defaults; a broken file is an error worth reporting.
    pub fn load() -> io::Result<Config> {
        match fs::read_to_string(path()) {
            Ok(text) => Config::from_toml(&text),
            Err(e) if e.kind() == io::ErrorKind::NotFound => Ok(Config::default()),
            Err(e) => Err(e),
        }
    }

    pub fn save(&self) -> io::Result<()> {
        fs::create_dir_all(dir())?;
        fs::write(path(), self.to_toml())
    }

    fn from_toml(text: &str) -> io::Result<Config> {
        toml::from_str(text).map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))
    }

    fn to_toml(&self) -> String {
        toml::to_string_pretty(self).expect("config always serializes")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip() {
        let c = Config { token: "ABCD-2345".into(), ..Config::default() };
        assert_eq!(Config::from_toml(&c.to_toml()).unwrap(), c);
    }

    #[test]
    fn missing_fields_use_defaults() {
        let c = Config::from_toml("token = \"ABCD-2345\"").unwrap();
        assert_eq!(c.url, DEFAULT_URL);
        assert_eq!(c.metrics_interval_ms, 2000);
        assert_eq!(c.token, "ABCD-2345");
    }

    #[test]
    fn garbage_is_an_error() {
        assert!(Config::from_toml("url = [").is_err());
    }
}
