#include <stdio.h>
#include <stdlib.h>

#include "event_bus.h"

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                    #cond);                                                  \
            exit(1);                                                         \
        }                                                                    \
    } while (0)

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

int main(void)
{
    test_delivers_to_matching_subscriber();
    test_any_receives_everything();
    test_rejects_invalid_ids();
    test_full_queue_drops_and_counts();
    test_republishing_handler_does_not_loop_forever();
    puts("event_bus: all tests passed");
    return 0;
}
