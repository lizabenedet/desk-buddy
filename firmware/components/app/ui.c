#include "ui.h"

#include "icons.h"
#include "weather.h"

#define CLOCK_VALID_YEAR 2025

bool ui_local_time(struct tm *out)
{
    time_t now = time(NULL);
#ifdef _WIN32
    localtime_s(out, &now);
#else
    localtime_r(&now, out);
#endif
    return out->tm_year + 1900 >= CLOCK_VALID_YEAR;
}

const char *ui_weather_icon(int wmo_code, bool night)
{
    switch (weather_kind(wmo_code)) {
    case WEATHER_CLEAR: return night ? ICON_MOON : ICON_SUN;
    case WEATHER_PARTLY: return ICON_PARTLY;
    case WEATHER_CLOUDY: return ICON_CLOUD;
    case WEATHER_FOG: return ICON_FOG;
    case WEATHER_DRIZZLE: return ICON_DRIZZLE;
    case WEATHER_RAIN: return ICON_RAIN;
    case WEATHER_SNOW: return ICON_SNOW;
    case WEATHER_THUNDER: return ICON_THUNDER;
    default: return ICON_NO_DATA;
    }
}

lv_obj_t *ui_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_height(btn, 52);
    lv_obj_set_style_min_width(btn, 96, 0);
    lv_obj_set_style_radius(btn, 14, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &font_pt_20, 0);
    lv_obj_center(label);
    return btn;
}

lv_obj_t *ui_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &font_pt_20, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_TEXT_DIM), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    return label;
}
