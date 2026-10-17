#pragma once

#include "lvgl.h"

// Home screen: the face plus its speech bubble. Listens for EV_FACE_MOOD,
// EV_FACE_REACT and EV_SAY; publishes EV_FACE_TOUCHED.
void home_create(lv_obj_t *parent);
