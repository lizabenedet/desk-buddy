// Pomodoro: 25 min focus / 5 min break, repeating until stopped.

#include "module.h"
#include "pomodoro.h"
#include "ui.h"

LV_FONT_DECLARE(font_pt_20);

#define TICK_MS     250
#define MIN_MS      (60 * 1000)

static pomodoro_t pomo;
static uint32_t last_tick;
static uint32_t shown_seconds = UINT32_MAX;

static lv_obj_t *phase_label;
static lv_obj_t *time_label;
static lv_obj_t *button_label;

static void publish_state(void)
{
    event_t ev = {.id = EV_TIMER, .value = (int32_t)pomodoro_seconds_left(&pomo), .arg = pomo.phase};
    event_bus_publish(&ev);
}

static void refresh(void)
{
    static const char *const names[] = {"Pomodoro", "Foco", "Pausa"};
    uint32_t s = pomo.phase == POMO_IDLE ? pomo.focus_ms / 1000 : pomodoro_seconds_left(&pomo);
    lv_label_set_text(phase_label, names[pomo.phase]);
    lv_label_set_text_fmt(time_label, "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
    lv_label_set_text(button_label, pomo.phase == POMO_IDLE ? "Iniciar" : "Parar");
}

static void tick(lv_timer_t *t)
{
    (void)t;
    uint32_t dt = lv_tick_elaps(last_tick);
    last_tick = lv_tick_get();
    if (pomo.phase == POMO_IDLE) {
        return;
    }
    if (pomodoro_update(&pomo, dt)) {
        char text[EVENT_TEXT_MAX];
        if (pomo.phase == POMO_BREAK) {
            lv_snprintf(text, sizeof(text), "Hora da pausa! %u minutos.", (unsigned)(pomo.break_ms / MIN_MS));
        } else {
            lv_snprintf(text, sizeof(text), "De volta ao foco!");
        }
        event_bus_publish_text(EV_ALERT, text, 0);
    }
    uint32_t s = pomodoro_seconds_left(&pomo);
    if (s != shown_seconds) {
        shown_seconds = s;
        refresh();
        publish_state();
    }
}

static void on_button(lv_event_t *e)
{
    (void)e;
    if (pomo.phase == POMO_IDLE) {
        pomodoro_start(&pomo);
    } else {
        pomodoro_stop(&pomo);
    }
    last_tick = lv_tick_get();
    shown_seconds = UINT32_MAX;
    refresh();
    publish_state();
}

// New durations take effect on the next phase; a running one keeps its time.
static void apply_settings(void)
{
    pomo.focus_ms = (uint32_t)settings_get_int("focus_min") * MIN_MS;
    pomo.break_ms = (uint32_t)settings_get_int("break_min") * MIN_MS;
    if (phase_label) {
        refresh();
    }
}

static void on_event(const event_t *ev)
{
    if (ev->id == EV_SETTINGS_CHANGED) {
        apply_settings();
    }
}

static void init(void)
{
    pomodoro_init(&pomo, 0, 0);
    apply_settings();
    last_tick = lv_tick_get();
    lv_timer_create(tick, TICK_MS, NULL);
}

static void create_screen(lv_obj_t *parent)
{
    phase_label = ui_title(parent, "");

    time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(time_label, &font_digits_64, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t *btn = ui_button(parent, "", on_button, NULL);
    lv_obj_set_width(btn, 180);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    button_label = lv_obj_get_child(btn, 0);

    refresh();
}

static const setting_def_t settings[] = {
    {"focus_min", "Pomodoro", "Foco (minutos)", SETTING_INT, 1, 120, 25, NULL},
    {"break_min", "Pomodoro", "Pausa (minutos)", SETTING_INT, 1, 60, 5, NULL},
};

const module_t timer_module = {
    .name = "timer",
    .init = init,
    .on_event = on_event,
    .create_screen = create_screen,
    .settings = settings,
    .settings_count = sizeof(settings) / sizeof(settings[0]),
};
