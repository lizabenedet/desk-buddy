#pragma once

#include <stdint.h>

// Minimal WebSocket server for the simulator (Winsock, localhost only):
// text frames, no fragmentation. Feeds pc_session like the board does.
void ws_server_start(uint16_t port);

// Non-blocking; call from the main loop.
void ws_server_poll(void);
