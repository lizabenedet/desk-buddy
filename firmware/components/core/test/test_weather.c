#include <string.h>

#include "check.h"
#include "weather.h"

static weather_report_t rainy_afternoon(void)
{
    weather_report_t r = {.valid = true};
    r.hourly_rain[14] = 60;
    r.hourly_rain[15] = 80;
    r.hourly_rain[16] = 30;   // a dry-ish hour inside the stretch
    r.hourly_rain[17] = 70;
    return r;
}

static void test_codes(void)
{
    CHECK(weather_kind(0) == WEATHER_CLEAR);
    CHECK(weather_kind(2) == WEATHER_PARTLY);
    CHECK(weather_kind(63) == WEATHER_RAIN && weather_kind(81) == WEATHER_RAIN);
    CHECK(weather_kind(95) == WEATHER_THUNDER);
    CHECK(weather_kind(-1) == WEATHER_UNKNOWN && weather_kind(4) == WEATHER_UNKNOWN);
    CHECK(strcmp(weather_description(3), "nublado") == 0);
    CHECK(strcmp(weather_description(1234), "sem dados") == 0);
}

static void test_rain_window(void)
{
    weather_report_t r = rainy_afternoon();
    int start, end;
    CHECK(weather_rain_window(&r, 7, WEATHER_RAIN_LIKELY, &start, &end));
    CHECK(start == 14 && end == 17);
    CHECK(weather_rain_window(&r, 16, WEATHER_RAIN_LIKELY, &start, &end));
    CHECK(start == 17 && end == 17);
    CHECK(!weather_rain_window(&r, 18, WEATHER_RAIN_LIKELY, &start, &end));
    CHECK(!weather_rain_window(&r, -1, WEATHER_RAIN_LIKELY, &start, &end));
    r.valid = false;
    CHECK(!weather_rain_window(&r, 7, WEATHER_RAIN_LIKELY, &start, &end));
}

static void test_should_warn(void)
{
    weather_report_t r = rainy_afternoon();
    CHECK(weather_should_warn(&r, 12, 10000, 0));               // 2 h ahead
    CHECK(!weather_should_warn(&r, 9, 10000, 0));               // 5 h ahead: too early
    CHECK(!weather_should_warn(&r, 12, 10000, 10000 - 60));     // warned a minute ago
    CHECK(weather_should_warn(&r, 12, 20000, 20000 - 3 * 3600));
    CHECK(!weather_should_warn(&r, 18, 10000, 0));              // over for today
    r.hourly_rain[14] = r.hourly_rain[15] = r.hourly_rain[17] = 55;
    CHECK(!weather_should_warn(&r, 12, 10000, 0));              // not likely enough
}

static void test_store_roundtrip(void)
{
    weather_report_t in = rainy_afternoon();
    in.temp = 21.5f;
    in.code = 63;
    weather_store_set(&in);
    weather_report_t out;
    weather_store_get(&out);
    CHECK(out.valid && out.code == 63 && out.hourly_rain[15] == 80);
}

int main(void)
{
    test_codes();
    test_rain_window();
    test_should_warn();
    test_store_roundtrip();
    puts("weather: all tests passed");
    return 0;
}
