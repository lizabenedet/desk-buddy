#pragma once

// Builds the UI and starts every module. Call once, after lv_init and the
// display driver, from the UI task (or with the LVGL lock held).
void app_start(void);
