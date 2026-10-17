#include "weather.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
static portMUX_TYPE store_mux = portMUX_INITIALIZER_UNLOCKED;
#define STORE_LOCK()   portENTER_CRITICAL(&store_mux)
#define STORE_UNLOCK() portEXIT_CRITICAL(&store_mux)
#else
#define STORE_LOCK()
#define STORE_UNLOCK()
#endif

static weather_report_t store;

void weather_store_set(const weather_report_t *r)
{
    STORE_LOCK();
    store = *r;
    STORE_UNLOCK();
}

void weather_store_get(weather_report_t *out)
{
    STORE_LOCK();
    *out = store;
    STORE_UNLOCK();
}

bool weather_rain_window(const weather_report_t *r, int from_hour, int threshold, int *start, int *end)
{
    if (!r->valid || from_hour < 0) {
        return false;
    }
    int first = -1, last = -1;
    for (int h = from_hour; h < 24; h++) {
        if (r->hourly_rain[h] >= threshold) {
            if (first < 0) {
                first = h;
            }
            last = h;
        }
    }
    if (first < 0) {
        return false;
    }
    *start = first;
    *end = last;
    return true;
}

int weather_rain_max(const weather_report_t *r, int from_hour)
{
    int max = 0;
    for (int h = from_hour < 0 ? 0 : from_hour; h < 24; h++) {
        if (r->hourly_rain[h] > max) {
            max = r->hourly_rain[h];
        }
    }
    return max;
}

bool weather_should_warn(const weather_report_t *r, int now_hour, time_t now, time_t last_warned)
{
    int start, end;
    if (!weather_rain_window(r, now_hour, WEATHER_WARN_PROB, &start, &end)) {
        return false;
    }
    if (start - now_hour > WEATHER_WARN_AHEAD_H) {
        return false;
    }
    return last_warned == 0 || now - last_warned >= WEATHER_WARN_EVERY_S;
}

weather_kind_t weather_kind(int code)
{
    switch (code) {
    case 0: case 1:
        return WEATHER_CLEAR;
    case 2:
        return WEATHER_PARTLY;
    case 3:
        return WEATHER_CLOUDY;
    case 45: case 48:
        return WEATHER_FOG;
    case 51: case 53: case 55: case 56: case 57:
        return WEATHER_DRIZZLE;
    case 61: case 63: case 65: case 66: case 67: case 80: case 81: case 82:
        return WEATHER_RAIN;
    case 71: case 73: case 75: case 77: case 85: case 86:
        return WEATHER_SNOW;
    case 95: case 96: case 99:
        return WEATHER_THUNDER;
    default:
        return WEATHER_UNKNOWN;
    }
}

const char *weather_description(int code)
{
    switch (code) {
    case 0: return "céu limpo";
    case 1: return "quase limpo";
    case 2: return "parcialmente nublado";
    case 3: return "nublado";
    case 45: case 48: return "neblina";
    case 51: case 53: case 55: return "garoa";
    case 56: case 57: return "garoa congelante";
    case 61: return "chuva fraca";
    case 63: return "chuva";
    case 65: return "chuva forte";
    case 66: case 67: return "chuva congelante";
    case 71: case 73: case 75: case 77: return "neve";
    case 80: case 81: case 82: return "pancadas de chuva";
    case 85: case 86: return "pancadas de neve";
    case 95: return "tempestade";
    case 96: case 99: return "tempestade com granizo";
    default: return "sem dados";
    }
}
