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

/*
 * Free-running touch state machine (~22 ms), always polling.
 *
 * We deliberately do NOT drive touch from the CST816S driver's INPUT_BTN_TOUCH
 * edge anymore. On this hardware the driver's IRQ stops delivering events once
 * the system idles and the display drops to the standby eyes (a tap then produced
 * NO event at all -- "touch completely dead after the keyboard was connected",
 * because the keyboard's typing gaps are what let the display idle). A timer-
 * driven poll wakes the CPU on its own and reads the chip's finger-count + XY
 * registers directly, so a touch is detected regardless of the IRQ or CPU idle
 * state, and the steady I2C traffic keeps the chip out of its low-power standby.
 *
 * Down/hold/up are derived from the finger-count register; the gesture register
 * (0x01) is still read exactly ONCE, at up -- never mid-slide (that clears the
 * chip's gesture state and breaks both swipe and the native long-press).
 */
static void touch_poll_handler(struct k_work *w) {
    ARG_UNUSED(w);

    int16_t x, y;
    bool finger = cst816s_read_xy(&x, &y);

    if (finger) {
        if (!touching) {
            /* ---- touch DOWN ---- */
            touching = true;
            long_press_fired = false;
            start_x = last_x = x;
            start_y = last_y = y;
            press_time = k_uptime_get();
#if IS_ENABLED(CONFIG_ASURADA_SCREENSAVER)
            /* Wake on touch-down when the eyes are up, so the following swipe/tap
             * acts on the VISIBLE carousel (also restores the deep display-off).
             * No bounce: tap is wake-only and the activity listener no longer
             * re-shows the eyes on ACTIVE; long-press still sleeps below. */
            if (asurada_screensaver_is_active()) {
                asurada_screensaver_wake();
            }
#endif
        } else {
            /* ---- held / dragging ---- */
            last_x = x;
            last_y = y;
            /* Long-press fires MID-HOLD off the duration timer (release timing is
             * unreliable). No gesture-register read here. */
            if (!long_press_fired && (k_uptime_get() - press_time) >= LONG_PRESS_MS) {
                long_press_fired = true;
                post_gesture(G_LONG_PRESS);
            }
        }
    } else if (touching) {
        /* ---- touch UP ---- */
        touching = false;
        if (!long_press_fired) {
            int64_t dur = k_uptime_get() - press_time;
            int16_t mdx = last_x - start_x, mdy = last_y - start_y;
            int amdx = (mdx < 0) ? -mdx : mdx;
            int amdy = (mdy < 0) ? -mdy : mdy;

            /* Long-press wins first: a held, roughly-stationary touch past the
             * threshold sleeps the display (fallback; usually already fired). */
            if (dur >= LONG_PRESS_MS && amdx < TAP_MAX_MOVE && amdy < TAP_MAX_MOVE) {
                post_gesture(G_LONG_PRESS);
            } else {
                /* Prefer the CST816S's own gesture engine (reg 0x01, read ONCE):
                 * it classifies a slide on-chip even when the coordinate registers
                 * latch the down position. Fall back to the polled delta for a tap. */
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
    }

    k_work_schedule(&touch_poll_work, K_MSEC(TOUCH_POLL_MS));   /* free-running */
}

/* (The old INPUT_BTN_TOUCH callback that used to start/stop the poll was removed:
 * touch_poll_handler above now runs free and owns the whole down/hold/up state
 * machine, so touch no longer depends on the driver's IRQ firing.) */

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
    /* Re-assert every 2 s. DisAutoSleep in particular must keep sticking: if the
     * chip ever resets or slips into standby it stops answering, which is what made
     * touch die after the display idled. Idempotent writes; negligible on a
     * USB-powered dongle. Reschedule via the passed work handle (the delayable is
     * defined below, so it isn't in scope by name here). */
    k_work_schedule(k_work_delayable_from_work(w), K_SECONDS(2));
}
static K_WORK_DELAYABLE_DEFINE(irq_ctl_work, enable_change_irq);

static int touch_init(void) {
    k_work_init(&gesture_work, gesture_work_handler);
    k_work_init_delayable(&touch_poll_work, touch_poll_handler);
    /* Configure the chip ~600 ms after boot (once the driver's own init ran), then
     * start the free-running touch poll just after so it reads a configured chip. */
    k_work_schedule(&irq_ctl_work, K_MSEC(600));
    k_work_schedule(&touch_poll_work, K_MSEC(700));
    return 0;
}

SYS_INIT(touch_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
