// Desktop simulator: same app code as the firmware, rendered in an SDL
// window at the panel's resolution, mouse acting as the touch screen.
//
// Dev keys: 0-9 and '-' set the resting mood, 'r' a short happy reaction,
// 's' makes the buddy say something.

#include "app.h"
#include "event_bus.h"
#include "face.h"
#include "lvgl.h"

#include <ctype.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

#define SIM_H_RES 480
#define SIM_V_RES 320

static void on_key(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    if (key < 128) {
        key = (uint32_t)tolower((int)key); // Caps Lock shouldn't matter
    }
    if (key >= '0' && key <= '9') {
        event_bus_publish_simple(EV_FACE_MOOD, (int32_t)(key - '0'));
    } else if (key == '-') {
        event_bus_publish_simple(EV_FACE_MOOD, MOOD_TIRED);
    } else if (key == 'r') {
        event_t ev = {.id = EV_FACE_REACT, .value = MOOD_HAPPY, .arg = 2000};
        event_bus_publish(&ev);
    } else if (key == 's') {
        event_t ev = {.id = EV_SAY, .arg = 3000};
        strcpy(ev.text, "Olá! Ação não é fácil, viu?");
        event_bus_publish(&ev);
    }
}

static void setup_dev_keys(void)
{
    lv_group_t *group = lv_group_create();
    lv_indev_set_group(lv_sdl_keyboard_create(), group);

    // Invisible receiver for key events.
    lv_obj_t *sink = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(sink);
    lv_obj_set_size(sink, 0, 0);
    lv_group_add_obj(group, sink);
    lv_obj_add_event_cb(sink, on_key, LV_EVENT_KEY, NULL);
}

int main(void)
{
    lv_init();
    lv_display_t *disp = lv_sdl_window_create(SIM_H_RES, SIM_V_RES);
    lv_sdl_window_set_title(disp, "desk-buddy");
    lv_sdl_mouse_create();

    app_start();
    setup_dev_keys();

    for (;;) {
        uint32_t idle = lv_timer_handler();
        sleep_ms(idle < 5 ? idle : 5);
    }
}
