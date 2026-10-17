#include "pet.h"

// Per hour.
#define ENERGY_DRAIN        5.0f
#define ENERGY_DRAIN_BUSY   12.0f
#define ENERGY_RECOVER      15.0f
#define HAPPINESS_DECAY     3.0f

#define PETTED_BONUS        8.0f
#define POKED_BONUS         1.0f
#define GOOD_NEWS_BONUS     5.0f

#define MS_PER_HOUR         3600000.0f

static float clamp(float v)
{
    return v < 0.0f ? 0.0f : (v > 100.0f ? 100.0f : v);
}

void pet_init(pet_t *p)
{
    p->energy = 80.0f;
    p->happiness = 70.0f;
}

bool pet_is_sleep_time(int hour)
{
    return hour >= 0 && (hour >= PET_SLEEP_START_HOUR || hour < PET_SLEEP_END_HOUR);
}

void pet_update(pet_t *p, uint32_t dt_ms, int hour, bool pc_busy)
{
    float hours = (float)dt_ms / MS_PER_HOUR;
    if (pet_is_sleep_time(hour)) {
        p->energy = clamp(p->energy + ENERGY_RECOVER * hours);
    } else {
        p->energy = clamp(p->energy - (pc_busy ? ENERGY_DRAIN_BUSY : ENERGY_DRAIN) * hours);
    }
    p->happiness = clamp(p->happiness - HAPPINESS_DECAY * hours);
}

void pet_on_petted(pet_t *p)
{
    p->happiness = clamp(p->happiness + PETTED_BONUS);
}

void pet_on_poked(pet_t *p)
{
    p->happiness = clamp(p->happiness + POKED_BONUS);
}

void pet_on_good_news(pet_t *p)
{
    p->happiness = clamp(p->happiness + GOOD_NEWS_BONUS);
}

mood_t pet_resting_mood(const pet_t *p, int hour)
{
    if (pet_is_sleep_time(hour)) {
        return MOOD_ASLEEP;
    }
    if (p->energy < 25.0f) {
        return MOOD_TIRED;
    }
    if (p->energy < 45.0f) {
        return MOOD_SLEEPY;
    }
    if (p->happiness < 30.0f) {
        return MOOD_SAD;
    }
    if (p->happiness > 85.0f) {
        return MOOD_HAPPY;
    }
    return MOOD_NEUTRAL;
}
