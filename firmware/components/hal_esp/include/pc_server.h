#pragma once

// WebSocket server for the PC agent at ws://buddy.local/ws (protocol/README.md),
// plus the mDNS name. Call after net_start().
void pc_server_start(void);
