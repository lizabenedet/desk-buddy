#pragma once

#include <stdbool.h>

#include "freertos/FreeRTOS.h"

// Wi-Fi station (through the ESP32-C6) + SNTP. Publishes EV_WIFI and
// EV_TIME_SYNCED. Sets the local timezone before anything else.
void net_start(const char *ssid, const char *password, const char *tz);

// Blocks until the station has an IP or `timeout` passes.
bool net_wait_connected(TickType_t timeout);
