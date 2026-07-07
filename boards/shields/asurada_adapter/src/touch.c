#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/atomic.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(asurada_touch, LOG_LEVEL_WRN);

#include "asurada_screens.h"
#include "asurada_brightness.h"
#if IS_ENABLED(CONFIG_ASURADA_SCREENSAVER)
#include "asurada_screensaver.h"
#endif

/*
 * CST816S touch gestures for the Asurada dongle.
 *
 *   tap            -> peek at the status screen (wake from the screensaver)
 *   long press     -> show the four-eyes screensaver now
 *   swipe up/down  -> brighter / dimmer backlight
 *   swipe L/R      -> navigate carousel (keyboard ⟷ trackball pages)
 *
 * The CST816S Zephyr driver reports INPUT_ABS_X / INPUT_ABS_Y coordinates and
 * an INPUT_BTN_TOUCH press/release. The input callback runs on the driver's
 * thread, so the resolved gesture is handed to the system work queue before it
 * touches LVGL / brightness state.
 *
 * Coordinate directions depend on panel mounting; if a gesture feels flipped,
 * swap the sign tests below (or rotate the display).
 */

#define SWIPE_THRESHOLD CONFIG_ASURADA_TOUCH_SWIPE_THRESHOLD
#define BRIGHTNESS_STEP CONFIG_ASURADA_BRIGHTNESS_STEP
#define TAP_MAX_MOVE 20   /* px: movement below this is a tap, not a swipe */
#define LONG_PRESS_MS 600
#define SWIPE_MAX_MS 700  /* a swipe must complete within this time */

enum gesture {
    G_NONE = 0,
    G_TAP,
    G_LONG_PRESS,
    G_SWIPE_UP,
    G_SWIPE_DOWN,
    G_SWIPE_LEFT,
    G_SWIPE_RIGHT,
};

static atomic_t pending_gesture = ATOMIC_INIT(G_NONE);
static struct k_work gesture_work;

static int16_t last_x, last_y;
static int16_t start_x, start_y;
static int64_t press_time;
static bool touching;

static void gesture_work_handler(struct k_work *w) {
    ARG_UNUSED(w);
    enum gesture g = (enum gesture)atomic_set(&pending_gesture, G_NONE);

    switch (g) {
    case G_TAP:
#if IS_ENABLED(CONFIG_ASURADA_SCREENSAVER)
        asurada_screensaver_wake();
#endif
        break;
    case G_LONG_PRESS:
#if IS_ENABLED(CONFIG_ASURADA_SCREENSAVER)
        asurada_screensaver_force_sleep();
#endif
        break;
    case G_SWIPE_UP:
        asurada_brightness_adjust(+BRIGHTNESS_STEP);
        break;
    case G_SWIPE_DOWN:
        asurada_brightness_adjust(-BRIGHTNESS_STEP);
        break;
    case G_SWIPE_LEFT:
        asurada_screens_page_prev();
        break;
    case G_SWIPE_RIGHT:
        asurada_screens_page_next();
        break;
    default:
        break;
    }
}

static void post_gesture(enum gesture g) {
    atomic_set(&pending_gesture, g);
    k_work_submit(&gesture_work);
}

static void classify_and_post(int16_t dx, int16_t dy, int64_t dur) {
    int adx = (dx < 0) ? -dx : dx;
    int ady = (dy < 0) ? -dy : dy;

    if (adx < TAP_MAX_MOVE && ady < TAP_MAX_MOVE) {
        post_gesture(dur >= LONG_PRESS_MS ? G_LONG_PRESS : G_TAP);
        return;
    }

    if (dur > SWIPE_MAX_MS) {
        return; /* too slow to count as a swipe */
    }

    if (adx >= ady) {
        if (adx < SWIPE_THRESHOLD) {
            return;
        }
        post_gesture(dx > 0 ? G_SWIPE_RIGHT : G_SWIPE_LEFT);
    } else {
        if (ady < SWIPE_THRESHOLD) {
            return;
        }
        /* screen Y grows downward: moving up (dy < 0) brightens */
        post_gesture(dy < 0 ? G_SWIPE_UP : G_SWIPE_DOWN);
    }
}

static void touch_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);

    switch (evt->type) {
    case INPUT_EV_ABS:
        if (evt->code == INPUT_ABS_X) {
            last_x = (int16_t)evt->value;
        } else if (evt->code == INPUT_ABS_Y) {
            last_y = (int16_t)evt->value;
        }
        break;
    case INPUT_EV_KEY:
        if (evt->code == INPUT_BTN_TOUCH) {
            if (evt->value) {
                touching = true;
                start_x = last_x;
                start_y = last_y;
                press_time = k_uptime_get();
            } else if (touching) {
                touching = false;
                int16_t dx = last_x - start_x;
                int16_t dy = last_y - start_y;
                int64_t dur = k_uptime_get() - press_time;
                classify_and_post(dx, dy, dur);
            }
        }
        break;
    default:
        break;
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(cst816s)), touch_cb, NULL);

static int touch_init(void) {
    k_work_init(&gesture_work, gesture_work_handler);
    return 0;
}

SYS_INIT(touch_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
