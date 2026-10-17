#pragma once

#include "face.h"
#include "lvgl.h"

// Full-size object that animates and draws `face`. Only the eye and mouth
// areas that actually changed are invalidated, to keep SPI traffic low.
lv_obj_t *face_view_create(lv_obj_t *parent, face_t *face);
