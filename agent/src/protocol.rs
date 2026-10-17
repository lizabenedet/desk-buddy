//! Messages exchanged with the buddy (see protocol/README.md) and the
//! mapping from Claude Code hook payloads to them.

use serde::{Deserialize, Serialize};

pub const VERSION: u32 = 1;

/// Agent -> buddy.
#[derive(Debug, Clone, PartialEq, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum Outgoing {
    Hello { version: u32, token: String },
    Claude {
        event: ClaudeEvent,
        #[serde(skip_serializing_if = "Option::is_none")]
        project: Option<String>,
    },
    Metrics(Metrics),
    Ping,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ClaudeEvent {
    SessionStart,
    Prompt,
    Waiting,
    Done,
    SessionEnd,
}

/// PC readings; `None` fields are left out of the JSON.
#[derive(Debug, Clone, Default, PartialEq, Serialize)]
pub struct Metrics {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cpu: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ram: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub gpu: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cpu_temp: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub gpu_temp: Option<f32>,
}

/// Buddy -> agent.
#[derive(Debug, Clone, PartialEq, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum Incoming {
    Welcome { version: u32 },
    Error { reason: String },
}

impl Outgoing {
    pub fn to_json(&self) -> String {
        serde_json::to_string(self).expect("messages always serialize")
    }
}

/// The fields we use from the JSON Claude Code writes to a hook's stdin.
#[derive(Debug, Deserialize)]
pub struct HookInput {
    pub hook_event_name: String,
    #[serde(default)]
    pub cwd: Option<String>,
}

impl HookInput {
    /// `None` for hooks the buddy doesn't react to.
    pub fn to_message(&self) -> Option<Outgoing> {
        let event = match self.hook_event_name.as_str() {
            "SessionStart" => ClaudeEvent::SessionStart,
            "UserPromptSubmit" => ClaudeEvent::Prompt,
            "Notification" => ClaudeEvent::Waiting,
            "Stop" => ClaudeEvent::Done,
            "SessionEnd" => ClaudeEvent::SessionEnd,
            _ => return None,
        };
        Some(Outgoing::Claude { event, project: self.project() })
    }

    /// Last component of the working directory, e.g. "desk-buddy".
    fn project(&self) -> Option<String> {
        let cwd = self.cwd.as_deref()?.trim_end_matches(['/', '\\']);
        let name = cwd.rsplit(['/', '\\']).next()?;
        (!name.is_empty()).then(|| name.to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn serializes_like_the_spec() {
        let hello = Outgoing::Hello { version: 1, token: "ABCD-2345".into() };
        assert_eq!(hello.to_json(), r#"{"type":"hello","version":1,"token":"ABCD-2345"}"#);
        assert_eq!(Outgoing::Ping.to_json(), r#"{"type":"ping"}"#);

        let m = Outgoing::Metrics(Metrics { cpu: Some(37.5), gpu_temp: Some(48.0), ..Default::default() });
        assert_eq!(m.to_json(), r#"{"type":"metrics","cpu":37.5,"gpu_temp":48.0}"#);

        let c = Outgoing::Claude { event: ClaudeEvent::SessionStart, project: None };
        assert_eq!(c.to_json(), r#"{"type":"claude","event":"session_start"}"#);
    }

    #[test]
    fn parses_replies() {
        let w: Incoming = serde_json::from_str(r#"{"type":"welcome","version":1}"#).unwrap();
        assert_eq!(w, Incoming::Welcome { version: 1 });
        let e: Incoming = serde_json::from_str(r#"{"type":"error","reason":"bad_token"}"#).unwrap();
        assert_eq!(e, Incoming::Error { reason: "bad_token".into() });
    }

    fn hook(json: &str) -> Option<Outgoing> {
        serde_json::from_str::<HookInput>(json).unwrap().to_message()
    }

    #[test]
    fn maps_hooks_with_project_from_cwd() {
        let msg = hook(r#"{"hook_event_name":"Notification","cwd":"C:\\Users\\me\\Projects\\desk-buddy","session_id":"x"}"#);
        assert_eq!(
            msg,
            Some(Outgoing::Claude { event: ClaudeEvent::Waiting, project: Some("desk-buddy".into()) })
        );
        let msg = hook(r#"{"hook_event_name":"Stop","cwd":"/home/me/proj/"}"#);
        assert_eq!(msg, Some(Outgoing::Claude { event: ClaudeEvent::Done, project: Some("proj".into()) }));
    }

    #[test]
    fn ignores_other_hooks_and_missing_cwd() {
        assert_eq!(hook(r#"{"hook_event_name":"PreToolUse","cwd":"/x"}"#), None);
        let msg = hook(r#"{"hook_event_name":"UserPromptSubmit"}"#);
        assert_eq!(msg, Some(Outgoing::Claude { event: ClaudeEvent::Prompt, project: None }));
    }
}
