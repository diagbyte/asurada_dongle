#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

#include "asurada_trackball.h"

LOG_MODULE_REGISTER(asurada_tb, LOG_LEVEL_INF);

/* Accumulated on the input driver thread, drained on the display work queue. */
static atomic_t acc_x = ATOMIC_INIT(0);
static atomic_t acc_y = ATOMIC_INIT(0);
static atomic_t moved = ATOMIC_INIT(0);

static void tb_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);
    if (evt->type != INPUT_EV_REL) {
        return;
    }
    if (evt->code == INPUT_REL_X) {
        atomic_add(&acc_x, evt->value);
    } else if (evt->code == INPUT_REL_Y) {
        atomic_add(&acc_y, evt->value);
    } else {
        return;
    }
    atomic_set(&moved, 1);
#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL_LOG)
    LOG_INF("trackball REL code=%u val=%d", (unsigned)evt->code, evt->value);
#endif
}

/* dev = NULL taps every input device; we filter on INPUT_EV_REL above so the
 * CST816S touch (INPUT_EV_ABS) is ignored. */
INPUT_CALLBACK_DEFINE(NULL, tb_cb, NULL);

bool asurada_trackball_fetch(int32_t *dx, int32_t *dy) {
    if (!atomic_get(&moved)) {
        *dx = 0;
        *dy = 0;
        return false;
    }
    *dx = (int32_t)atomic_set(&acc_x, 0);
    *dy = (int32_t)atomic_set(&acc_y, 0);
    atomic_set(&moved, 0);
    return true;
}
