// Pomodoro: 25 min focus / 5 min break, repeating until stopped.

#include "module.h"
#include "pomodoro.h"
#include "ui.h"

LV_FONT_DECLARE(font_pt_20);

#define FOCUS_MS    (25 * 60 * 1000)
#define BREAK_MS    (5 * 60 * 1000)
#define TICK_MS     250

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
    uint32_t s = pomo.phase == POMO_IDLE ? FOCUS_MS / 1000 : pomodoro_seconds_left(&pomo);
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
        event_bus_publish_text(EV_ALERT, pomo.phase == POMO_BREAK ? "Hora da pausa! 5 minutos."
                                                                  : "De volta ao foco!", 0);
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

static void init(void)
{
    pomodoro_init(&pomo, FOCUS_MS, BREAK_MS);
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

const module_t timer_module = {
    .name = "timer",
    .init = init,
    .create_screen = create_screen,
};
