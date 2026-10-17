#pragma once

// Background task: fetches today's forecast from Open-Meteo every 15 min,
// stores it in weather_store and publishes EV_WEATHER. Strings must outlive
// the task (Kconfig values are fine).
void weather_fetch_start(const char *latitude, const char *longitude, const char *tz_url_encoded);
