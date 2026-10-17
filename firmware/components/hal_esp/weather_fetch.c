#include "weather_fetch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net.h"
#include "settings.h"
#include "weather.h"

static const char *TAG = "weather";

#define REFRESH_MS      (15 * 60 * 1000)
#define RETRY_MS        (60 * 1000)
#define BODY_MAX        4096
#define URL_MAX         512

static TaskHandle_t fetch_task_handle;

// Coordinates go straight into the URL: allow only what a number needs.
static bool is_coordinate(const char *s)
{
    if (!*s) {
        return false;
    }
    for (; *s; s++) {
        if (!((*s >= '0' && *s <= '9') || *s == '.' || *s == '-')) {
            return false;
        }
    }
    return true;
}

static bool fetch_once(char *body)
{
    char lat[SETTING_TEXT_MAX], lon[SETTING_TEXT_MAX];
    settings_get_text("weather_lat", lat, sizeof(lat));
    settings_get_text("weather_lon", lon, sizeof(lon));
    if (!is_coordinate(lat) || !is_coordinate(lon)) {
        ESP_LOGW(TAG, "invalid location '%s', '%s'", lat, lon);
        return false;
    }

    // timezone=auto: hours come in the location's own timezone.
    char url[URL_MAX];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&current=temperature_2m,weather_code&hourly=precipitation_probability"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
             "&forecast_days=1&timezone=auto",
             lat, lon);

    esp_http_client_config_t http = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http);
    bool ok = false;
    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGW(TAG, "connect failed");
        goto done;
    }
    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGW(TAG, "HTTP %d", status);
        goto done;
    }
    int total = 0, n;
    while (total < BODY_MAX - 1 && (n = esp_http_client_read(client, body + total, BODY_MAX - 1 - total)) > 0) {
        total += n;
    }
    body[total] = '\0';

    weather_report_t report;
    if (!weather_parse_open_meteo(body, time(NULL), &report)) {
        ESP_LOGW(TAG, "unexpected response (%d bytes)", total);
        goto done;
    }
    weather_store_set(&report);
    event_bus_publish_simple(EV_WEATHER, 0);
    ESP_LOGI(TAG, "%.1f C, code %d", report.temp, report.code);
    ok = true;
done:
    esp_http_client_cleanup(client);
    return ok;
}

static void fetch_task(void *arg)
{
    (void)arg;
    char *body = malloc(BODY_MAX);
    configASSERT(body);
    for (;;) {
        net_wait_connected(portMAX_DELAY);
        bool ok = fetch_once(body);
        // Sleep until the next refresh, or until the location changes.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ok ? REFRESH_MS : RETRY_MS));
    }
}

static void on_settings_changed(const event_t *ev, void *ctx)
{
    (void)ctx;
    if (strncmp(ev->text, "weather_", 8) == 0) {
        xTaskNotifyGive(fetch_task_handle);
    }
}

void weather_fetch_start(void)
{
    xTaskCreate(fetch_task, "weather", 10 * 1024, NULL, 3, &fetch_task_handle);
    event_bus_subscribe(EV_SETTINGS_CHANGED, on_settings_changed, NULL);
}
