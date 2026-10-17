#include "home.h"

#include <stdio.h>

#include "event_bus.h"
#include "face.h"
#include "face_view.h"
#include "icons.h"
#include "pomodoro.h"
#include "ui.h"
#include "weather.h"

#define TAP_REACT_MS    1200
#define HOLD_REACT_MS   2500
#define ALERT_REACT_MS  (10 * 60 * 1000)
#define ALERT_FLASH_MS  500
#define BUBBLE_MAX_W    400
#define STATUS_H        30

static face_t face;
static lv_obj_t *bubble;
static lv_timer_t *bubble_timer;

static lv_obj_t *status_time;
static lv_obj_t *status_timer_icon;
static lv_obj_t *status_timer;
static lv_obj_t *status_weather_icon;
static lv_obj_t *status_temp;
static lv_obj_t *status_wifi;
static int shown_minute = -1;

static lv_obj_t *alert_overlay;
static lv_timer_t *alert_flash;

/* ---------- speech bubble ---------- */

static void hide_bubble(lv_timer_t *t)
{
    lv_timer_pause(t);
    lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
}

// duration_ms = 0 keeps it up until hidden explicitly.
static void say(const char *text, uint32_t duration_ms)
{
    lv_label_set_text(bubble, text);
    lv_obj_remove_flag(bubble, LV_OBJ_FLAG_HIDDEN);
    if (duration_ms) {
        lv_timer_set_period(bubble_timer, duration_ms);
        lv_timer_reset(bubble_timer);
        lv_timer_resume(bubble_timer);
    } else {
        lv_timer_pause(bubble_timer);
    }
}

static lv_obj_t *create_bubble(lv_obj_t *parent)
{
    lv_obj_t *b = lv_label_create(parent);
    lv_obj_set_style_text_font(b, &font_pt_20, 0);
    lv_obj_set_style_text_color(b, lv_color_black(), 0);
    lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(b, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_pad_hor(b, 14, 0);
    lv_obj_set_style_pad_ver(b, 8, 0);
    lv_obj_set_style_max_width(b, BUBBLE_MAX_W, 0);
    lv_obj_set_width(b, LV_SIZE_CONTENT);
    lv_label_set_long_mode(b, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, STATUS_H + 6);
    lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
    return b;
}

/* ---------- alert: flashing border until the screen is touched ---------- */

static void flash_cb(lv_timer_t *t)
{
    (void)t;
    lv_opa_t opa = lv_obj_get_style_border_opa(alert_overlay, 0);
    lv_obj_set_style_border_opa(alert_overlay, opa ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
}

static void dismiss_alert(lv_event_t *e)
{
    (void)e;
    lv_timer_delete(alert_flash);
    lv_obj_delete(alert_overlay);
    alert_overlay = NULL;
    lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
    face_react(&face, MOOD_HAPPY, TAP_REACT_MS);
    event_bus_publish_simple(EV_ALERT_DISMISSED, 0);
}

static void show_alert(const char *text)
{
    say(text, 0);
    face_react(&face, MOOD_ALERT, ALERT_REACT_MS);
    if (alert_overlay) {
        return;
    }
    // Lives on the top layer so it catches touches on every screen.
    alert_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(alert_overlay);
    lv_obj_set_size(alert_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(alert_overlay, 8, 0);
    lv_obj_set_style_border_color(alert_overlay, lv_color_hex(face_preset(MOOD_ALERT)->color), 0);
    lv_obj_set_style_border_opa(alert_overlay, LV_OPA_COVER, 0);
    lv_obj_add_flag(alert_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(alert_overlay, dismiss_alert, LV_EVENT_CLICKED, NULL);
    alert_flash = lv_timer_create(flash_cb, ALERT_FLASH_MS, NULL);
}

/* ---------- status bar ---------- */

static lv_obj_t *status_label(lv_obj_t *parent, const lv_font_t *font)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(UI_TEXT_DIM), 0);
    lv_label_set_text(l, "");
    return l;
}

static void refresh_time(lv_timer_t *t)
{
    (void)t;
    struct tm tm;
    if (!ui_local_time(&tm)) {
        lv_label_set_text(status_time, "--:--");
        return;
    }
    if (tm.tm_min != shown_minute) {   // avoid redrawing every second
        shown_minute = tm.tm_min;
        lv_label_set_text_fmt(status_time, "%02d:%02d", tm.tm_hour, tm.tm_min);
    }
}

static void refresh_weather(void)
{
    weather_report_t r;
    weather_store_get(&r);
    if (!r.valid) {
        return;
    }
    struct tm tm;
    bool night = ui_local_time(&tm) && (tm.tm_hour >= 18 || tm.tm_hour < 6);
    lv_label_set_text(status_weather_icon, ui_weather_icon(r.code, night));
    lv_label_set_text_fmt(status_temp, "%d°", (int)(r.temp + (r.temp < 0 ? -0.5f : 0.5f)));
}

static void refresh_timer(int32_t seconds_left, pomodoro_phase_t phase)
{
    bool running = phase != POMO_IDLE;
    lv_obj_set_flag(status_timer_icon, LV_OBJ_FLAG_HIDDEN, !running);
    lv_obj_set_flag(status_timer, LV_OBJ_FLAG_HIDDEN, !running);
    if (running) {
        lv_label_set_text_fmt(status_timer, "%02d:%02d", (int)(seconds_left / 60), (int)(seconds_left % 60));
    }
}

static void create_status_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), STATUS_H);
    lv_obj_set_style_pad_hor(bar, 10, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, 6, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);

    status_time = status_label(bar, &font_pt_20);
    lv_obj_set_flex_grow(status_time, 1);
    status_timer_icon = status_label(bar, &icons_20);
    lv_label_set_text(status_timer_icon, ICON_TIMER);
    status_timer = status_label(bar, &font_pt_20);
    refresh_timer(0, POMO_IDLE);
    status_weather_icon = status_label(bar, &icons_20);
    status_temp = status_label(bar, &font_pt_20);
    status_wifi = status_label(bar, &icons_20);
    lv_label_set_text(status_wifi, ICON_WIFI_OFF);

    refresh_time(NULL);
    lv_timer_create(refresh_time, 1000, NULL);
}

