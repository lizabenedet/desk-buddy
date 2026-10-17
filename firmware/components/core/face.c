#include "face.h"

#include <stddef.h>

#define TRANSITION_MS   300
#define BLINK_MS        160
#define BLINK_MIN_MS    2000
#define BLINK_MAX_MS    6000
#define GLANCE_MIN_MS   1500
#define GLANCE_MAX_MS   5000
#define GLANCE_RANGE    0.35f

static const face_params_t presets[MOOD_COUNT] = {
    //                 open scale  lid_t  angle  smile  lk_x   lk_y   curve  m_open m_w    color
    [MOOD_NEUTRAL]   = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.5f, 0x40E0FF},
    [MOOD_HAPPY]     = {1.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.7f, 0x50FFB0},
    [MOOD_SAD]       = {0.8f, 0.95f, 0.0f, -0.6f, 0.0f, 0.0f, 0.4f, -0.7f, 0.0f, 0.45f, 0x4080FF},
    [MOOD_ANGRY]     = {0.9f, 1.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.4f, 0xFF4040},
    [MOOD_SURPRISED] = {1.0f, 1.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.3f, 0x80F0FF},
    [MOOD_SLEEPY]    = {1.0f, 1.0f, 0.55f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.3f, 0x2090B0},
    [MOOD_ASLEEP]    = {0.06f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.15f, 0.2f, 0x1870A0},
    [MOOD_THINKING]  = {1.0f, 0.95f, 0.25f, 0.0f, 0.0f, 0.6f, -0.6f, 0.0f, 0.0f, 0.3f, 0xA080FF},
    [MOOD_ALERT]     = {1.0f, 1.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.35f, 0.35f, 0xFFC000},
    [MOOD_HOT]       = {1.0f, 1.0f, 0.3f, -0.2f, 0.0f, 0.0f, 0.1f, -0.2f, 0.45f, 0.4f, 0xFF6020},
    [MOOD_TIRED]     = {1.0f, 0.95f, 0.45f, -0.25f, 0.0f, 0.0f, 0.2f, -0.3f, 0.0f, 0.4f, 0x6090A0},
};

static uint32_t next_rand(face_t *f)
{
    // xorshift32
    uint32_t x = f->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    f->rng = x;
    return x;
}

static uint32_t rand_range(face_t *f, uint32_t lo, uint32_t hi)
{
    return lo + next_rand(f) % (hi - lo + 1);
}

static float rand_unit(face_t *f)
{
    return (float)(next_rand(f) % 2001) / 1000.0f - 1.0f;
}

static float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static uint32_t lerp_color(uint32_t a, uint32_t b, float t)
{
    uint32_t out = 0;
    for (int shift = 0; shift <= 16; shift += 8) {
        float ca = (float)((a >> shift) & 0xFF);
        float cb = (float)((b >> shift) & 0xFF);
        out |= ((uint32_t)(lerpf(ca, cb, t) + 0.5f) & 0xFF) << shift;
    }
    return out;
}

static float ease_in_out(float t)
{
    return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
}

static bool idles(mood_t m)
{
    return m != MOOD_ASLEEP;
}

static bool glances(mood_t m)
{
    return m != MOOD_ASLEEP && m != MOOD_THINKING;
}

const face_params_t *face_preset(mood_t mood)
{
    return &presets[mood < MOOD_COUNT ? mood : MOOD_NEUTRAL];
}

face_params_t face_lerp(const face_params_t *a, const face_params_t *b, float t)
{
    return (face_params_t){
        .eye_open = lerpf(a->eye_open, b->eye_open, t),
        .eye_scale = lerpf(a->eye_scale, b->eye_scale, t),
        .lid_top = lerpf(a->lid_top, b->lid_top, t),
        .lid_angle = lerpf(a->lid_angle, b->lid_angle, t),
        .eye_smile = lerpf(a->eye_smile, b->eye_smile, t),
        .look_x = lerpf(a->look_x, b->look_x, t),
        .look_y = lerpf(a->look_y, b->look_y, t),
        .mouth_curve = lerpf(a->mouth_curve, b->mouth_curve, t),
        .mouth_open = lerpf(a->mouth_open, b->mouth_open, t),
        .mouth_width = lerpf(a->mouth_width, b->mouth_width, t),
        .color = lerp_color(a->color, b->color, t),
    };
}

