#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// PC protocol (protocol/README.md): turns agent messages into bus events.
// Transport-agnostic: the board's WebSocket server and the simulator's both
// feed text frames in here.

#define PC_LINK_VERSION     1
#define PC_LINK_TOKEN_LEN   10      // "XXXX-XXXX" + NUL
#define PC_LINK_TIMEOUT_MS  45000   // no message for this long = disconnected

typedef enum {
    CLAUDE_SESSION_START,
    CLAUDE_PROMPT,
    CLAUDE_WAITING,
    CLAUDE_DONE,
    CLAUDE_SESSION_END,
} claude_event_t;

typedef enum {
    PC_LINK_OK,             // handled; send `reply` if non-empty
    PC_LINK_AUTHENTICATED,  // hello accepted; send `reply`, mark the connection
    PC_LINK_CLOSE,          // send `reply`, then close the connection
} pc_link_result_t;

// Handles one frame. `authed`: whether this connection already sent a valid
// hello. `reply` always receives a (possibly empty) JSON string.
pc_link_result_t pc_link_handle(const char *json, bool authed, const char *token, char *reply,
                                size_t reply_len);

// Connection bookkeeping shared by the transports: one authenticated agent at
// a time, PC_LINK_TIMEOUT_MS of silence = gone. Publishes EV_PC_CONNECTED.
// Connections are identified by the transport (socket fd).
typedef struct {
    char token[PC_LINK_TOKEN_LEN];
    int authed_conn;        // -1 if none
    uint32_t last_rx_ms;
} pc_session_t;

typedef struct {
    bool close_this;        // close the connection the frame came from
    int close_other;        // a previous agent to drop, or -1
} pc_session_action_t;

void pc_session_init(pc_session_t *s, const char *token);
pc_session_action_t pc_session_frame(pc_session_t *s, int conn, const char *json, uint32_t now_ms,
                                     char *reply, size_t reply_len);
void pc_session_closed(pc_session_t *s, int conn);
// Connection to close because it went silent, or -1.
int pc_session_expired(const pc_session_t *s, uint32_t now_ms);

// Latest metrics; NaN for fields the agent didn't send.
typedef struct {
    bool valid;
    float cpu;
    float ram;
    float gpu;
    float cpu_temp;
    float gpu_temp;
} pc_metrics_t;

void pc_metrics_get(pc_metrics_t *out);
void pc_metrics_clear(void);   // on disconnect

#define PC_HOT_C        80.0f
#define PC_BURNING_C    90.0f
#define PC_BUSY_CPU     85.0f

// Hottest of CPU/GPU in °C, NaN if unknown.
float pc_metrics_max_temp(const pc_metrics_t *m);
bool pc_metrics_hot(const pc_metrics_t *m);
bool pc_metrics_busy(const pc_metrics_t *m);

// Pairing token: loaded from storage, generated on first use.
void pc_link_token(char out[PC_LINK_TOKEN_LEN]);
