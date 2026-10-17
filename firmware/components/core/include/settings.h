#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// User settings declared by modules, persisted in storage and edited on the
// buddy.local page. Reads and writes are safe from any task; a change
// publishes EV_SETTINGS_CHANGED with the key in `text`.

#define SETTINGS_MAX        32
#define SETTING_TEXT_MAX    32

typedef enum {
    SETTING_INT,
    SETTING_TEXT,
} setting_type_t;

typedef struct {
    const char *key;        // storage key: at most 15 characters
    const char *group;      // section on the page, e.g. "Pomodoro"
    const char *label;
    setting_type_t type;
    int32_t min, max;       // SETTING_INT
    int32_t def_int;
    const char *def_text;   // SETTING_TEXT
} setting_def_t;

// Loads stored values (or defaults). `defs` must outlive the program.
void settings_register(const setting_def_t *defs, size_t count);

int32_t settings_get_int(const char *key);
void settings_get_text(const char *key, char *out, size_t len);

// {"settings":[{key, group, label, type, value, min?, max?}, ...]}; free() the result.
char *settings_to_json(void);

// Applies {"key": value, ...} all-or-nothing. On failure writes a message
// (Portuguese, for the page) to `err` and changes nothing.
bool settings_apply_json(const char *json, char *err, size_t err_len);

// For tests.
void settings_reset(void);
