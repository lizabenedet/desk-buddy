//! `buddy hook`: run by Claude Code for each hook. Reads the hook payload on
//! stdin and hands it to the running agent over localhost.
//!
//! Must never print or fail: hook output can end up in Claude's context
//! (costing tokens) and a non-zero exit can block Claude. So every error is
//! swallowed and the agent being down is simply ignored.

use std::io::{IsTerminal, Read, Write};
use std::net::{SocketAddr, TcpStream};
use std::time::Duration;

use serde::{Deserialize, Serialize};

use crate::config::HOOK_PORT;
use crate::protocol::{ClaudeEvent, HookInput, Outgoing};

const CONNECT_TIMEOUT: Duration = Duration::from_millis(300);
const MAX_INPUT: u64 = 1 << 20;

/// Hook process -> agent, one JSON line per connection.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct HookEvent {
    pub event: ClaudeEvent,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub project: Option<String>,
}

impl HookEvent {
    pub fn into_message(self) -> Outgoing {
        Outgoing::Claude { event: self.event, project: self.project }
    }
}

pub fn run() {
    let _ = try_run();
}

fn try_run() -> Option<()> {
    let stdin = std::io::stdin();
    if stdin.is_terminal() {
        return None; // run by hand: nothing to read
    }
    let mut input = String::new();
    stdin.lock().take(MAX_INPUT).read_to_string(&mut input).ok()?;
    let event = parse(&input)?;

    let addr = SocketAddr::from(([127, 0, 0, 1], HOOK_PORT));
    let mut stream = TcpStream::connect_timeout(&addr, CONNECT_TIMEOUT).ok()?;
    stream.set_write_timeout(Some(CONNECT_TIMEOUT)).ok()?;
    let mut line = serde_json::to_string(&event).ok()?;
    line.push('\n');
    stream.write_all(line.as_bytes()).ok()
}

fn parse(input: &str) -> Option<HookEvent> {
    // PowerShell with a UTF-8 $OutputEncoding pipes a BOM first.
    let hook: HookInput = serde_json::from_str(input.trim_start_matches('\u{feff}')).ok()?;
    match hook.to_message()? {
        Outgoing::Claude { event, project } => Some(HookEvent { event, project }),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_claude_payloads() {
        let ev = parse(r#"{"session_id":"s","hook_event_name":"Stop","cwd":"C:\\p\\desk-buddy"}"#);
        assert_eq!(ev, Some(HookEvent { event: ClaudeEvent::Done, project: Some("desk-buddy".into()) }));
        assert_eq!(parse(r#"{"hook_event_name":"PreToolUse"}"#), None);
        assert_eq!(parse("not json"), None);
    }

    #[test]
    fn tolerates_a_bom() {
        let ev = parse("\u{feff}{\"hook_event_name\":\"Stop\"}\r\n");
        assert_eq!(ev, Some(HookEvent { event: ClaudeEvent::Done, project: None }));
    }

    #[test]
    fn line_roundtrip() {
        let ev = HookEvent { event: ClaudeEvent::Waiting, project: None };
        let line = serde_json::to_string(&ev).unwrap();
        assert_eq!(line, r#"{"event":"waiting"}"#);
        assert_eq!(serde_json::from_str::<HookEvent>(&line).unwrap(), ev);
    }
}