/* ---------- events / touch ---------- */

static void on_event(const event_t *ev, void *ctx)
{
    (void)ctx;
    switch (ev->id) {
    case EV_FACE_MOOD:
        face_set_mood(&face, (mood_t)ev->value);
        break;
    case EV_FACE_REACT:
        face_react(&face, (mood_t)ev->value, (uint32_t)ev->arg);
        break;
    case EV_SAY:
        if (!alert_overlay) {  // don't cover an alert's message
            say(ev->text, (uint32_t)ev->arg);
        }
        break;
    case EV_ALERT:
        show_alert(ev->text);
        break;
    case EV_FACE_TOUCHED:
        // Poke = startled, petting = happy. The pet module also listens.
        face_react(&face, ev->value == TOUCH_HOLD ? MOOD_HAPPY : MOOD_SURPRISED,
                   ev->value == TOUCH_HOLD ? HOLD_REACT_MS : TAP_REACT_MS);
        break;
    case EV_WIFI:
        lv_label_set_text(status_wifi, ev->value ? ICON_WIFI : ICON_WIFI_OFF);
        break;
    case EV_TIME_SYNCED:
        shown_minute = -1;
        refresh_time(NULL);
        break;
    case EV_WEATHER:
        refresh_weather();
        break;
    case EV_TIMER:
        refresh_timer(ev->value, (pomodoro_phase_t)ev->arg);
        break;
    default:
        break;
    }
}

static void on_touch(lv_event_t *e)
{
    touch_kind_t kind = lv_event_get_code(e) == LV_EVENT_LONG_PRESSED ? TOUCH_HOLD : TOUCH_TAP;
    event_bus_publish_simple(EV_FACE_TOUCHED, kind);
}

void home_create(lv_obj_t *parent)
{
    face_init(&face, lv_tick_get() | 1);

    lv_obj_t *view = face_view_create(parent, &face);
    lv_obj_add_flag(view, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(view, on_touch, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(view, on_touch, LV_EVENT_LONG_PRESSED, NULL);

    create_status_bar(parent);
    bubble = create_bubble(parent);
    bubble_timer = lv_timer_create(hide_bubble, 1000, NULL);
    lv_timer_pause(bubble_timer);

    event_bus_subscribe(EV_ANY, on_event, NULL);
}
