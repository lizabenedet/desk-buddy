#pragma once

#include "event_bus.h"
#include "lvgl.h"
#include "settings.h"

// A feature of the buddy. All hooks are optional and run on the UI task.
// To add a feature: implement a module_t and list it in modules.c.
typedef struct {
    const char *name;
    void (*init)(void);
    void (*on_event)(const event_t *ev);
    // Builds the module's swipe page inside `parent` (a full-screen tile).
    void (*create_screen)(lv_obj_t *parent);
    // Fields on the buddy.local page; registered (and loaded) before init().
    const setting_def_t *settings;
    size_t settings_count;
} module_t;

extern const module_t *const app_modules[];
extern const size_t app_module_count;
