#pragma once

#include <stdbool.h>

// Shared bits of the buddy.local page, served by the board and the simulator.

// HTTP Basic auth: user "buddy", password = the pairing token.
// `authorization` is the raw header value (may be NULL).
#define WEB_AUTH_USER "buddy"
#define WEB_AUTH_REALM "desk-buddy"

bool web_auth_ok(const char *authorization, const char *token);

// GET /api/settings body: settings_to_json() plus "version" and "ota".
// free() the result.
char *web_settings_body(const char *version, bool ota);

// POST /api/settings: applies `body` and returns {"ok":true} or
// {"ok":false,"error":"..."}; `ok` tells the transport which status to send.
// free() the result.
char *web_apply_settings(const char *body, bool *ok);
