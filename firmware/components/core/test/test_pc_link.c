#include <string.h>

#include "check.h"
#include "event_bus.h"
#include "pc_link.h"
#include "storage.h"

/* In-memory storage.h for this test. */
static char stored[64];
static size_t stored_len;
bool storage_load(const char *key, void *buf, size_t len)
{
    (void)key;
    if (stored_len != len) {
        return false;
    }
    memcpy(buf, stored, len);
    return true;
}
bool storage_save(const char *key, const void *buf, size_t len)
{
    (void)key;
    memcpy(stored, buf, len);
    stored_len = len;
    return true;
}

#define TOKEN "ABCD-2345"

static event_t last;
static int count;
static void capture(const event_t *ev, void *ctx)
{
    (void)ctx;
    last = *ev;
    count++;
}

static void setup(void)
{
    event_bus_reset();
    event_bus_subscribe(EV_ANY, capture, NULL);
    count = 0;
    pc_metrics_clear();
}

static pc_link_result_t handle(const char *json, bool authed, char *reply)
{
    return pc_link_handle(json, authed, TOKEN, reply, 128);
}

static void test_hello(void)
{
    char reply[128];
    setup();
    CHECK(handle("{\"type\":\"hello\",\"version\":1,\"token\":\"ABCD-2345\"}", false, reply) == PC_LINK_AUTHENTICATED);
    CHECK(strstr(reply, "welcome"));
    CHECK(handle("{\"type\":\"hello\",\"version\":1,\"token\":\"WRONG\"}", false, reply) == PC_LINK_CLOSE);
    CHECK(strstr(reply, "bad_token"));
    CHECK(handle("{\"type\":\"hello\",\"version\":2,\"token\":\"ABCD-2345\"}", false, reply) == PC_LINK_CLOSE);
    CHECK(strstr(reply, "unsupported_version"));
    CHECK(handle("{\"type\":\"hello\",\"version\":1}", false, reply) == PC_LINK_CLOSE);
}

static void test_requires_auth(void)
{
    char reply[128];
    setup();
    CHECK(handle("{\"type\":\"claude\",\"event\":\"done\"}", false, reply) == PC_LINK_CLOSE);
    CHECK(strstr(reply, "not_authenticated"));
    CHECK(handle("garbage", false, reply) == PC_LINK_CLOSE);
    event_bus_dispatch();
    CHECK(count == 0);
}

static void test_claude_events(void)
{
    char reply[128];
    setup();
    CHECK(handle("{\"type\":\"claude\",\"event\":\"waiting\",\"project\":\"desk-buddy\"}", true, reply) == PC_LINK_OK);
    CHECK(reply[0] == '\0');
    event_bus_dispatch();
    CHECK(count == 1 && last.id == EV_CLAUDE && last.value == CLAUDE_WAITING);
    CHECK(strcmp(last.text, "desk-buddy") == 0);

    handle("{\"type\":\"claude\",\"event\":\"done\"}", true, reply);
    event_bus_dispatch();
    CHECK(last.value == CLAUDE_DONE && last.text[0] == '\0');

    handle("{\"type\":\"claude\",\"event\":\"explode\"}", true, reply);
    CHECK(strstr(reply, "bad_message"));
    CHECK(handle("garbage", true, reply) == PC_LINK_OK);   // authed: report, don't drop
}

static void test_metrics_partial(void)
{
    char reply[128];
    setup();
    handle("{\"type\":\"metrics\",\"cpu\":37.5,\"ram\":61}", true, reply);
    event_bus_dispatch();
    CHECK(last.id == EV_PC_METRICS);
    pc_metrics_t m;
    pc_metrics_get(&m);
    CHECK(m.valid && NEAR(m.cpu, 37.5f) && NEAR(m.ram, 61.0f));
    CHECK(isnan(m.gpu) && isnan(m.cpu_temp) && isnan(m.gpu_temp));
    CHECK(isnan(pc_metrics_max_temp(&m)) && !pc_metrics_hot(&m) && !pc_metrics_busy(&m));

    handle("{\"type\":\"metrics\",\"cpu\":90,\"gpu_temp\":83}", true, reply);
    pc_metrics_get(&m);
    CHECK(NEAR(pc_metrics_max_temp(&m), 83.0f) && pc_metrics_hot(&m) && pc_metrics_busy(&m));

    pc_metrics_clear();
    pc_metrics_get(&m);
    CHECK(!m.valid && !pc_metrics_hot(&m));
}

static void test_pc_sleep_and_ping(void)
{
    char reply[128];
    setup();
    handle("{\"type\":\"pc\",\"state\":\"sleep\"}", true, reply);
    handle("{\"type\":\"ping\"}", true, reply);
    CHECK(reply[0] == '\0');
    event_bus_dispatch();
    CHECK(count == 1 && last.id == EV_PC_SLEEP);
}

static void test_token_generated_once(void)
{
    stored_len = 0;
    char a[PC_LINK_TOKEN_LEN], b[PC_LINK_TOKEN_LEN];
    pc_link_token(a);
    CHECK(strlen(a) == 9 && a[4] == '-');
    pc_link_token(b);
    CHECK(strcmp(a, b) == 0);   // persisted, not regenerated
    memcpy(stored, "bad", 4);   // corrupted storage -> new valid token
    pc_link_token(b);
    CHECK(strlen(b) == 9 && b[4] == '-');
}

#define HELLO "{\"type\":\"hello\",\"version\":1,\"token\":\"ABCD-2345\"}"

// Delivers everything queued; returns how many events arrived.
static int drain(void)
{
    count = 0;
    while (event_bus_dispatch()) {
    }
    return count;
}

static void test_session_lifecycle(void)
{
    char reply[128];
    pc_session_t s;
    setup();
    pc_session_init(&s, TOKEN);

    pc_session_action_t a = pc_session_frame(&s, 5, "{\"type\":\"ping\"}", 0, reply, sizeof(reply));
    CHECK(a.close_this);   // not authenticated

    a = pc_session_frame(&s, 5, HELLO, 1000, reply, sizeof(reply));
    CHECK(!a.close_this && a.close_other == -1 && s.authed_conn == 5);
    CHECK(drain() == 1 && last.id == EV_PC_CONNECTED && last.value == 1);

    // Activity keeps it alive; silence expires it.
    pc_session_frame(&s, 5, "{\"type\":\"ping\"}", 40000, reply, sizeof(reply));
    CHECK(pc_session_expired(&s, 40000 + PC_LINK_TIMEOUT_MS) == -1);
    CHECK(pc_session_expired(&s, 40001 + PC_LINK_TIMEOUT_MS) == 5);

    // A second agent replaces the first.
    a = pc_session_frame(&s, 9, HELLO, 50000, reply, sizeof(reply));
    CHECK(a.close_other == 5 && s.authed_conn == 9);
    CHECK(drain() == 0);           // still connected, no flicker
    pc_session_closed(&s, 5);                  // old socket closing is a no-op
    CHECK(drain() == 0);

    pc_session_closed(&s, 9);
    CHECK(s.authed_conn == -1);
    CHECK(drain() == 1 && last.id == EV_PC_CONNECTED && last.value == 0);
    CHECK(pc_session_expired(&s, 999999) == -1);
}

int main(void)
{
    test_session_lifecycle();
    test_hello();
    test_requires_auth();
    test_claude_events();
    test_metrics_partial();
    test_pc_sleep_and_ping();
    test_token_generated_once();
    puts("pc_link: all tests passed");
    return 0;
}
