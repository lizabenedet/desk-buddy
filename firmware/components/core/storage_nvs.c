// storage.h on NVS (board only; the simulator has its own file-based one).
// nvs_flash_init() must have run (main does it).

#ifdef ESP_PLATFORM

#include "nvs.h"
#include "storage.h"

#define NAMESPACE "buddy"

bool storage_load(const char *key, void *buf, size_t len)
{
    nvs_handle_t h;
    if (nvs_open(NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t size = len;
    bool ok = nvs_get_blob(h, key, buf, &size) == ESP_OK && size == len;
    nvs_close(h);
    return ok;
}

bool storage_save(const char *key, const void *buf, size_t len)
{
    nvs_handle_t h;
    if (nvs_open(NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    bool ok = nvs_set_blob(h, key, buf, len) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

#endif
