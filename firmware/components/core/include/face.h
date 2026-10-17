#pragma once

#include <stdbool.h>
#include <stdint.h>

// Procedural face state: moods are parameter presets, the face eases
// between them, blinks and glances around on its own. No drawing here.

typedef enum {
    MOOD_NEUTRAL,
    MOOD_HAPPY,
    MOOD_SAD,
    MOOD_ANGRY,
    MOOD_SURPRISED,
    MOOD_SLEEPY,
    MOOD_ASLEEP,
    MOOD_THINKING,
    MOOD_ALERT,     // waiting for the user
    MOOD_HOT,
    MOOD_TIRED,
    MOOD_COUNT,
} mood_t;

typedef struct {
    float eye_open;     // 0 closed .. 1 open
    float eye_scale;    // size multiplier
    float lid_top;      // 0 .. 1, top lid lowered (sleepy)
    float lid_angle;    // -1 sad (outer corners down) .. 1 angry (inner corners down)
    float eye_smile;    // 0 .. 1, lower lid pushed up into a ^ shape
    float look_x;       // -1 left .. 1 right
    float look_y;       // -1 up .. 1 down
    float mouth_curve;  // -1 frown .. 1 smile
    float mouth_open;   // 0 .. 1
    float mouth_width;  // 0 .. 1
    uint32_t color;     // 0xRRGGBB
} face_params_t;

typedef struct {
    face_params_t from;
    face_params_t current;
    face_params_t target;
    uint32_t transition_ms;
    uint32_t transition_elapsed;

    mood_t base_mood;
    mood_t shown_mood;
    uint32_t react_left_ms;     // 0 = not reacting

    uint32_t next_blink_ms;
    uint32_t blink_elapsed;     // < BLINK_MS while blinking
    bool blinking;

    uint32_t next_glance_ms;
    float glance_x;
    float glance_y;

    uint32_t rng;
} face_t;

void face_init(face_t *f, uint32_t seed);

// Resting mood, shown whenever no reaction is active.
void face_set_mood(face_t *f, mood_t mood);

// Temporary mood; returns to the resting mood after `duration_ms`.
void face_react(face_t *f, mood_t mood, uint32_t duration_ms);

void face_update(face_t *f, uint32_t dt_ms);

// What to draw right now: current params with blink and glance applied.
face_params_t face_output(const face_t *f);

const face_params_t *face_preset(mood_t mood);
face_params_t face_lerp(const face_params_t *a, const face_params_t *b, float t);
