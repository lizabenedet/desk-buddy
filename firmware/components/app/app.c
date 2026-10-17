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

    home_create(lv_tileview_add_tile(tiles, 0, 0, LV_DIR_RIGHT));

    for (size_t i = 0; i < app_module_count; i++) {
        const module_t *m = app_modules[i];
        if (m->init) {
            m->init();
        }
        if (m->on_event) {
            event_bus_subscribe(EV_ANY, on_any_event, (void *)m);
        }
        if (m->create_screen) {
            lv_dir_t dir = LV_DIR_LEFT | (i + 1 < app_module_count ? LV_DIR_RIGHT : 0);
            m->create_screen(lv_tileview_add_tile(tiles, (uint8_t)(i + 1), 0, dir));
        }
    }

    lv_timer_create(dispatch_timer_cb, DISPATCH_PERIOD_MS, NULL);
}
