// One daily alarm, set with +/- buttons, persisted.

#include "module.h"
#include "storage.h"
#include "ui.h"

LV_FONT_DECLARE(font_pt_20);

#define STORAGE_KEY "alarm"

typedef struct {
    uint8_t hour;
    uint8_t minute;
    bool enabled;
} alarm_cfg_t;

static alarm_cfg_t cfg = {.hour = 7, .minute = 0, .enabled = false};
static int fired_yday = -1;

static lv_obj_t *time_label;
static lv_obj_t *toggle;

static void refresh(void)
{
    lv_label_set_text_fmt(time_label, "%02u:%02u", cfg.hour, cfg.minute);
    lv_obj_set_style_text_color(time_label, lv_color_hex(cfg.enabled ? 0xFFFFFF : UI_TEXT_DIM), 0);
}

static void save(void)
{
    storage_save(STORAGE_KEY, &cfg, sizeof(cfg));
}

static void on_adjust(lv_event_t *e)
{
    int delta_min = (int)(intptr_t)lv_event_get_user_data(e);
    int total = (cfg.hour * 60 + cfg.minute + delta_min + 24 * 60) % (24 * 60);
    cfg.hour = (uint8_t)(total / 60);
    cfg.minute = (uint8_t)(total % 60);
    fired_yday = -1;
    refresh();
    save();
}

static void on_toggle(lv_event_t *e)
{
    cfg.enabled = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    refresh();
    save();
}

static void check(lv_timer_t *t)
{
    (void)t;
    struct tm tm;
    if (!cfg.enabled || !ui_local_time(&tm) || fired_yday == tm.tm_yday) {
        return;
    }
    if (tm.tm_hour == cfg.hour && tm.tm_min == cfg.minute) {
        fired_yday = tm.tm_yday;
        char text[EVENT_TEXT_MAX];
        lv_snprintf(text, sizeof(text), "Alarme! São %02u:%02u.", cfg.hour, cfg.minute);
        event_bus_publish_text(EV_ALERT, text, 0);
    }
}

static void init(void)
{
    alarm_cfg_t stored;
    if (storage_load(STORAGE_KEY, &stored, sizeof(stored)) && stored.hour < 24 && stored.minute < 60) {
        cfg = stored;
    }
    lv_timer_create(check, 1000, NULL);
}

static void create_screen(lv_obj_t *parent)
{
    ui_title(parent, "Alarme");

    time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(time_label, &font_digits_64, 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 150);
    ui_button(row, "-1h", on_adjust, (void *)(intptr_t)-60);
    ui_button(row, "+1h", on_adjust, (void *)(intptr_t)60);
    ui_button(row, "-10", on_adjust, (void *)(intptr_t)-10);
    ui_button(row, "+10", on_adjust, (void *)(intptr_t)10);

    toggle = lv_switch_create(parent);
    lv_obj_set_size(toggle, 90, 46);
    lv_obj_align(toggle, LV_ALIGN_BOTTOM_MID, 40, -24);
    lv_obj_set_state(toggle, LV_STATE_CHECKED, cfg.enabled);
    lv_obj_add_event_cb(toggle, on_toggle, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "Ligado");
    lv_obj_set_style_text_font(label, &font_pt_20, 0);
    lv_obj_align_to(label, toggle, LV_ALIGN_OUT_LEFT_MID, -14, 0);

    refresh();
}

const module_t alarm_module = {
    .name = "alarm",
    .init = init,
    .create_screen = create_screen,
};
