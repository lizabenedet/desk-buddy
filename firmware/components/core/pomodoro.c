#include "pomodoro.h"

void pomodoro_init(pomodoro_t *p, uint32_t focus_ms, uint32_t break_ms)
{
    *p = (pomodoro_t){.phase = POMO_IDLE, .focus_ms = focus_ms, .break_ms = break_ms};
}

void pomodoro_start(pomodoro_t *p)
{
    p->phase = POMO_FOCUS;
    p->left_ms = p->focus_ms;
}

void pomodoro_stop(pomodoro_t *p)
{
    p->phase = POMO_IDLE;
    p->left_ms = 0;
}

bool pomodoro_update(pomodoro_t *p, uint32_t dt_ms)
{
    if (p->phase == POMO_IDLE) {
        return false;
    }
    if (dt_ms < p->left_ms) {
        p->left_ms -= dt_ms;
        return false;
    }
    // Carry the overshoot into the next phase so the cycle doesn't drift.
    uint32_t over = dt_ms - p->left_ms;
    if (p->phase == POMO_FOCUS) {
        p->phase = POMO_BREAK;
        p->left_ms = p->break_ms;
    } else {
        p->phase = POMO_FOCUS;
        p->left_ms = p->focus_ms;
    }
    p->left_ms = over < p->left_ms ? p->left_ms - over : 0;
    return true;
}

uint32_t pomodoro_seconds_left(const pomodoro_t *p)
{
    return (p->left_ms + 999) / 1000;
}
