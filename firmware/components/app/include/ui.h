#pragma once

#include <stdbool.h>
#include <time.h>

#include "lvgl.h"

LV_FONT_DECLARE(font_digits_64);

#define UI_TEXT_DIM 0x9AA4B0

// Local time; false until the clock has been set (NTP on the board).
bool ui_local_time(struct tm *out);

// Icon glyph (icons_20 / icons_64) for a WMO code.
const char *ui_weather_icon(int wmo_code, bool night);

// Big touch-friendly button (the touch panel is resistive).
lv_obj_t *ui_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data);

// Small dim caption at the top of a module screen.
lv_obj_t *ui_title(lv_obj_t *parent, const char *text);
