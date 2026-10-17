// Bring-up firmware: color bars to check panel orientation/colors and a
// touch marker to check touch calibration.

#include "display.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "main";

static lv_obj_t *touch_dot;
static lv_obj_t *touch_label;

static void on_press(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_obj_set_pos(touch_dot, p.x - 8, p.y - 8);
    lv_label_set_text_fmt(touch_label, "toque: %d, %d", (int)p.x, (int)p.y);
}

static void build_test_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // Red, green, blue, white bars, left to right: wrong order = mirrored, wrong colors = RGB/BGR swapped.
    const uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *bar = lv_obj_create(scr);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 120, 60);
        lv_obj_set_pos(bar, i * 120, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(colors[i]), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    }

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "desk-buddy");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -10);

    touch_label = lv_label_create(scr);
    lv_label_set_text(touch_label, "toque na tela");
    lv_obj_set_style_text_color(touch_label, lv_color_white(), 0);
    lv_obj_align(touch_label, LV_ALIGN_CENTER, 0, 30);

    touch_dot = lv_obj_create(scr);
    lv_obj_remove_style_all(touch_dot);
    lv_obj_set_size(touch_dot, 16, 16);
    lv_obj_set_style_radius(touch_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(touch_dot, lv_color_hex(0xFFCC00), 0);
    lv_obj_set_style_bg_opa(touch_dot, LV_OPA_COVER, 0);
    lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_pos(touch_dot, -20, -20);

    lv_obj_add_event_cb(scr, on_press, LV_EVENT_PRESSING, NULL);
}

void app_main(void)
{
    lv_display_t *disp;
    ESP_ERROR_CHECK(display_init(&disp));
    ESP_LOGI(TAG, "display up");

    lvgl_port_lock(0);
    build_test_screen();
    lvgl_port_unlock();
}
