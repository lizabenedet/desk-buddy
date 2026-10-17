#include "check.h"
#include "pomodoro.h"

static void test_idle_does_nothing(void)
{
    pomodoro_t p;
    pomodoro_init(&p, 1000, 500);
    CHECK(!pomodoro_update(&p, 5000));
    CHECK(p.phase == POMO_IDLE && pomodoro_seconds_left(&p) == 0);
}

static void test_cycles_focus_break_focus(void)
{
    pomodoro_t p;
    pomodoro_init(&p, 1000, 500);
    pomodoro_start(&p);
    CHECK(p.phase == POMO_FOCUS && pomodoro_seconds_left(&p) == 1);
    CHECK(!pomodoro_update(&p, 999));
    CHECK(pomodoro_update(&p, 1));
    CHECK(p.phase == POMO_BREAK && p.left_ms == 500);
    CHECK(pomodoro_update(&p, 500));
    CHECK(p.phase == POMO_FOCUS && p.left_ms == 1000);
}

static void test_overshoot_carries_over(void)
{
    pomodoro_t p;
    pomodoro_init(&p, 1000, 500);
    pomodoro_start(&p);
    CHECK(pomodoro_update(&p, 1200));
    CHECK(p.phase == POMO_BREAK && p.left_ms == 300);
}

static void test_seconds_round_up(void)
{
    pomodoro_t p;
    pomodoro_init(&p, 25 * 60 * 1000, 5 * 60 * 1000);
    pomodoro_start(&p);
    pomodoro_update(&p, 1);
    CHECK(pomodoro_seconds_left(&p) == 25 * 60);
}

static void test_stop(void)
{
    pomodoro_t p;
    pomodoro_init(&p, 1000, 500);
    pomodoro_start(&p);
    pomodoro_stop(&p);
    CHECK(p.phase == POMO_IDLE && !pomodoro_update(&p, 2000));
}

int main(void)
{
    test_idle_does_nothing();
    test_cycles_focus_break_focus();
    test_overshoot_carries_over();
    test_seconds_round_up();
    test_stop();
    puts("pomodoro: all tests passed");
    return 0;
}
