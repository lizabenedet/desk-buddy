#include "pc_server.h"

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mdns.h"
#include "pc_link.h"

static const char *TAG = "pc_server";

#define HOSTNAME        "buddy"
#define FRAME_MAX       2048
#define REPLY_MAX       160
#define CHECK_PERIOD_US (5 * 1000 * 1000)

// Only touched from the httpd task (handlers, close_fn and queued work).
static httpd_handle_t server;
static pc_session_t session;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void send_text(httpd_req_t *req, const char *text)
{
    httpd_ws_frame_t out = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)text,
        .len = strlen(text),
        .final = true,
    };
    httpd_ws_send_frame(req, &out);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        return ESP_OK;   // handshake done
    }
    int fd = httpd_req_to_sockfd(req);
    httpd_ws_frame_t frame = {.type = HTTPD_WS_TYPE_TEXT};
    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);   // length only
    if (err != ESP_OK) {
        return err;
    }
    if (frame.type != HTTPD_WS_TYPE_TEXT || frame.len == 0 || frame.len >= FRAME_MAX) {
        httpd_sess_trigger_close(server, fd);
        return ESP_OK;
    }
    char *buf = malloc(frame.len + 1);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    frame.payload = (uint8_t *)buf;
    err = httpd_ws_recv_frame(req, &frame, frame.len);
    if (err == ESP_OK) {
        buf[frame.len] = '\0';
        char reply[REPLY_MAX];
        pc_session_action_t action = pc_session_frame(&session, fd, buf, now_ms(), reply, sizeof(reply));
        if (reply[0]) {
            send_text(req, reply);
        }
        if (action.close_other >= 0) {
            httpd_sess_trigger_close(server, action.close_other);
        }
        if (action.close_this) {
            httpd_sess_trigger_close(server, fd);
        }
    }
    free(buf);
    return err;
}

static void on_close(httpd_handle_t hd, int fd)
{
    (void)hd;
    pc_session_closed(&session, fd);
    close(fd);
}

static void check_timeout(void *arg)
{
    (void)arg;
    int fd = pc_session_expired(&session, now_ms());
    if (fd >= 0) {
        ESP_LOGW(TAG, "agent silent, dropping");
        httpd_sess_trigger_close(server, fd);
    }
}

static void timer_cb(void *arg)
{
    (void)arg;
    httpd_queue_work(server, check_timeout, NULL);   // run on the httpd task
}

static void start_mdns(void)
{
    if (mdns_init() != ESP_OK) {
        ESP_LOGW(TAG, "mDNS init failed");
        return;
    }
    mdns_hostname_set(HOSTNAME);
    mdns_instance_name_set("desk-buddy");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
}

void pc_server_start(void)
{
    char token[PC_LINK_TOKEN_LEN];
    pc_link_token(token);
    pc_session_init(&session, token);

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.close_fn = on_close;
    cfg.lru_purge_enable = true;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed");
        return;
    }
    static const httpd_uri_t ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(server, &ws);

    const esp_timer_create_args_t args = {.callback = timer_cb, .name = "pc_timeout"};
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, CHECK_PERIOD_US));

    start_mdns();
    ESP_LOGI(TAG, "listening on ws://%s.local/ws", HOSTNAME);
}
