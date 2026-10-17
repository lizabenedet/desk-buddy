// Parses a real Open-Meteo response (Jaraguá do Sul, 2026-10-17) saved in
// open_meteo_sample.json.

#include <string.h>

#include "check.h"
#include "weather.h"

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    CHECK(f);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    CHECK(buf && fread(buf, 1, (size_t)len, f) == (size_t)len);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

static void test_real_response(void)
{
    char *json = read_file(FIXTURE_DIR "/open_meteo_sample.json");
    weather_report_t r;
    CHECK(weather_parse_open_meteo(json, 1234, &r));
    free(json);

    CHECK(r.valid && r.updated == 1234);
    CHECK(NEAR(r.temp, 27.5f) && r.code == 3);
    CHECK(r.day_code == 51 && NEAR(r.temp_min, 15.9f) && NEAR(r.temp_max, 27.6f));
    CHECK(r.day_rain_prob == 84);
    CHECK(r.hourly_rain[0] == 84 && r.hourly_rain[3] == 55 && r.hourly_rain[23] == 17);

    // The 84% was overnight: from the afternoon on, it's a dry day.
    int start, end;
    CHECK(weather_rain_max(&r, 15) == 17);
    CHECK(!weather_rain_window(&r, 15, WEATHER_RAIN_LIKELY, &start, &end));
    CHECK(weather_rain_window(&r, 0, WEATHER_RAIN_LIKELY, &start, &end) && start == 0 && end == 3);
}

static void test_rejects_bad_input(void)
{
    weather_report_t r = {.valid = true, .code = 99};
    CHECK(!weather_parse_open_meteo("not json", 0, &r));
    CHECK(!weather_parse_open_meteo("{\"current\":{}}", 0, &r));
    CHECK(r.valid && r.code == 99); // untouched on failure
}

static void test_null_hours_are_skipped(void)
{
    const char *json =
        "{\"current\":{\"temperature_2m\":20,\"weather_code\":0},"
        "\"daily\":{\"weather_code\":[0],\"temperature_2m_max\":[25],\"temperature_2m_min\":[15]},"
        "\"hourly\":{\"time\":[\"2026-10-17T10:00\",\"2026-10-17T11:00\"],"
        "\"precipitation_probability\":[null,60]}}";
    weather_report_t r;
    CHECK(weather_parse_open_meteo(json, 0, &r));
    CHECK(r.hourly_rain[10] == 0 && r.hourly_rain[11] == 60 && r.day_rain_prob == 0);
}

int main(void)
{
    test_real_response();
    test_rejects_bad_input();
    test_null_hours_are_skipped();
    puts("weather_parse: all tests passed");
    return 0;
}
