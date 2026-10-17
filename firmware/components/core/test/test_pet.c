#include "check.h"
#include "pet.h"

#define HOUR_MS 3600000u

static void test_drains_by_day_recovers_at_night(void)
{
    pet_t p;
    pet_init(&p);
    float start = p.energy;
    pet_update(&p, 2 * HOUR_MS, 14, false);
    CHECK(NEAR(p.energy, start - 10.0f));
    pet_update(&p, 2 * HOUR_MS, 2, false);
    CHECK(NEAR(p.energy, start + 20.0f));
}

static void test_busy_pc_drains_faster(void)
{
    pet_t calm, busy;
    pet_init(&calm);
    pet_init(&busy);
    pet_update(&calm, HOUR_MS, 14, false);
    pet_update(&busy, HOUR_MS, 14, true);
    CHECK(busy.energy < calm.energy);
}

static void test_meters_clamped(void)
{
    pet_t p;
    pet_init(&p);
    pet_update(&p, 100 * HOUR_MS, 14, true);
    CHECK(p.energy == 0.0f && p.happiness == 0.0f);
    for (int i = 0; i < 50; i++) {
        pet_on_petted(&p);
    }
    CHECK(p.happiness == 100.0f);
}

static void test_unknown_hour_never_sleeps(void)
{
    CHECK(!pet_is_sleep_time(-1));
    CHECK(pet_is_sleep_time(23) && pet_is_sleep_time(0) && pet_is_sleep_time(6));
    CHECK(!pet_is_sleep_time(7) && !pet_is_sleep_time(22));
}

static void test_resting_mood(void)
{
    pet_t p = {.energy = 80, .happiness = 70};
    CHECK(pet_resting_mood(&p, 14) == MOOD_NEUTRAL);
    CHECK(pet_resting_mood(&p, 1) == MOOD_ASLEEP);
    p.energy = 40;
    CHECK(pet_resting_mood(&p, 14) == MOOD_SLEEPY);
    p.energy = 10;
    CHECK(pet_resting_mood(&p, 14) == MOOD_TIRED);
    p.energy = 80;
    p.happiness = 20;
    CHECK(pet_resting_mood(&p, 14) == MOOD_SAD);
    p.happiness = 90;
    CHECK(pet_resting_mood(&p, 14) == MOOD_HAPPY);
}

int main(void)
{
    test_drains_by_day_recovers_at_night();
    test_busy_pc_drains_faster();
    test_meters_clamped();
    test_unknown_hour_never_sleeps();
    test_resting_mood();
    puts("pet: all tests passed");
    return 0;
}
