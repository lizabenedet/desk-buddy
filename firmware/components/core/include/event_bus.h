#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Events are published from any task and delivered later, on the UI task,
// by event_bus_dispatch(). Handlers may therefore touch LVGL directly.

typedef enum {
    EV_ANY = 0,         // subscribe-only: receive every event
    EV_FACE_TOUCHED,    // value: touch_kind_t
    EV_FACE_MOOD,       // value: mood_t, the resting mood
    EV_FACE_REACT,      // value: mood_t, arg: duration in ms
    EV_SAY,             // text: what to say, arg: duration in ms
    EV_ALERT,           // text: message; stays until the user touches the screen
    EV_ALERT_DISMISSED,
    EV_WIFI,            // value: 1 connected, 0 disconnected
    EV_TIME_SYNCED,
    EV_WEATHER,         // new report in weather_store_get()
    EV_TIMER,           // value: seconds left, arg: pomodoro_phase_t (POMO_IDLE when stopped)
    EV_COUNT,
} event_id_t;

typedef enum {
    TOUCH_TAP,          // quick poke
    TOUCH_HOLD,         // long press, petting
} touch_kind_t;

#define EVENT_TEXT_MAX 96

typedef struct {
    event_id_t id;
    int32_t value;
    int32_t arg;
    char text[EVENT_TEXT_MAX];
} event_t;

typedef void (*event_handler_t)(const event_t *ev, void *ctx);

void event_bus_reset(void);
bool event_bus_subscribe(event_id_t id, event_handler_t handler, void *ctx);

// Copies the event into the queue. Returns false (and drops it) when full.
bool event_bus_publish(const event_t *ev);
bool event_bus_publish_simple(event_id_t id, int32_t value);
// Truncates `text` to fit.
bool event_bus_publish_text(event_id_t id, const char *text, int32_t arg);

// Delivers queued events to subscribers. Returns how many were delivered.
size_t event_bus_dispatch(void);

uint32_t event_bus_dropped(void);
