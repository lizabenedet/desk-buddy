#include "module.h"

extern const module_t clock_module;
extern const module_t weather_module;
extern const module_t timer_module;
extern const module_t alarm_module;
extern const module_t pet_module;
extern const module_t pc_module;
extern const module_t claude_module;
extern const module_t system_module;

// Order here is the swipe order, right of the home screen (modules without
// a screen don't add a page).
const module_t *const app_modules[] = {
    &clock_module,
    &weather_module,
    &timer_module,
    &alarm_module,
    &pet_module,
    &pc_module,
    &claude_module,
    &system_module,
};

const size_t app_module_count = sizeof(app_modules) / sizeof(app_modules[0]);
