#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "face.h"

// Pet meters, 0..100. Nothing bad ever happens at 0: low meters only change
// the resting mood.

typedef struct {
    float energy;
    float happiness;
} pet_t;

#define PET_SLEEP_START_HOUR 23
#define PET_SLEEP_END_HOUR   7

void pet_init(pet_t *p);

// `hour` is local time (0..23), or -1 if the clock isn't set yet.
void pet_update(pet_t *p, uint32_t dt_ms, int hour, bool pc_busy);

void pet_on_petted(pet_t *p);
void pet_on_poked(pet_t *p);
void pet_on_good_news(pet_t *p);   // e.g. Claude finished a task

bool pet_is_sleep_time(int hour);
mood_t pet_resting_mood(const pet_t *p, int hour);
