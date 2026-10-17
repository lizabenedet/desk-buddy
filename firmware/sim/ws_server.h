#pragma once

#include <stdint.h>

// Minimal WebSocket + HTTP server for the simulator (Winsock, localhost only):
// the agent's /ws (text frames, no fragmentation) and the settings page.
void ws_server_start(uint16_t port);

// Non-blocking; call from the main loop.
void ws_server_poll(void);
