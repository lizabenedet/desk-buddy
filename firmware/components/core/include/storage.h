#pragma once

#include <stdbool.h>
#include <stddef.h>

// Small persistent blobs (pet meters, alarm). NVS on the board, files in
// the simulator. Keys: at most 15 characters.
bool storage_load(const char *key, void *buf, size_t len);
bool storage_save(const char *key, const void *buf, size_t len);
