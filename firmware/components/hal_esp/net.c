#include "net.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "event_bus.h"
#include "freertos/event_groups.h"

static const char *TAG = "net";

#define CONNECTED_BIT   BIT0
#define RETRY_MIN_MS    1000
#define RETRY_MAX_MS    60000

static EventGroupHandle_t net_events;
static esp_timer_handle_t retry_timer;
static bool sntp_started;
static uint32_t retry_ms = RETRY_MIN_MS;

static void retry_connect(void *arg)
{
    (void)arg;
    esp_wifi_connect();
}

static void on_time_sync(struct timeval *tv)
{
    (void)tv;
    ESP_LOGI(TAG, "time synced");
    event_bus_publish_simple(EV_TIME_SYNCED, 0);
}

static void start_sntp(void)
{
    if (sntp_started) {
        return;
    }
    sntp_started = true;
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = on_time_sync;
    esp_netif_sntp_init(&cfg);
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(net_events, CONNECTED_BIT);
        event_bus_publish_simple(EV_WIFI, 0);
        // Back off without blocking the event loop.
        ESP_LOGW(TAG, "disconnected, retrying in %u ms", (unsigned)retry_ms);
        esp_timer_stop(retry_timer);
        esp_timer_start_once(retry_timer, (uint64_t)retry_ms * 1000);
        retry_ms = retry_ms * 2 > RETRY_MAX_MS ? RETRY_MAX_MS : retry_ms * 2;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "got IP");
        retry_ms = RETRY_MIN_MS;
        xEventGroupSetBits(net_events, CONNECTED_BIT);
        event_bus_publish_simple(EV_WIFI, 1);
        start_sntp();
    }
}

void net_start(const char *ssid, const char *password, const char *tz)
{
    setenv("TZ", tz, 1);
    tzset();

    net_events = xEventGroupCreate();
    if (!ssid || !ssid[0]) {
        ESP_LOGW(TAG, "no Wi-Fi SSID configured (idf.py menuconfig -> desk-buddy)");
        return;
    }

    const esp_timer_create_args_t timer_args = {.callback = retry_connect, .name = "wifi_retry"};
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &retry_timer));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL));

    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool net_wait_connected(TickType_t timeout)
{
    return xEventGroupWaitBits(net_events, CONNECTED_BIT, pdFALSE, pdTRUE, timeout) & CONNECTED_BIT;
}
