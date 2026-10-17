#include <string.h>

#include "check.h"
#include "event_bus.h"
#include "settings.h"
#include "storage.h"
#include "web.h"

/* In-memory storage.h: a few keyed blobs. */
static struct {
    char key[16];
    unsigned char data[64];
    size_t len;
} store[8];

bool storage_load(const char *key, void *buf, size_t len)
{
    for (int i = 0; i < 8; i++) {
        if (strcmp(store[i].key, key) == 0 && store[i].len == len) {
            memcpy(buf, store[i].data, len);
            return true;
        }
    }
    return false;
}

bool storage_save(const char *key, const void *buf, size_t len)
{
    for (int i = 0; i < 8; i++) {
        if (store[i].key[0] == '\0' || strcmp(store[i].key, key) == 0) {
            strcpy(store[i].key, key);
            memcpy(store[i].data, buf, len);
            store[i].len = len;
            return true;
        }
    }
    return false;
}

static const setting_def_t defs[] = {
    {"focus_min", "Pomodoro", "Foco (min)", SETTING_INT, 1, 90, 25, NULL},
    {"weather_lat", "Clima", "Latitude", SETTING_TEXT, 0, 0, 0, "-26.4851"},
};

static int changes;
static char last_key[EVENT_TEXT_MAX];
static void on_change(const event_t *ev, void *ctx)
{
    (void)ctx;
    changes++;
    strcpy(last_key, ev->text);
}

static void setup(void)
{
    memset(store, 0, sizeof(store));
    settings_reset();
    event_bus_reset();
    event_bus_subscribe(EV_SETTINGS_CHANGED, on_change, NULL);
    changes = 0;
    settings_register(defs, 2);
}

static void test_defaults_and_json(void)
{
    setup();
    CHECK(settings_get_int("focus_min") == 25);
    char t[SETTING_TEXT_MAX];
    settings_get_text("weather_lat", t, sizeof(t));
    CHECK(strcmp(t, "-26.4851") == 0);
    CHECK(settings_get_int("nope") == 0);

    char *json = settings_to_json();
    CHECK(strstr(json, "\"key\":\"focus_min\""));
    CHECK(strstr(json, "\"min\":1") && strstr(json, "\"max\":90"));
    CHECK(strstr(json, "\"value\":\"-26.4851\""));
    free(json);
}

static void test_apply_persists_and_notifies(void)
{
    setup();
    char err[128];
    CHECK(settings_apply_json("{\"focus_min\":30,\"weather_lat\":\"-27.0\"}", err, sizeof(err)));
    CHECK(settings_get_int("focus_min") == 30);
    event_bus_dispatch();
    CHECK(changes == 2);

    // Unchanged values don't notify.
    CHECK(settings_apply_json("{\"focus_min\":30}", err, sizeof(err)));
    changes = 0;
    event_bus_dispatch();
    CHECK(changes == 0);

    // A fresh registration picks up what was stored.
    settings_reset();
    settings_register(defs, 2);
    CHECK(settings_get_int("focus_min") == 30);
    char t[SETTING_TEXT_MAX];
    settings_get_text("weather_lat", t, sizeof(t));
    CHECK(strcmp(t, "-27.0") == 0);
}

static void test_rejects_bad_input_atomically(void)
{
    setup();
    char err[128];
    CHECK(!settings_apply_json("{\"focus_min\":200}", err, sizeof(err)));
    CHECK(strstr(err, "entre 1 e 90"));
    CHECK(!settings_apply_json("{\"focus_min\":2.5}", err, sizeof(err)));
    CHECK(!settings_apply_json("{\"focus_min\":\"30\"}", err, sizeof(err)));
    CHECK(!settings_apply_json("{\"unknown\":1}", err, sizeof(err)));
    CHECK(!settings_apply_json("not json", err, sizeof(err)));
    CHECK(!settings_apply_json("{\"weather_lat\":\"0123456789012345678901234567890123\"}", err, sizeof(err)));
    // Valid field + invalid field: nothing applied.
    CHECK(!settings_apply_json("{\"weather_lat\":\"1.0\",\"focus_min\":0}", err, sizeof(err)));
    char t[SETTING_TEXT_MAX];
    settings_get_text("weather_lat", t, sizeof(t));
    CHECK(strcmp(t, "-26.4851") == 0);
    event_bus_dispatch();
    CHECK(changes == 0);
}

static void test_out_of_range_stored_value_falls_back(void)
{
    setup();
    int32_t bad = 500;
    storage_save("focus_min", &bad, sizeof(bad));
    settings_reset();
    settings_register(defs, 2);
    CHECK(settings_get_int("focus_min") == 25);
}

static void test_web_auth(void)
{
    // base64("buddy:ABCD-2345") = YnVkZHk6QUJDRC0yMzQ1
    CHECK(web_auth_ok("Basic YnVkZHk6QUJDRC0yMzQ1", "ABCD-2345"));
    CHECK(!web_auth_ok("Basic YnVkZHk6QUJDRC0yMzQ2", "ABCD-2345"));
    CHECK(!web_auth_ok("Bearer YnVkZHk6QUJDRC0yMzQ1", "ABCD-2345"));
    CHECK(!web_auth_ok(NULL, "ABCD-2345"));
    CHECK(!web_auth_ok("Basic ", ""));
}

static void test_settings_body(void)
{
    setup();
    char *body = web_settings_body("1.2.3", true);
    CHECK(strstr(body, "\"settings\":[") && strstr(body, "\"version\":\"1.2.3\"") && strstr(body, "\"ota\":true"));
    free(body);

    bool ok;
    char *reply = web_apply_settings("{\"focus_min\":45}", &ok);
    CHECK(ok && strcmp(reply, "{\"ok\":true}") == 0);
    free(reply);
    reply = web_apply_settings("{\"focus_min\":0}", &ok);
    CHECK(!ok && strstr(reply, "\"ok\":false") && strstr(reply, "entre 1 e 90"));
    free(reply);
}

int main(void)
{
    test_defaults_and_json();
    test_apply_persists_and_notifies();
    test_rejects_bad_input_atomically();
    test_out_of_range_stored_value_falls_back();
    test_web_auth();
    test_settings_body();
    puts("settings: all tests passed");
    return 0;
}
