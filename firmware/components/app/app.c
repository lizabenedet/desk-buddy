#include "app.h"

#include "event_bus.h"
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

static void on_home_clicked(lv_event_t *e)
{
    (void)e;
    event_bus_publish_simple(EV_FACE_TOUCHED, 0);
}

static void build_home(lv_obj_t *tile)
{
    // Placeholder until the procedural face (phase 4).
    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, "(o_o)");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_center(label);

    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tile, on_home_clicked, LV_EVENT_CLICKED, NULL);
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

    build_home(lv_tileview_add_tile(tiles, 0, 0, LV_DIR_RIGHT));

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
