// Simulator LVGL config. Only overrides; everything else uses LVGL defaults.
// Keep fonts and color depth in sync with firmware/sdkconfig.defaults.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (512 * 1024)

#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL.h>

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_28 1

#define LV_USE_LOG 1
#define LV_LOG_PRINTF 1

#define LV_USE_SYSMON 1
#define LV_USE_PERF_MONITOR 1

#endif
