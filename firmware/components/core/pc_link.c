#include "pc_link.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "event_bus.h"
#include "storage.h"

#ifdef ESP_PLATFORM
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
static portMUX_TYPE metrics_mux = portMUX_INITIALIZER_UNLOCKED;
#define METRICS_LOCK()   portENTER_CRITICAL(&metrics_mux)
#define METRICS_UNLOCK() portEXIT_CRITICAL(&metrics_mux)
#define RANDOM()         esp_random()
#else
#include <time.h>
#define METRICS_LOCK()
#define METRICS_UNLOCK()
static uint32_t sim_random(void)
{
    static bool seeded;
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = true;
    }
    return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}
#define RANDOM() sim_random()
#endif

#define TOKEN_KEY "token"

static pc_metrics_t metrics;

static void set_reply(char *reply, size_t len, const char *json)
{
    snprintf(reply, len, "%s", json);
}

static void set_error(char *reply, size_t len, const char *reason)
{
    snprintf(reply, len, "{\"type\":\"error\",\"reason\":\"%s\"}", reason);
}

static float number_or_nan(const cJSON *root, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsNumber(item) ? (float)item->valuedouble : NAN;
}

static bool handle_claude(const cJSON *root)
{
    static const struct {
        const char *name;
        claude_event_t ev;
    } names[] = {
        {"session_start", CLAUDE_SESSION_START}, {"prompt", CLAUDE_PROMPT}, {"waiting", CLAUDE_WAITING},
        {"done", CLAUDE_DONE}, {"session_end", CLAUDE_SESSION_END},
    };
    const char *name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "event"));
    if (!name) {
        return false;
    }
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (strcmp(name, names[i].name) == 0) {
            const char *project = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "project"));
            event_t ev = {.id = EV_CLAUDE, .value = names[i].ev};
            if (project) {
                strncpy(ev.text, project, EVENT_TEXT_MAX - 1);
            }
            event_bus_publish(&ev);
            return true;
        }
    }
    return false;
}

static void handle_metrics(const cJSON *root)
{
    pc_metrics_t m = {
        .valid = true,
        .cpu = number_or_nan(root, "cpu"),
        .ram = number_or_nan(root, "ram"),
        .gpu = number_or_nan(root, "gpu"),
        .cpu_temp = number_or_nan(root, "cpu_temp"),
        .gpu_temp = number_or_nan(root, "gpu_temp"),
    };
    METRICS_LOCK();
    metrics = m;
    METRICS_UNLOCK();
    event_bus_publish_simple(EV_PC_METRICS, 0);
}

pc_link_result_t pc_link_handle(const char *json, bool authed, const char *token, char *reply, size_t reply_len)
{
    reply[0] = '\0';
    cJSON *root = cJSON_Parse(json);
    const char *type = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "type"));
    pc_link_result_t result = PC_LINK_OK;

    if (!type) {
        set_error(reply, reply_len, "bad_message");
        result = authed ? PC_LINK_OK : PC_LINK_CLOSE;
    } else if (strcmp(type, "hello") == 0) {
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
        const char *given = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "token"));
        if (!cJSON_IsNumber(version) || version->valueint != PC_LINK_VERSION) {
            set_error(reply, reply_len, "unsupported_version");
            result = PC_LINK_CLOSE;
        } else if (!given || strcmp(given, token) != 0) {
            set_error(reply, reply_len, "bad_token");
            result = PC_LINK_CLOSE;
        } else {
            set_reply(reply, reply_len, "{\"type\":\"welcome\",\"version\":1}");
            result = PC_LINK_AUTHENTICATED;
        }
    } else if (!authed) {
        set_error(reply, reply_len, "not_authenticated");
        result = PC_LINK_CLOSE;
    } else if (strcmp(type, "claude") == 0) {
        if (!handle_claude(root)) {
            set_error(reply, reply_len, "bad_message");
        }
    } else if (strcmp(type, "metrics") == 0) {
        handle_metrics(root);
    } else if (strcmp(type, "pc") == 0) {
        const char *state = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "state"));
        if (state && strcmp(state, "sleep") == 0) {
            event_bus_publish_simple(EV_PC_SLEEP, 0);
        } else {
            set_error(reply, reply_len, "bad_message");
        }
    } else if (strcmp(type, "ping") != 0) {
        set_error(reply, reply_len, "bad_message");
    }

    cJSON_Delete(root);
    return result;
}

