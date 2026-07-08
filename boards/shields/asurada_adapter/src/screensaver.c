#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/activity.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/event_manager.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(asurada_saver, LOG_LEVEL_WRN);

#include "asurada_screensaver.h"
#include "asurada_brightness.h"

/*
 * Asurada four-eyes idle screensaver.
 *
 * Driven by zmk_activity_state_changed (ACTIVE -> status screen, IDLE/SLEEP ->
 * eyes + dim). Because the ZMK activity listener runs on the system work queue
 * but LVGL must run on the display work queue, every LVGL action is marshalled
 * onto zmk_display_work_q().
 *
 * The four eyes are green LEDs in a 2x2 cluster that idly glance to a random
 * angle (+/-30 deg) and hold ("looking around"), with a ring + soft glow and a
 * gentle brightness pulse -- the confirmed Asurada face.
 */

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define EYE_COUNT   4
#define EYE_GREEN   0x5FE23F   /* LED core */
#define EYE_RING    0xA8FF88   /* brighter ring */
#define TICK_MS     40
#define FACE_C      120        /* 240/2 face centre */
#define EYE_R       32         /* LED radius, px */
#define CLUSTER_R   58         /* centre -> each LED centre (2x2 spread) */
#define GLANCE_DEG  30         /* random glance amplitude, +/- degrees */

static lv_obj_t *eyes_screen;
static lv_obj_t *eyes[EYE_COUNT];
static lv_obj_t *saved_status_screen;
static lv_timer_t *blink_timer;
static lv_timer_t *peek_timer;
static bool showing_eyes;

/* Small self-contained PRNG so we don't depend on the entropy generator. */
static uint32_t rng_state = 0x1234abcdu;
static uint32_t xrand(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static float cur_angle, tgt_angle;   /* radians; eased current + random target */
static int glance_countdown = 25;

/* Position the four LEDs at the corners of a square rotated by `ang`. */
static void place_eyes(float ang) {
    for (int i = 0; i < EYE_COUNT; i++) {
        float a = ang + (float)(M_PI / 4.0f + i * (M_PI / 2.0f));
        int cx = FACE_C + (int)(CLUSTER_R * cosf(a));
        int cy = FACE_C + (int)(CLUSTER_R * sinf(a));
        lv_obj_set_pos(eyes[i], cx - EYE_R, cy - EYE_R);
    }
}

static void glance_tick(lv_timer_t *t) {
    ARG_UNUSED(t);

    /* Move (and thus redraw) ONLY while easing toward a new glance angle. When
     * settled, do nothing so the LEDs stay perfectly static -- the old per-tick
     * brightness pulse redrew all four shadowed LEDs every frame, which caused
     * the flicker and ghost trails on the GC9A01. */
    float diff = tgt_angle - cur_angle;
    if (fabsf(diff) > 0.002f) {
        cur_angle += diff * 0.10f;
        place_eyes(cur_angle);
    }

    /* Option B: hold, then glance to a new random angle. Infrequent + gentle. */
    if (--glance_countdown <= 0) {
        float frac = (float)(xrand() % 2001) / 1000.0f - 1.0f;   /* -1..1 */
        tgt_angle = frac * (float)(GLANCE_DEG * M_PI / 180.0f);
        glance_countdown = 45 + (xrand() % 105);                 /* ~1.8..6.0 s */
    }
}

static void build_eyes_screen(void) {
    eyes_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(eyes_screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(eyes_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(eyes_screen, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < EYE_COUNT; i++) {
        eyes[i] = lv_obj_create(eyes_screen);
        lv_obj_remove_style_all(eyes[i]);
        lv_obj_set_size(eyes[i], EYE_R * 2, EYE_R * 2);
        lv_obj_set_style_radius(eyes[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(eyes[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(eyes[i], lv_color_hex(EYE_GREEN), LV_PART_MAIN);
        lv_obj_set_style_border_color(eyes[i], lv_color_hex(EYE_RING), LV_PART_MAIN);
        lv_obj_set_style_border_width(eyes[i], 3, LV_PART_MAIN);
        lv_obj_set_style_border_opa(eyes[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(eyes[i], lv_color_hex(EYE_GREEN), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(eyes[i], 8, LV_PART_MAIN);   /* modest glow, less trail */
        lv_obj_set_style_shadow_spread(eyes[i], 2, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(eyes[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_clear_flag(eyes[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    cur_angle = 0.0f;
    tgt_angle = 0.0f;
    glance_countdown = 25;
    place_eyes(0.0f);
}

/* --- display-work-queue context below --- */

static void enter_eyes(void) {
    if (!eyes_screen) {
        build_eyes_screen();
    }
    if (!showing_eyes) {
        saved_status_screen = lv_scr_act();
        lv_scr_load(eyes_screen);
        showing_eyes = true;
        if (!blink_timer) {
            blink_timer = lv_timer_create(glance_tick, TICK_MS, NULL);
        } else {
            lv_timer_resume(blink_timer);
        }
    }
    asurada_brightness_dim(CONFIG_ASURADA_SCREENSAVER_BRIGHTNESS);
}

static void exit_eyes(void) {
    if (showing_eyes && saved_status_screen) {
        lv_scr_load(saved_status_screen);
        showing_eyes = false;
        if (blink_timer) {
            lv_timer_pause(blink_timer);
        }
    }
    asurada_brightness_restore();
}

static void peek_expire(lv_timer_t *t) {
    ARG_UNUSED(t);
    lv_timer_pause(peek_timer);
    if (zmk_activity_get_state() != ZMK_ACTIVITY_ACTIVE) {
        enter_eyes();
    }
}

static struct k_work enter_work;
static struct k_work exit_work;
static struct k_work peek_work;

static void enter_work_handler(struct k_work *w) {
    ARG_UNUSED(w);
    if (peek_timer) {
        lv_timer_pause(peek_timer);
    }
    enter_eyes();
}

static void exit_work_handler(struct k_work *w) {
    ARG_UNUSED(w);
    if (peek_timer) {
        lv_timer_pause(peek_timer);
    }
    exit_eyes();
}

static void peek_work_handler(struct k_work *w) {
    ARG_UNUSED(w);
    exit_eyes();
    if (!peek_timer) {
        peek_timer = lv_timer_create(peek_expire, 6000, NULL);
    }
    lv_timer_set_period(peek_timer, 6000);
    lv_timer_reset(peek_timer);
    lv_timer_resume(peek_timer);
}

/* --- public API (any thread) --- */

void asurada_screensaver_wake(void) {
    k_work_submit_to_queue(zmk_display_work_q(), &peek_work);
}

void asurada_screensaver_force_sleep(void) {
    k_work_submit_to_queue(zmk_display_work_q(), &enter_work);
}

bool asurada_screensaver_is_active(void) {
    return showing_eyes;
}

/* --- ZMK activity listener (system work queue) --- */

static int activity_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->state == ZMK_ACTIVITY_ACTIVE) {
        k_work_submit_to_queue(zmk_display_work_q(), &exit_work);
    } else {
        k_work_submit_to_queue(zmk_display_work_q(), &enter_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(asurada_screensaver, activity_listener);
ZMK_SUBSCRIPTION(asurada_screensaver, zmk_activity_state_changed);

static int screensaver_init(void) {
    rng_state ^= (uint32_t)k_uptime_get();
    k_work_init(&enter_work, enter_work_handler);
    k_work_init(&exit_work, exit_work_handler);
    k_work_init(&peek_work, peek_work_handler);
    return 0;
}

SYS_INIT(screensaver_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
