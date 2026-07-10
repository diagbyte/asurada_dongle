#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/drivers/i2c.h>

#include <zmk/ble.h>

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
 *   tap            -> toggle the screen: sleep when awake, wake when asleep
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
#define LONG_PRESS_MS 800 /* held this long = long-press; > SWIPE_MAX_MS so a real
                           * swipe always releases first and can't be mistaken */
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
static bool long_press_fired;   /* long-press already posted mid-hold this touch */

static void gesture_work_handler(struct k_work *w) {
    ARG_UNUSED(w);
    enum gesture g = (enum gesture)atomic_set(&pending_gesture, G_NONE);

    switch (g) {
    case G_TAP:
        /* Tap = WAKE only (show the keyboard/status). It never sleeps on tap, so
         * there is no toggle to fight the activity system. Sleep is long-press or
         * the idle timeout. Page nav is swipe-only. */
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
#if IS_ENABLED(CONFIG_ASURADA_STATUS_SCREEN_ASURADA)
        asurada_screens_page_prev();
#else
        zmk_ble_prof_prev();
#endif
        break;
    case G_SWIPE_RIGHT:
#if IS_ENABLED(CONFIG_ASURADA_STATUS_SCREEN_ASURADA)
        asurada_screens_page_next();
#else
        zmk_ble_prof_next();
#endif
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

/*
 * CST816S coordinate polling. The Zephyr driver only emits INPUT_ABS on the
 * touch down/up IRQ, so a drag never streams intermediate coordinates and every
 * gesture reads as a tap (dx=dy~=0) -- exactly the "swipe does nothing, every
 * touch taps to the next page" symptom seen on hardware. Instead we read the XY
 * registers straight off the chip: once at touch-down for the start point, then
 * on a ~22 ms work item for the life of the touch, so a drag builds a real delta.
 */
#define CST816S_REG_FINGER_NUM 0x02   /* count; X at 0x03/0x04, Y at 0x05/0x06 */
#define CST816S_REG_GESTURE    0x01   /* on-chip slide classification */
#define CST816S_GESTURE_UP     0x01
#define CST816S_GESTURE_DOWN   0x02
#define CST816S_GESTURE_LEFT   0x03
#define CST816S_GESTURE_RIGHT  0x04
#define CST816S_GESTURE_LONG_PRESS 0x0c   /* CST816S native long-press */
#define TOUCH_POLL_MS 22

static const struct i2c_dt_spec cst816s_i2c = I2C_DT_SPEC_GET(DT_NODELABEL(cst816s));
static struct k_work_delayable touch_poll_work;

static bool cst816s_read_xy(int16_t *x, int16_t *y) {
    uint8_t b[5];   /* 0x02 count, 0x03 XH, 0x04 XL, 0x05 YH, 0x06 YL */
    if (i2c_burst_read_dt(&cst816s_i2c, CST816S_REG_FINGER_NUM, b, sizeof(b)) != 0) {
        return false;
    }
    if ((b[0] & 0x0F) == 0) {
        return false;   /* finger lifted */
    }
    *x = (int16_t)(((b[1] & 0x0F) << 8) | b[2]);
    *y = (int16_t)(((b[3] & 0x0F) << 8) | b[4]);
    return true;
}

static void touch_poll_handler(struct k_work *w) {
    ARG_UNUSED(w);
    if (!touching) {
        return;
    }
    int16_t x, y;
    if (cst816s_read_xy(&x, &y)) {
        last_x = x;
        last_y = y;
    }
    /* Fire the long-press MID-HOLD rather than trusting the touch-up event: with
     * the change-IRQ config a stationary hold streams no interrupts and the panel
     * may even drop the finger, so the release timing is unreliable. Prefer the
     * chip's own long-press gesture (0x0C); fall back to our own duration timer
     * (LONG_PRESS_MS > SWIPE_MAX_MS so a real swipe releases first). */
    if (!long_press_fired) {
        uint8_t gid = 0;
        (void)i2c_reg_read_byte_dt(&cst816s_i2c, CST816S_REG_GESTURE, &gid);
        if (gid == CST816S_GESTURE_LONG_PRESS ||
            (k_uptime_get() - press_time) >= LONG_PRESS_MS) {
            long_press_fired = true;
            post_gesture(G_LONG_PRESS);
        }
    }
    k_work_schedule(&touch_poll_work, K_MSEC(TOUCH_POLL_MS));
}

static void touch_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);

    /* Only the touch up/down edge matters here; coordinates come from
     * cst816s_read_xy() (raw registers), not the driver's INPUT_ABS events. */
    if (evt->type != INPUT_EV_KEY || evt->code != INPUT_BTN_TOUCH) {
        return;
    }

    if (evt->value) {
        touching = true;
        long_press_fired = false;
        cst816s_read_xy(&last_x, &last_y);   /* seed the start point */
        start_x = last_x;
        start_y = last_y;
        press_time = k_uptime_get();
        k_work_schedule(&touch_poll_work, K_MSEC(TOUCH_POLL_MS));
    } else if (touching) {
        touching = false;
        k_work_cancel_delayable(&touch_poll_work);
        if (long_press_fired) {
            return;   /* long-press already posted mid-hold */
        }
        int64_t dur = k_uptime_get() - press_time;
        int16_t mdx = last_x - start_x, mdy = last_y - start_y;
        int amdx = (mdx < 0) ? -mdx : mdx;
        int amdy = (mdy < 0) ? -mdy : mdy;

        /* Long-press wins first: a held, roughly-stationary touch past the
         * threshold sleeps the display. Check the duration directly so a bit of
         * finger drift (which the chip may report as a slide) can't steal it. */
        if (dur >= LONG_PRESS_MS && amdx < TAP_MAX_MOVE && amdy < TAP_MAX_MOVE) {
            post_gesture(G_LONG_PRESS);
            return;
        }

        /* Otherwise prefer the CST816S's own gesture engine (reg 0x01): it
         * classifies a slide on-chip, which works even when the coordinate
         * registers don't stream a drag (this panel latches the down coord).
         * Fall back to the polled delta for a plain tap. */
        uint8_t gid = 0;
        (void)i2c_reg_read_byte_dt(&cst816s_i2c, CST816S_REG_GESTURE, &gid);
        switch (gid) {
        case CST816S_GESTURE_UP:    post_gesture(G_SWIPE_UP);    break;
        case CST816S_GESTURE_DOWN:  post_gesture(G_SWIPE_DOWN);  break;
        case CST816S_GESTURE_LEFT:  post_gesture(G_SWIPE_LEFT);  break;
        case CST816S_GESTURE_RIGHT: post_gesture(G_SWIPE_RIGHT); break;
        case CST816S_GESTURE_LONG_PRESS: post_gesture(G_LONG_PRESS); break;
        default:
            classify_and_post(mdx, mdy, dur);
            break;
        }
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(cst816s)), touch_cb, NULL);

