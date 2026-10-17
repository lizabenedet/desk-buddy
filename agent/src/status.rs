//! What the agent is doing right now, shown in the tray menu.

use std::sync::{Arc, Mutex};

use crate::metrics::SourceStatus;

#[derive(Debug, Clone, Default)]
pub struct Status {
    pub connected: bool,
    pub last_error: Option<String>,
    pub sources: Vec<SourceStatus>,
}

pub type SharedStatus = Arc<Mutex<Status>>;

impl Status {
    pub fn connection_line(&self) -> String {
        if self.connected {
            "Buddy: conectado".to_string()
        } else {
            match &self.last_error {
                Some(e) => format!("Buddy: desconectado ({e})"),
                None => "Buddy: conectando...".to_string(),
            }
        }
    }

    pub fn source_lines(&self) -> Vec<String> {
        self.sources
            .iter()
            .map(|s| match &s.error {
                None => format!("{}: ok", s.name),
                Some(e) => format!("{}: {e}", s.name),
            })
            .collect()
    }
}
