#pragma once

#include <stdbool.h>
#include <stdint.h>

// Focus/break cycle that repeats until stopped.

typedef enum {
    POMO_IDLE,
    POMO_FOCUS,
    POMO_BREAK,
} pomodoro_phase_t;

typedef struct {
    pomodoro_phase_t phase;
    uint32_t left_ms;
    uint32_t focus_ms;
    uint32_t break_ms;
} pomodoro_t;

void pomodoro_init(pomodoro_t *p, uint32_t focus_ms, uint32_t break_ms);
void pomodoro_start(pomodoro_t *p);
void pomodoro_stop(pomodoro_t *p);

// Returns true when a phase just ended (and the next one started).
bool pomodoro_update(pomodoro_t *p, uint32_t dt_ms);

uint32_t pomodoro_seconds_left(const pomodoro_t *p);
