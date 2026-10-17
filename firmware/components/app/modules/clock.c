#include <time.h>

#include "module.h"

static lv_obj_t *time_label;
static lv_obj_t *date_label;

static void refresh(lv_timer_t *t)
{
    (void)t;
    time_t now = time(NULL);
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    lv_label_set_text_fmt(time_label, "%02d:%02d", tm.tm_hour, tm.tm_min);
    lv_label_set_text_fmt(date_label, "%02d/%02d/%04d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
}

static void create_screen(lv_obj_t *parent)
{
    time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_28, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -16);

    date_label = lv_label_create(parent);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 20);

    refresh(NULL);
    lv_timer_create(refresh, 1000, NULL);
}

const module_t clock_module = {
    .name = "clock",
    .create_screen = create_screen,
};