void pc_session_init(pc_session_t *s, const char *token)
{
    *s = (pc_session_t){.authed_conn = -1};
    strncpy(s->token, token, PC_LINK_TOKEN_LEN - 1);
}

pc_session_action_t pc_session_frame(pc_session_t *s, int conn, const char *json, uint32_t now_ms,
                                     char *reply, size_t reply_len)
{
    pc_session_action_t action = {.close_this = false, .close_other = -1};
    bool authed = conn == s->authed_conn;
    switch (pc_link_handle(json, authed, s->token, reply, reply_len)) {
    case PC_LINK_AUTHENTICATED:
        if (s->authed_conn < 0) {
            event_bus_publish_simple(EV_PC_CONNECTED, 1);
        } else if (s->authed_conn != conn) {
            action.close_other = s->authed_conn;   // newest agent wins, stays "connected"
        }
        s->authed_conn = conn;
        s->last_rx_ms = now_ms;
        break;
    case PC_LINK_CLOSE:
        action.close_this = true;
        break;
    case PC_LINK_OK:
        if (authed) {
            s->last_rx_ms = now_ms;
        }
        break;
    }
    return action;
}

void pc_session_closed(pc_session_t *s, int conn)
{
    if (conn != s->authed_conn) {
        return;
    }
    s->authed_conn = -1;
    pc_metrics_clear();
    event_bus_publish_simple(EV_PC_CONNECTED, 0);
}

int pc_session_expired(const pc_session_t *s, uint32_t now_ms)
{
    if (s->authed_conn >= 0 && now_ms - s->last_rx_ms > PC_LINK_TIMEOUT_MS) {
        return s->authed_conn;
    }
    return -1;
}

void pc_metrics_get(pc_metrics_t *out)
{
    METRICS_LOCK();
    *out = metrics;
    METRICS_UNLOCK();
}

void pc_metrics_clear(void)
{
    METRICS_LOCK();
    metrics = (pc_metrics_t){0};
    METRICS_UNLOCK();
}

float pc_metrics_max_temp(const pc_metrics_t *m)
{
    if (!m->valid) {
        return NAN;
    }
    if (isnan(m->cpu_temp)) {
        return m->gpu_temp;
    }
    if (isnan(m->gpu_temp)) {
        return m->cpu_temp;
    }
    return m->cpu_temp > m->gpu_temp ? m->cpu_temp : m->gpu_temp;
}

bool pc_metrics_hot(const pc_metrics_t *m)
{
    float t = pc_metrics_max_temp(m);
    return !isnan(t) && t >= PC_HOT_C;
}

bool pc_metrics_busy(const pc_metrics_t *m)
{
    return m->valid && !isnan(m->cpu) && m->cpu >= PC_BUSY_CPU;
}

static bool token_valid(const char *t)
{
    if (strlen(t) != PC_LINK_TOKEN_LEN - 1 || t[4] != '-') {
        return false;
    }
    for (int i = 0; i < PC_LINK_TOKEN_LEN - 1; i++) {
        if (i != 4 && !((t[i] >= 'A' && t[i] <= 'Z') || (t[i] >= '0' && t[i] <= '9'))) {
            return false;
        }
    }
    return true;
}

void pc_link_token(char out[PC_LINK_TOKEN_LEN])
{
    if (storage_load(TOKEN_KEY, out, PC_LINK_TOKEN_LEN) && out[PC_LINK_TOKEN_LEN - 1] == '\0' && token_valid(out)) {
        return;
    }
    // No look-alikes (0/O, 1/I/L) so it's easy to type from the screen.
    static const char alphabet[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
    for (int i = 0; i < PC_LINK_TOKEN_LEN - 1; i++) {
        out[i] = i == 4 ? '-' : alphabet[RANDOM() % (sizeof(alphabet) - 1)];
    }
    out[PC_LINK_TOKEN_LEN - 1] = '\0';
    storage_save(TOKEN_KEY, out, PC_LINK_TOKEN_LEN);
}
