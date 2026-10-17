#include <string.h>

#include "check.h"
#include "event_bus.h"

static int calls;
static int32_t last_value;

static void count_handler(const event_t *ev, void *ctx)
{
    (void)ctx;
    calls++;
    last_value = ev->value;
}

static void republish_handler(const event_t *ev, void *ctx)
{
    (void)ctx;
    event_bus_publish(ev);
}

static void setup(void)
{
    event_bus_reset();
    calls = 0;
    last_value = 0;
}

static void test_delivers_to_matching_subscriber(void)
{
    setup();
    CHECK(event_bus_subscribe(EV_FACE_TOUCHED, count_handler, NULL));
    CHECK(event_bus_publish_simple(EV_FACE_TOUCHED, 7));
    CHECK(calls == 0); // nothing until dispatch
    CHECK(event_bus_dispatch() == 1);
    CHECK(calls == 1);
    CHECK(last_value == 7);
}

static void test_any_receives_everything(void)
{
    setup();
    CHECK(event_bus_subscribe(EV_ANY, count_handler, NULL));
    event_bus_publish_simple(EV_FACE_TOUCHED, 1);
    event_bus_publish_simple(EV_FACE_TOUCHED, 2);
    event_bus_dispatch();
    CHECK(calls == 2);
    CHECK(last_value == 2);
}

static void test_rejects_invalid_ids(void)
{
    setup();
    CHECK(!event_bus_publish_simple(EV_ANY, 0));
    CHECK(!event_bus_publish_simple(EV_COUNT, 0));
    CHECK(!event_bus_subscribe(EV_COUNT, count_handler, NULL));
    CHECK(!event_bus_subscribe(EV_FACE_TOUCHED, NULL, NULL));
}

static void test_full_queue_drops_and_counts(void)
{
    setup();
    int accepted = 0;
    for (int i = 0; i < 40; i++) {
        accepted += event_bus_publish_simple(EV_FACE_TOUCHED, i);
    }
    CHECK(accepted == 32);
    CHECK(event_bus_dropped() == 8);
}

static void test_republishing_handler_does_not_loop_forever(void)
{
    setup();
    event_bus_subscribe(EV_FACE_TOUCHED, republish_handler, NULL);
    event_bus_publish_simple(EV_FACE_TOUCHED, 0);
    CHECK(event_bus_dispatch() == 32);
    CHECK(event_bus_dispatch() == 32); // still pending, but bounded per call
}

static char last_text[EVENT_TEXT_MAX];

static void text_handler(const event_t *ev, void *ctx)
{
    (void)ctx;
    strcpy(last_text, ev->text);
}

static void test_publish_text_truncates(void)
{
    setup();
    event_bus_subscribe(EV_SAY, text_handler, NULL);
    char long_text[100];
    memset(long_text, 'a', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    CHECK(event_bus_publish_text(EV_SAY, long_text, 0));
    event_bus_dispatch();
    CHECK(strlen(last_text) == EVENT_TEXT_MAX - 1);
}

int main(void)
{
    test_publish_text_truncates();
    test_delivers_to_matching_subscriber();
    test_any_receives_everything();
    test_rejects_invalid_ids();
    test_full_queue_drops_and_counts();
    test_republishing_handler_does_not_loop_forever();
    puts("event_bus: all tests passed");
    return 0;
}
