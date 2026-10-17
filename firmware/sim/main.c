// Desktop simulator: same app code as the firmware, rendered in an SDL
// window at the panel's resolution, mouse acting as the touch screen.

#include "app.h"
#include "lvgl.h"

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

#define SIM_H_RES 480
#define SIM_V_RES 320

int main(void)
{
    lv_init();
    lv_display_t *disp = lv_sdl_window_create(SIM_H_RES, SIM_V_RES);
    lv_sdl_window_set_title(disp, "desk-buddy");
    lv_sdl_mouse_create();

    app_start();

    for (;;) {
        uint32_t idle = lv_timer_handler();
        sleep_ms(idle < 5 ? idle : 5);
    }
}
