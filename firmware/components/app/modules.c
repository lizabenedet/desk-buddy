#include "module.h"

extern const module_t clock_module;

// Order here is the swipe order, right of the home screen.
const module_t *const app_modules[] = {
    &clock_module,
};

const size_t app_module_count = sizeof(app_modules) / sizeof(app_modules[0]);
