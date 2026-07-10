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

/* In SCROLL mode the dongle input-listener rewrites the trackball's REL_X/Y into
 * REL_HWHEEL/WHEEL (scaled 1/10 by zip_xy_scaler 1 10), so the ball would freeze
 * while scrolling. Fold the wheel deltas back into the rotation accumulators,
 * x10 to undo that scaler, so the ball keeps rolling. Handling both X/Y and the
 * wheel is order-independent: whichever form reaches this global callback spins
 * the ball. (SNIPE keeps REL_X/Y, just 1/5-scaled, so it already rolls.) */
#define SCROLL_BALL_GAIN 10

static void tb_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);
    if (evt->type != INPUT_EV_REL) {
        return;
    }
    switch (evt->code) {
    case INPUT_REL_X:
        atomic_add(&acc_x, evt->value);
        break;
    case INPUT_REL_Y:
        atomic_add(&acc_y, evt->value);
        break;
    case INPUT_REL_HWHEEL:
        atomic_add(&acc_x, evt->value * SCROLL_BALL_GAIN);
        break;
    case INPUT_REL_WHEEL:
        atomic_add(&acc_y, evt->value * SCROLL_BALL_GAIN);
        break;
    default:
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
