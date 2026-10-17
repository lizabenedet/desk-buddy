#include "web_routes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "event_bus.h"
#include "pc_link.h"
#include "web.h"

static const char *TAG = "web";

#define BODY_MAX        2048
#define OTA_CHUNK       4096
#define RESTART_DELAY_US (1500 * 1000)

extern const char index_html_start[] asm("_binary_index_html_start");

static char token[PC_LINK_TOKEN_LEN];

static bool authorized(httpd_req_t *req)
{
    char header[128] = "";
    httpd_req_get_hdr_value_str(req, "Authorization", header, sizeof(header));
    if (web_auth_ok(header, token)) {
        return true;
    }
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"" WEB_AUTH_REALM "\", charset=\"UTF-8\"");
    httpd_resp_sendstr(req, "token errado");
    return false;
}

static esp_err_t send_json(httpd_req_t *req, const char *status, const char *json)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t page_get(httpd_req_t *req)
{
    if (!authorized(req)) {
        return ESP_OK;
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req, index_html_start);
}

static esp_err_t settings_get(httpd_req_t *req)
{
    if (!authorized(req)) {
        return ESP_OK;
    }
    char *json = web_settings_body(esp_app_get_description()->version, true);
    esp_err_t err = send_json(req, "200 OK", json ? json : "{}");
    free(json);
    return err;
}

static esp_err_t settings_post(httpd_req_t *req)
{
    if (!authorized(req)) {
        return ESP_OK;
    }
    if (req->content_len >= BODY_MAX) {
        return send_json(req, "413 Payload Too Large", "{\"ok\":false,\"error\":\"grande demais\"}");
    }
    char body[BODY_MAX];
    size_t got = 0;
    while (got < req->content_len) {
        int n = httpd_req_recv(req, body + got, req->content_len - got);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (n <= 0) {
            return ESP_FAIL;
        }
        got += (size_t)n;
    }
    body[got] = '\0';
    bool ok;
    char *reply = web_apply_settings(body, &ok);
    esp_err_t err = send_json(req, ok ? "200 OK" : "400 Bad Request", reply);
    free(reply);
    return err;
}

static void restart_cb(void *arg)
{
    (void)arg;
    esp_restart();
}

static esp_err_t ota_fail(httpd_req_t *req, esp_ota_handle_t ota, const char *why)
{
    ESP_LOGW(TAG, "OTA failed: %s", why);
    if (ota) {
        esp_ota_abort(ota);
    }
    event_bus_publish_text(EV_SAY, "A atualização falhou.", 5000);
    char reply[160];
    snprintf(reply, sizeof(reply), "{\"ok\":false,\"error\":\"%s\"}", why);
    return send_json(req, "400 Bad Request", reply);
}

// Streams the image into the inactive OTA slot. The new firmware confirms
// itself after booting (main.c); if it doesn't, the bootloader rolls back.
static esp_err_t ota_post(httpd_req_t *req)
{
    if (!authorized(req)) {
        return ESP_OK;
    }
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) {
        return ota_fail(req, 0, "sem partição de atualização");
    }
    if (req->content_len == 0 || req->content_len > part->size) {
        return ota_fail(req, 0, "tamanho do arquivo inválido");
    }
    esp_ota_handle_t ota = 0;
    if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &ota) != ESP_OK) {
        return ota_fail(req, 0, "não consegui iniciar a gravação");
    }
    char *buf = malloc(OTA_CHUNK);
    if (!buf) {
        return ota_fail(req, ota, "sem memória");
    }
    event_bus_publish_text(EV_SAY, "Atualizando firmware...", 0);

    size_t remaining = req->content_len;
    int shown_pct = 0;
    while (remaining > 0) {
        int n = httpd_req_recv(req, buf, remaining < OTA_CHUNK ? remaining : OTA_CHUNK);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (n <= 0) {
            free(buf);
            return ota_fail(req, ota, "a conexão caiu");
        }
        if (esp_ota_write(ota, buf, (size_t)n) != ESP_OK) {
            free(buf);
            return ota_fail(req, ota, "erro ao gravar");
        }
        remaining -= (size_t)n;
        int pct = (int)(100 - remaining * 100 / req->content_len);
        if (pct >= shown_pct + 10) {
            shown_pct = pct - pct % 10;
            char text[EVENT_TEXT_MAX];
            snprintf(text, sizeof(text), "Atualizando firmware... %d%%", shown_pct);
            event_bus_publish_text(EV_SAY, text, 0);
        }
    }
    free(buf);

    esp_err_t err = esp_ota_end(ota);   // also validates the image
    if (err != ESP_OK) {
        return ota_fail(req, 0, err == ESP_ERR_OTA_VALIDATE_FAILED ? "o arquivo não é um firmware válido"
                                                                   : "erro ao finalizar");
    }
    if (esp_ota_set_boot_partition(part) != ESP_OK) {
        return ota_fail(req, 0, "não consegui ativar o firmware novo");
    }
    ESP_LOGI(TAG, "OTA done (%u bytes), restarting", (unsigned)req->content_len);
    event_bus_publish_text(EV_SAY, "Atualizado! Reiniciando...", 0);
    send_json(req, "200 OK", "{\"ok\":true}");

    const esp_timer_create_args_t args = {.callback = restart_cb, .name = "ota_restart"};
    esp_timer_handle_t timer;
    if (esp_timer_create(&args, &timer) == ESP_OK) {
        esp_timer_start_once(timer, RESTART_DELAY_US);   // let the reply reach the browser
    }
    return ESP_OK;
}

void web_routes_register(httpd_handle_t server, const char *pairing_token)
{
    strncpy(token, pairing_token, sizeof(token) - 1);
    static const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = page_get},
        {.uri = "/api/settings", .method = HTTP_GET, .handler = settings_get},
        {.uri = "/api/settings", .method = HTTP_POST, .handler = settings_post},
        {.uri = "/api/ota", .method = HTTP_POST, .handler = ota_post},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
}
