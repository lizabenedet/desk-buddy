#pragma once

// HTTP server on port 80: the PC agent's WebSocket at /ws (protocol/README.md)
// and the settings/OTA page, plus the mDNS name. Call after net_start() and
// app_start() (settings must be registered).
void pc_server_start(void);