/*
 * CST816S drag/swipe fix. By default the chip's IrqCtl (reg 0xFA) fires the INT
 * mainly on touch down/up, so the Zephyr driver never delivers the intermediate
 * coordinates a drag needs -- every gesture then reads as a tap (dx=dy≈0). We:
 *   - 0xFE DisAutoSleep = 1                  : keep the chip awake so the config
 *                                              sticks (USB-powered dongle, fine).
 *   - 0xFA IrqCtl = EnTouch | EnChange (0x60): interrupt on every coordinate
 *     change, so a drag streams INPUT_ABS events and the swipe classifier works.
 * Written on a delayed work item, after the touch driver's own init has already
 * configured the chip, so our values win. If the write fails or the chip ignores
 * it, gestures fall back to tap-only + the auto-page-follow path.
 */
#define CST816S_REG_DIS_AUTOSLEEP  0xFE
#define CST816S_REG_IRQ_CTL        0xFA
#define CST816S_IRQ_EN_CHANGE      0x60   /* EnTouch | EnChange */
#define CST816S_REG_MOTION_MASK    0xEC
#define CST816S_MOTION_EN_CONT     0x06   /* EnConUD | EnConLR: on-chip slide detect */

static void enable_change_irq(struct k_work *w) {
    ARG_UNUSED(w);
    if (!device_is_ready(cst816s_i2c.bus)) {
        LOG_WRN("CST816S I2C bus not ready; swipe stays tap-only");
        return;
    }
    int e1 = i2c_reg_write_byte_dt(&cst816s_i2c, CST816S_REG_DIS_AUTOSLEEP, 0x01);
    int e2 = i2c_reg_write_byte_dt(&cst816s_i2c, CST816S_REG_IRQ_CTL, CST816S_IRQ_EN_CHANGE);
    /* Enable the chip's continuous slide-gesture engine so reg 0x01 reports
     * up/down/left/right -- this is what the touch-up handler reads. */
    int e3 = i2c_reg_write_byte_dt(&cst816s_i2c, CST816S_REG_MOTION_MASK, CST816S_MOTION_EN_CONT);
    if (e1 || e2 || e3) {
        LOG_WRN("CST816S reg write failed (%d/%d/%d); swipe may stay tap-only", e1, e2, e3);
    }
}
static K_WORK_DELAYABLE_DEFINE(irq_ctl_work, enable_change_irq);

static int touch_init(void) {
    k_work_init(&gesture_work, gesture_work_handler);
    k_work_init_delayable(&touch_poll_work, touch_poll_handler);
    /* Configure change-IRQ ~600 ms after boot, once the CST816S driver's own
     * init has run and the chip is awake. */
    k_work_schedule(&irq_ctl_work, K_MSEC(600));
    return 0;
}

SYS_INIT(touch_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
