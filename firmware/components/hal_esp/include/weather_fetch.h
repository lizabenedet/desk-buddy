#pragma once

// Background task: fetches today's forecast from Open-Meteo every 15 min for
// the location in the weather_lat/weather_lon settings (refetching right away
// when they change), stores it in weather_store and publishes EV_WEATHER.
void weather_fetch_start(void);
