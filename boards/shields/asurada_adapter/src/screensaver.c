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
 * The four eyes are cyan capsules that blink at random intervals and pulse a
 * soft glow. This is a deliberately simple, low-RAM placeholder for the real
 * Asurada face art -- swap the geometry / add an lv_image sprite later.
 */

#define EYE_COUNT 4
#define EYE_COLOR 0x35E0FF
#define BLINK_FRAMES 8
#define TICK_MS 45

/* Top-left position, width and (open) height of each eye on the 240x240 face. */
static const int16_t eye_x[EYE_COUNT] = { 74, 124,  82, 128 };
static const int16_t eye_y[EYE_COUNT] = { 126, 126, 92,  92 };
static const int16_t eye_w[EYE_COUNT] = { 42,  42,  30,  30 };
static const int16_t eye_h[EYE_COUNT] = { 24,  24,  16,  16 };

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

static int blink_phase;
static int blink_countdown = 40;
static uint32_t glow;

static void apply_eyes(int openness, lv_opa_t opa) {
    for (int i = 0; i < EYE_COUNT; i++) {
        int h = eye_h[i] * openness / 100;
        if (h < 2) {
            h = 2;
        }
        int y = eye_y[i] + (eye_h[i] - h) / 2;
        lv_obj_set_size(eyes[i], eye_w[i], h);
        lv_obj_set_pos(eyes[i], eye_x[i], y);
        lv_obj_set_style_bg_opa(eyes[i], opa, LV_PART_MAIN);
    }
}

static void blink_tick(lv_timer_t *t) {
    ARG_UNUSED(t);

    /* Soft glow: triangle wave 195..255. */
    glow += 3;
    int g = glow % 120;
    int tri = (g < 60) ? g : (120 - g);
    lv_opa_t opa = (lv_opa_t)(195 + tri);

    int openness;
    if (blink_phase > 0) {
        blink_phase--;
        int pos = BLINK_FRAMES - blink_phase; /* 1..BLINK_FRAMES */
        int half = BLINK_FRAMES / 2;
        int dist = pos - half;
        if (dist < 0) {
            dist = -dist;
        }
        openness = dist * 100 / half; /* closes to 0 at mid-blink, reopens */
    } else {
        openness = 100;
        if (--blink_countdown <= 0) {
            blink_phase = BLINK_FRAMES;
            blink_countdown = 30 + (xrand() % 70); /* ~1.35s .. 4.5s */
        }
    }

    apply_eyes(openness, opa);
}

static void build_eyes_screen(void) {
    eyes_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(eyes_screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(eyes_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(eyes_screen, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < EYE_COUNT; i++) {
        eyes[i] = lv_obj_create(eyes_screen);
        lv_obj_set_size(eyes[i], eye_w[i], eye_h[i]);
        lv_obj_set_pos(eyes[i], eye_x[i], eye_y[i]);
        lv_obj_set_style_bg_color(eyes[i], lv_color_hex(EYE_COLOR), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(eyes[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(eyes[i], 0, LV_PART_MAIN);
        lv_obj_set_style_radius(eyes[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_pad_all(eyes[i], 0, LV_PART_MAIN);
        lv_obj_clear_flag(eyes[i], LV_OBJ_FLAG_SCROLLABLE);
    }
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
            blink_timer = lv_timer_create(blink_tick, TICK_MS, NULL);
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