static void show(face_t *f, mood_t mood)
{
    if (mood == f->shown_mood) {
        return;
    }
    f->shown_mood = mood;
    f->from = f->current;
    f->target = *face_preset(mood);
    f->transition_ms = TRANSITION_MS;
    f->transition_elapsed = 0;
}

void face_init(face_t *f, uint32_t seed)
{
    *f = (face_t){0};
    f->rng = seed ? seed : 0x9E3779B9u;
    f->base_mood = MOOD_NEUTRAL;
    f->shown_mood = MOOD_NEUTRAL;
    f->current = f->from = f->target = *face_preset(MOOD_NEUTRAL);
    f->next_blink_ms = rand_range(f, BLINK_MIN_MS, BLINK_MAX_MS);
    f->next_glance_ms = rand_range(f, GLANCE_MIN_MS, GLANCE_MAX_MS);
}

void face_set_mood(face_t *f, mood_t mood)
{
    if (mood >= MOOD_COUNT) {
        return;
    }
    f->base_mood = mood;
    if (!f->react_left_ms) {
        show(f, mood);
    }
}

void face_react(face_t *f, mood_t mood, uint32_t duration_ms)
{
    if (mood >= MOOD_COUNT || duration_ms == 0) {
        return;
    }
    f->react_left_ms = duration_ms;
    show(f, mood);
}

static void update_reaction(face_t *f, uint32_t dt)
{
    if (!f->react_left_ms) {
        return;
    }
    if (dt >= f->react_left_ms) {
        f->react_left_ms = 0;
        show(f, f->base_mood);
    } else {
        f->react_left_ms -= dt;
    }
}

static void update_transition(face_t *f, uint32_t dt)
{
    if (f->transition_elapsed >= f->transition_ms) {
        f->current = f->target;
        return;
    }
    f->transition_elapsed += dt;
    float t = (float)f->transition_elapsed / (float)f->transition_ms;
    f->current = face_lerp(&f->from, &f->target, ease_in_out(t > 1.0f ? 1.0f : t));
}

static void update_blink(face_t *f, uint32_t dt)
{
    if (f->blinking) {
        f->blink_elapsed += dt;
        if (f->blink_elapsed >= BLINK_MS) {
            f->blinking = false;
            f->next_blink_ms = rand_range(f, BLINK_MIN_MS, BLINK_MAX_MS);
        }
        return;
    }
    if (!idles(f->shown_mood)) {
        return;
    }
    if (dt >= f->next_blink_ms) {
        f->blinking = true;
        f->blink_elapsed = 0;
    } else {
        f->next_blink_ms -= dt;
    }
}

static void update_glance(face_t *f, uint32_t dt)
{
    if (!glances(f->shown_mood)) {
        f->glance_x = f->glance_y = 0.0f;
        return;
    }
    if (dt < f->next_glance_ms) {
        f->next_glance_ms -= dt;
        return;
    }
    f->next_glance_ms = rand_range(f, GLANCE_MIN_MS, GLANCE_MAX_MS);
    // Look back at the user about half the time.
    if (next_rand(f) % 2) {
        f->glance_x = f->glance_y = 0.0f;
    } else {
        f->glance_x = rand_unit(f) * GLANCE_RANGE;
        f->glance_y = rand_unit(f) * GLANCE_RANGE * 0.5f;
    }
}

void face_update(face_t *f, uint32_t dt_ms)
{
    update_reaction(f, dt_ms);
    update_transition(f, dt_ms);
    update_blink(f, dt_ms);
    update_glance(f, dt_ms);
}

face_params_t face_output(const face_t *f)
{
    face_params_t out = f->current;
    if (f->blinking) {
        // Close then reopen: 1 -> 0 -> 1 over BLINK_MS.
        float t = (float)f->blink_elapsed / (float)BLINK_MS;
        float closed = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
        out.eye_open *= 1.0f - closed;
    }
    out.look_x += f->glance_x;
    out.look_y += f->glance_y;
    return out;
}
