#pragma once

#include "esp_http_server.h"

// buddy.local page: GET /, GET+POST /api/settings and POST /api/ota, all
// behind Basic auth with the pairing token.
void web_routes_register(httpd_handle_t server, const char *token);
