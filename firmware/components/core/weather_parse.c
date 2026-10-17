#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "weather.h"

static bool get_number(const cJSON *obj, const char *key, double *out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    *out = item->valuedouble;
    return true;
}

// First element of a daily array, e.g. daily.temperature_2m_max[0].
static bool get_first(const cJSON *obj, const char *key, double *out)
{
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(obj, key);
    const cJSON *item = cJSON_IsArray(arr) ? cJSON_GetArrayItem(arr, 0) : NULL;
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    *out = item->valuedouble;
    return true;
}

// "2026-10-17T15:00" -> 15
static int hour_of(const char *iso)
{
    if (!iso || strlen(iso) < 13 || iso[10] != 'T') {
        return -1;
    }
    int h = atoi(iso + 11);
    return h >= 0 && h < 24 ? h : -1;
}

bool weather_parse_open_meteo(const char *json, time_t now, weather_report_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }
    weather_report_t r = {.updated = now};
    bool ok = false;
    double v;

    const cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    const cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    const cJSON *hourly = cJSON_GetObjectItemCaseSensitive(root, "hourly");
    if (!current || !daily || !hourly) {
        goto done;
    }

    if (!get_number(current, "temperature_2m", &v)) goto done;
    r.temp = (float)v;
    if (!get_number(current, "weather_code", &v)) goto done;
    r.code = (int)v;
    if (!get_first(daily, "weather_code", &v)) goto done;
    r.day_code = (int)v;
    if (!get_first(daily, "temperature_2m_min", &v)) goto done;
    r.temp_min = (float)v;
    if (!get_first(daily, "temperature_2m_max", &v)) goto done;
    r.temp_max = (float)v;
    if (get_first(daily, "precipitation_probability_max", &v)) {
        r.day_rain_prob = (int)v;
    }

    // Hourly values are optional per hour (null when unknown).
    const cJSON *times = cJSON_GetObjectItemCaseSensitive(hourly, "time");
    const cJSON *probs = cJSON_GetObjectItemCaseSensitive(hourly, "precipitation_probability");
    int n = cJSON_IsArray(times) && cJSON_IsArray(probs) ? cJSON_GetArraySize(times) : 0;
    for (int i = 0; i < n; i++) {
        int h = hour_of(cJSON_GetStringValue(cJSON_GetArrayItem(times, i)));
        const cJSON *p = cJSON_GetArrayItem(probs, i);
        if (h >= 0 && cJSON_IsNumber(p)) {
            r.hourly_rain[h] = (int8_t)p->valueint;
        }
    }

    r.valid = true;
    *out = r;
    ok = true;
done:
    cJSON_Delete(root);
    return ok;
}
