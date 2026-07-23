#pragma once
#include <lvgl.h>

/*
 * Low-battery warning pulse. When `critical` is true, the given battery body's
 * border opacity is animated to fade in/out (a slow blink) to draw the eye; when
 * false, the pulse is stopped and the border restored to fully opaque. Idempotent
 * -- safe to call every render; it only (re)starts the animation on the
 * transition into the critical state.
 */
void asurada_batt_pulse_set(lv_obj_t *body, bool critical);
