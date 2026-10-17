#include "app.h"
#include "display.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "main";

void app_main(void)
{
    lv_display_t *disp;
    ESP_ERROR_CHECK(display_init(&disp));
    ESP_LOGI(TAG, "display up");

    lvgl_port_lock(0);
    app_start();
    lvgl_port_unlock();
}
