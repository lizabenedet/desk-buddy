// Today's weather screen, the morning summary and the "rain is coming"
// warning. Data comes from weather_store (filled by the board's fetcher).

#include <stdio.h>
#include <string.h>

#include "icons.h"
#include "module.h"
#include "pet.h"
#include "ui.h"
#include "weather.h"

#define CHECK_PERIOD_MS     60000
#define GREETING_UNTIL_HOUR 11   // greet from wake-up time until this hour
#define GREETING_MS         60000
#define WARNING_MS          20000

static lv_obj_t *icon;
static lv_obj_t *temp;
static lv_obj_t *desc;
static lv_obj_t *range;
static lv_obj_t *rain;
static lv_obj_t *updated;

static int greeted_yday = -1;
static time_t last_warned;

static int round_temp(float t)
{
    return (int)(t + (t < 0 ? -0.5f : 0.5f));
}

// "das 14h às 18h", "às 14h" or "" when no rain is likely.
static void rain_phrase(const weather_report_t *r, int from_hour, char *out, size_t len)
{
    int start, end;
    if (!weather_rain_window(r, from_hour, WEATHER_RAIN_LIKELY, &start, &end)) {
        out[0] = '\0';
    } else if (start == end) {
        snprintf(out, len, "às %dh", start);
    } else {
        snprintf(out, len, "das %dh às %dh", start, end + 1);
    }
}

static void refresh_screen(void)
{
    weather_report_t r;
    weather_store_get(&r);
    if (!r.valid) {
        lv_label_set_text(icon, ICON_NO_DATA);
        lv_label_set_text(temp, "--");
        lv_label_set_text(desc, "sem dados ainda");
        return;
    }
    struct tm tm;
    bool have_time = ui_local_time(&tm);
    bool night = have_time && (tm.tm_hour >= 18 || tm.tm_hour < 6);

    lv_label_set_text(icon, ui_weather_icon(r.code, night));
    lv_label_set_text_fmt(temp, "%d°", round_temp(r.temp));
    lv_label_set_text(desc, weather_description(r.code));
    // Rain chance for what's left of the day; the daily max may be from hours already gone.
    lv_label_set_text_fmt(range, "mín %d°   máx %d°   chuva %d%%", round_temp(r.temp_min),
                          round_temp(r.temp_max), weather_rain_max(&r, have_time ? tm.tm_hour : 0));

    char phrase[32];
    rain_phrase(&r, have_time ? tm.tm_hour : 0, phrase, sizeof(phrase));
    if (phrase[0]) {
        lv_label_set_text_fmt(rain, "Chuva provável %s", phrase);
    } else {
        lv_label_set_text(rain, "Sem chuva prevista para hoje");
    }

    struct tm up;
#ifdef _WIN32
    localtime_s(&up, &r.updated);
#else
    localtime_r(&r.updated, &up);
#endif
    lv_label_set_text_fmt(updated, "atualizado às %02d:%02d", up.tm_hour, up.tm_min);
}

static void maybe_talk(lv_timer_t *t)
{
    (void)t;
    weather_report_t r;
    weather_store_get(&r);
    struct tm tm;
    if (!r.valid || !ui_local_time(&tm) || pet_is_sleep_time(tm.tm_hour)) {
        return;
    }
    char phrase[32];
    char text[EVENT_TEXT_MAX];

    if (greeted_yday != tm.tm_yday && tm.tm_hour >= pet_wake_hour() && tm.tm_hour < GREETING_UNTIL_HOUR) {
        greeted_yday = tm.tm_yday;
        rain_phrase(&r, tm.tm_hour, phrase, sizeof(phrase));
        snprintf(text, sizeof(text), "Bom dia! Hoje %d°–%d°, %s%s.", round_temp(r.temp_min),
                 round_temp(r.temp_max), phrase[0] ? "chuva " : "sem chuva", phrase);
        event_bus_publish_text(EV_SAY, text, GREETING_MS);
        last_warned = time(NULL); // the greeting already mentions the rain
        return;
    }

    time_t now = time(NULL);
    if (weather_should_warn(&r, tm.tm_hour, now, last_warned)) {
        last_warned = now;
        rain_phrase(&r, tm.tm_hour, phrase, sizeof(phrase));
        snprintf(text, sizeof(text), "Vai chover %s, leva o guarda-chuva!", phrase);
        event_bus_publish_text(EV_SAY, text, WARNING_MS);
        event_t react = {.id = EV_FACE_REACT, .value = MOOD_SURPRISED, .arg = 2000};
        event_bus_publish(&react);
    }
}

static void on_event(const event_t *ev)
{
    if (ev->id == EV_WEATHER) {
        refresh_screen();
        maybe_talk(NULL);
    }
}

static lv_obj_t *text_label(lv_obj_t *parent, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, &font_pt_20, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, "");
    return l;
}

static void create_screen(lv_obj_t *parent)
{
    ui_title(parent, "Clima hoje");

    icon = lv_label_create(parent);
    lv_obj_set_style_text_font(icon, &icons_64, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, -90, 56);

    temp = lv_label_create(parent);
    lv_obj_set_style_text_font(temp, &font_digits_64, 0);
    lv_obj_align(temp, LV_ALIGN_TOP_MID, 50, 52);

    desc = text_label(parent, 0xFFFFFF);
    lv_obj_align(desc, LV_ALIGN_TOP_MID, 0, 140);
    range = text_label(parent, UI_TEXT_DIM);
    lv_obj_align(range, LV_ALIGN_TOP_MID, 0, 176);
    rain = text_label(parent, 0x7FC8FF);
    lv_obj_align(rain, LV_ALIGN_TOP_MID, 0, 214);
    updated = text_label(parent, 0x5C6670);
    lv_obj_align(updated, LV_ALIGN_BOTTOM_MID, 0, -12);

    refresh_screen();
    lv_timer_create(maybe_talk, CHECK_PERIOD_MS, NULL);
}

// Read by the board's fetcher (Jaraguá do Sul by default).
static const setting_def_t settings[] = {
    {"weather_lat", "Clima", "Latitude", SETTING_TEXT, 0, 0, 0, "-26.4851"},
    {"weather_lon", "Clima", "Longitude", SETTING_TEXT, 0, 0, 0, "-49.0713"},
};

const module_t weather_module = {
    .name = "weather",
    .on_event = on_event,
    .create_screen = create_screen,
    .settings = settings,
    .settings_count = sizeof(settings) / sizeof(settings[0]),
};
