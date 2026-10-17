#include "app.h"
#include "display.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_ota_ops.h"
#include "net.h"
#include "nvs_flash.h"
#include "pc_server.h"
#include "weather_fetch.h"

static const char *TAG = "main";

static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    init_nvs();

    lv_display_t *disp;
    ESP_ERROR_CHECK(display_init(&disp));
    ESP_LOGI(TAG, "display up");

    // Timezone first so the UI never shows UTC.
    net_start(CONFIG_BUDDY_WIFI_SSID, CONFIG_BUDDY_WIFI_PASSWORD, CONFIG_BUDDY_TZ);

    lvgl_port_lock(0);
    app_start();   // registers the modules' settings
    lvgl_port_unlock();

    weather_fetch_start();
    pc_server_start();

    // We got this far: keep this firmware. A freshly updated image that
    // crashes before here is rolled back by the bootloader on the next reset.
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        ESP_LOGI(TAG, "firmware marked valid");
    }
}
