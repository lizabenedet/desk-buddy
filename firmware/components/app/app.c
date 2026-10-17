#include "app.h"

#include "event_bus.h"
#include "home.h"
#include "module.h"

#define DISPATCH_PERIOD_MS 20

static void on_any_event(const event_t *ev, void *ctx)
{
    const module_t *m = ctx;
    m->on_event(ev);
}

static void dispatch_timer_cb(lv_timer_t *t)
{
    (void)t;
    event_bus_dispatch();
}

void app_start(void)
{
    lv_display_t *disp = lv_display_get_default();
    lv_display_set_theme(disp, lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                                                     lv_palette_main(LV_PALETTE_AMBER), true,
                                                     LV_FONT_DEFAULT));

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_t *tiles = lv_tileview_create(scr);
    lv_obj_set_style_bg_opa(tiles, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(tiles, LV_SCROLLBAR_MODE_OFF);

    // Tiles are contiguous: modules without a screen don't take a slot.
    size_t screens = 0;
    for (size_t i = 0; i < app_module_count; i++) {
        screens += app_modules[i]->create_screen != NULL;
    }

    home_create(lv_tileview_add_tile(tiles, 0, 0, screens ? LV_DIR_RIGHT : LV_DIR_NONE));

    uint8_t col = 0;
    for (size_t i = 0; i < app_module_count; i++) {
        const module_t *m = app_modules[i];
        if (m->init) {
            m->init();
        }
        if (m->on_event) {
            event_bus_subscribe(EV_ANY, on_any_event, (void *)m);
        }
        if (m->create_screen) {
            col++;
            lv_dir_t dir = LV_DIR_LEFT | (col < screens ? LV_DIR_RIGHT : LV_DIR_NONE);
            m->create_screen(lv_tileview_add_tile(tiles, col, 0, dir));
        }
    }

    lv_timer_create(dispatch_timer_cb, DISPATCH_PERIOD_MS, NULL);
}
