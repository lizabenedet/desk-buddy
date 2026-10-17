#include "event_bus.h"

#include <string.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
static portMUX_TYPE bus_mux = portMUX_INITIALIZER_UNLOCKED;
#define BUS_LOCK()   portENTER_CRITICAL(&bus_mux)
#define BUS_UNLOCK() portEXIT_CRITICAL(&bus_mux)
#else
// The simulator is single-threaded.
#define BUS_LOCK()
#define BUS_UNLOCK()
#endif

#define QUEUE_LEN       32
#define MAX_SUBSCRIBERS 32

typedef struct {
    event_id_t id;
    event_handler_t handler;
    void *ctx;
} subscriber_t;

static event_t queue[QUEUE_LEN];
static size_t head;
static size_t count;
static uint32_t dropped;

static subscriber_t subscribers[MAX_SUBSCRIBERS];
static size_t subscriber_count;

void event_bus_reset(void)
{
    BUS_LOCK();
    head = 0;
    count = 0;
    dropped = 0;
    BUS_UNLOCK();
    subscriber_count = 0;
}

bool event_bus_subscribe(event_id_t id, event_handler_t handler, void *ctx)
{
    if (!handler || id >= EV_COUNT || subscriber_count == MAX_SUBSCRIBERS) {
        return false;
    }
    subscribers[subscriber_count++] = (subscriber_t){id, handler, ctx};
    return true;
}

bool event_bus_publish(const event_t *ev)
{
    if (!ev || ev->id == EV_ANY || ev->id >= EV_COUNT) {
        return false;
    }
    bool ok;
    BUS_LOCK();
    if (count == QUEUE_LEN) {
        dropped++;
        ok = false;
    } else {
        queue[(head + count) % QUEUE_LEN] = *ev;
        count++;
        ok = true;
    }
    BUS_UNLOCK();
    return ok;
}

bool event_bus_publish_simple(event_id_t id, int32_t value)
{
    event_t ev = {.id = id, .value = value};
    return event_bus_publish(&ev);
}

static bool pop(event_t *out)
{
    bool ok = false;
    BUS_LOCK();
    if (count) {
        *out = queue[head];
        head = (head + 1) % QUEUE_LEN;
        count--;
        ok = true;
    }
    BUS_UNLOCK();
    return ok;
}

size_t event_bus_dispatch(void)
{
    // Bounded so handlers that publish can't starve the UI task.
    size_t delivered = 0;
    event_t ev;
    while (delivered < QUEUE_LEN && pop(&ev)) {
        for (size_t i = 0; i < subscriber_count; i++) {
            if (subscribers[i].id == EV_ANY || subscribers[i].id == ev.id) {
                subscribers[i].handler(&ev, subscribers[i].ctx);
            }
        }
        delivered++;
    }
    return delivered;
}

uint32_t event_bus_dropped(void)
{
    return dropped;
}
