// storage.h for the simulator: one file per key next to the executable.

#include <stdio.h>

#include "storage.h"

static void path_for(const char *key, char *out, size_t len)
{
    snprintf(out, len, "storage_%s.bin", key);
}

bool storage_load(const char *key, void *buf, size_t len)
{
    char path[64];
    path_for(key, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    bool ok = fread(buf, 1, len, f) == len;
    fclose(f);
    return ok;
}

bool storage_save(const char *key, const void *buf, size_t len)
{
    char path[64];
    path_for(key, path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    bool ok = fwrite(buf, 1, len, f) == len;
    fclose(f);
    return ok;
}
