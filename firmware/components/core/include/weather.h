#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// WMO weather interpretation codes, as returned by Open-Meteo.

typedef enum {
    WEATHER_CLEAR,
    WEATHER_PARTLY,
    WEATHER_CLOUDY,
    WEATHER_FOG,
    WEATHER_DRIZZLE,
    WEATHER_RAIN,
    WEATHER_SNOW,
    WEATHER_THUNDER,
    WEATHER_UNKNOWN,
} weather_kind_t;

weather_kind_t weather_kind(int wmo_code);
const char *weather_description(int wmo_code);   // Portuguese

#define WEATHER_RAIN_LIKELY   50   // % to call it "likely rain"
#define WEATHER_WARN_PROB     60
#define WEATHER_WARN_AHEAD_H  3
#define WEATHER_WARN_EVERY_S  (3 * 3600)

// Today only.
typedef struct {
    bool valid;
    time_t updated;
    float temp;             // now
    int code;               // now
    int day_code;
    float temp_min;
    float temp_max;
    int day_rain_prob;      // %, max over the day
    int8_t hourly_rain[24]; // % per local hour of today
} weather_report_t;

// Likely-rain stretch from `from_hour` to the end of the day: first and last
// hour with probability >= threshold. Returns false if none.
bool weather_rain_window(const weather_report_t *r, int from_hour, int threshold, int *start, int *end);

// Highest hourly rain probability from `from_hour` to the end of the day.
int weather_rain_max(const weather_report_t *r, int from_hour);

// Fills `out` from an Open-Meteo /v1/forecast response requested with
// current=temperature_2m,weather_code, hourly=precipitation_probability,
// daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max
// and forecast_days=1. Returns false if a required field is missing.
bool weather_parse_open_meteo(const char *json, time_t now, weather_report_t *out);

// Whether to tell the user rain is coming: likely rain within the next
// WEATHER_WARN_AHEAD_H hours, and no warning in the last WEATHER_WARN_EVERY_S.
bool weather_should_warn(const weather_report_t *r, int now_hour, time_t now, time_t last_warned);

// Shared latest report: written by the fetcher task, read on the UI task.
void weather_store_set(const weather_report_t *r);
void weather_store_get(weather_report_t *out);
