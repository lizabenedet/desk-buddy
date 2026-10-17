#pragma once

#include "esp_err.h"
#include "lvgl.h"

// Brings up SPI bus, ILI9486 panel, XPT2046 touch and the LVGL task.
// Call LVGL APIs only between lvgl_port_lock() / lvgl_port_unlock().
esp_err_t display_init(lv_display_t **out_disp);
