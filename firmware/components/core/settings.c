#include "settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "event_bus.h"
#include "storage.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
static portMUX_TYPE settings_mux = portMUX_INITIALIZER_UNLOCKED;
#define LOCK()   portENTER_CRITICAL(&settings_mux)
#define UNLOCK() portEXIT_CRITICAL(&settings_mux)
#else
#define LOCK()
#define UNLOCK()
#endif

typedef struct {
    const setting_def_t *def;
    int32_t ival;
    char text[SETTING_TEXT_MAX];
} slot_t;

static slot_t slots[SETTINGS_MAX];
static size_t slot_count;

static slot_t *find(const char *key)
{
    for (size_t i = 0; i < slot_count; i++) {
        if (strcmp(slots[i].def->key, key) == 0) {
            return &slots[i];
        }
    }
    return NULL;
}

void settings_reset(void)
{
    slot_count = 0;
}

void settings_register(const setting_def_t *defs, size_t count)
{
    for (size_t i = 0; i < count && slot_count < SETTINGS_MAX; i++) {
        const setting_def_t *d = &defs[i];
        slot_t s = {.def = d, .ival = d->def_int};
        snprintf(s.text, sizeof(s.text), "%s", d->def_text ? d->def_text : "");
        if (d->type == SETTING_INT) {
            int32_t v;
            if (storage_load(d->key, &v, sizeof(v)) && v >= d->min && v <= d->max) {
                s.ival = v;
            }
        } else {
            char t[SETTING_TEXT_MAX];
            if (storage_load(d->key, t, sizeof(t)) && memchr(t, '\0', sizeof(t))) {
                memcpy(s.text, t, sizeof(t));
            }
        }
        LOCK();
        slots[slot_count++] = s;
        UNLOCK();
    }
}

int32_t settings_get_int(const char *key)
{
    LOCK();
    slot_t *s = find(key);
    int32_t v = s ? s->ival : 0;
    UNLOCK();
    return v;
}

void settings_get_text(const char *key, char *out, size_t len)
{
    char copy[SETTING_TEXT_MAX] = "";
    LOCK();
    slot_t *s = find(key);
    if (s) {
        memcpy(copy, s->text, sizeof(copy));
    }
    UNLOCK();
    snprintf(out, len, "%s", copy);
}

char *settings_to_json(void)
{
    slot_t snapshot[SETTINGS_MAX];
    LOCK();
    size_t n = slot_count;
    memcpy(snapshot, slots, n * sizeof(slot_t));
    UNLOCK();

    cJSON *root = cJSON_CreateObject();
    cJSON *list = cJSON_AddArrayToObject(root, "settings");
    for (size_t i = 0; i < n; i++) {
        const setting_def_t *d = snapshot[i].def;
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "key", d->key);
        cJSON_AddStringToObject(item, "group", d->group);
        cJSON_AddStringToObject(item, "label", d->label);
        if (d->type == SETTING_INT) {
            cJSON_AddStringToObject(item, "type", "int");
            cJSON_AddNumberToObject(item, "value", snapshot[i].ival);
            cJSON_AddNumberToObject(item, "min", d->min);
            cJSON_AddNumberToObject(item, "max", d->max);
        } else {
            cJSON_AddStringToObject(item, "type", "text");
            cJSON_AddStringToObject(item, "value", snapshot[i].text);
        }
        cJSON_AddItemToArray(list, item);
    }
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

static bool validate(const cJSON *item, const setting_def_t *d, char *err, size_t err_len)
{
    if (d->type == SETTING_INT) {
        if (!cJSON_IsNumber(item) || item->valuedouble != (double)(int32_t)item->valuedouble) {
            snprintf(err, err_len, "%s: precisa ser um número inteiro", d->label);
            return false;
        }
        if (item->valueint < d->min || item->valueint > d->max) {
            snprintf(err, err_len, "%s: entre %d e %d", d->label, (int)d->min, (int)d->max);
            return false;
        }
    } else if (!cJSON_IsString(item) || strlen(item->valuestring) >= SETTING_TEXT_MAX) {
        snprintf(err, err_len, "%s: texto de até %d caracteres", d->label, SETTING_TEXT_MAX - 1);
        return false;
    }
    return true;
}

bool settings_apply_json(const char *json, char *err, size_t err_len)
{
    cJSON *root = cJSON_Parse(json);
    if (!cJSON_IsObject(root)) {
        snprintf(err, err_len, "JSON inválido");
        cJSON_Delete(root);
        return false;
    }
    // Validate everything first so a bad field doesn't leave a half-applied change.
    const cJSON *item;
    cJSON_ArrayForEach(item, root) {
        slot_t *s = find(item->string);
        if (!s) {
            snprintf(err, err_len, "configuração desconhecida: %s", item->string);
            cJSON_Delete(root);
            return false;
        }
        if (!validate(item, s->def, err, err_len)) {
            cJSON_Delete(root);
            return false;
        }
    }
    cJSON_ArrayForEach(item, root) {
        slot_t *s = find(item->string);
        const setting_def_t *d = s->def;
        bool changed;
        LOCK();
        if (d->type == SETTING_INT) {
            changed = s->ival != item->valueint;
            s->ival = item->valueint;
        } else {
            changed = strcmp(s->text, item->valuestring) != 0;
            memset(s->text, 0, sizeof(s->text));
            strcpy(s->text, item->valuestring);
        }
        UNLOCK();
        if (!changed) {
            continue;
        }
        if (d->type == SETTING_INT) {
            storage_save(d->key, &s->ival, sizeof(s->ival));
        } else {
            storage_save(d->key, s->text, sizeof(s->text));
        }
        event_bus_publish_text(EV_SETTINGS_CHANGED, d->key, 0);
    }
    cJSON_Delete(root);
    return true;
}
