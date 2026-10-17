#include "module.h"
#include "ui.h"

LV_FONT_DECLARE(font_pt_20);

static const char *const weekdays[] = {"domingo", "segunda-feira", "terça-feira", "quarta-feira",
                                       "quinta-feira", "sexta-feira", "sábado"};
static const char *const months[] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho",
                                     "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"};

static lv_obj_t *time_label;
static lv_obj_t *date_label;
static int shown_minute = -1;

static void refresh(lv_timer_t *t)
{
    (void)t;
    struct tm tm;
    if (!ui_local_time(&tm)) {
        lv_label_set_text(time_label, "--:--");
        lv_label_set_text(date_label, "acertando o relógio...");
        shown_minute = -1;
        return;
    }
    if (tm.tm_min == shown_minute) {
        return;
    }
    shown_minute = tm.tm_min;
    lv_label_set_text_fmt(time_label, "%02d:%02d", tm.tm_hour, tm.tm_min);
    lv_label_set_text_fmt(date_label, "%s, %d de %s", weekdays[tm.tm_wday], tm.tm_mday, months[tm.tm_mon]);
}

static void on_event(const event_t *ev)
{
    if (ev->id == EV_TIME_SYNCED) {
        shown_minute = -1;
        refresh(NULL);
    }
}

static void create_screen(lv_obj_t *parent)
{
    time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(time_label, &font_digits_64, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -24);

    date_label = lv_label_create(parent);
    lv_obj_set_style_text_font(date_label, &font_pt_20, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(UI_TEXT_DIM), 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 34);

    refresh(NULL);
    lv_timer_create(refresh, 1000, NULL);
}

const module_t clock_module = {
    .name = "clock",
    .on_event = on_event,
    .create_screen = create_screen,
};
