#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <lvgl.h>
#include <zmk/display.h>

#include "asurada_screens.h"

#define SCREEN_W 240
#define MAX_PAGES 4
#define AUTO_COOLDOWN_MS 400

static lv_obj_t *track;
static lv_obj_t *pages[MAX_PAGES];
static int page_count;
static int active_page;
static asurada_page_activate_cb activate_cb;
static int64_t last_switch;
static atomic_t goto_target = ATOMIC_INIT(-1);
static struct k_work goto_work_item;

static void anim_x_cb(void *obj, int32_t v) {
    lv_obj_set_x((lv_obj_t *)obj, v);
}

/* Runs on the display work queue. */
static void go_to(int page) {
    if (page_count == 0) {
        return;
    }
    page = ((page % page_count) + page_count) % page_count; /* wrap */
    int prev = active_page;
    active_page = page;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, track);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_set_values(&a, lv_obj_get_x(track), -page * SCREEN_W);
    lv_anim_set_duration(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    if (activate_cb && prev != page) {
        activate_cb(prev, false);
        activate_cb(page, true);
    }
}

struct page_msg {
    struct k_work work;
    int delta;
};
static struct page_msg next_msg, prev_msg;

static void page_work(struct k_work *w) {
    struct page_msg *m = CONTAINER_OF(w, struct page_msg, work);
    go_to(active_page + m->delta);
}

lv_obj_t *asurada_screens_page(int i) {
    return pages[i];
}

lv_obj_t *asurada_screens_init(lv_obj_t *screen, int n_pages) {
    page_count = (n_pages > MAX_PAGES) ? MAX_PAGES : n_pages;
    active_page = 0;

    track = lv_obj_create(screen);
    lv_obj_remove_style_all(track);
    lv_obj_set_size(track, SCREEN_W * page_count, SCREEN_W);
    lv_obj_set_pos(track, 0, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < page_count; i++) {
        pages[i] = lv_obj_create(track);
        lv_obj_remove_style_all(pages[i]);
        lv_obj_set_size(pages[i], SCREEN_W, SCREEN_W);
        lv_obj_set_pos(pages[i], i * SCREEN_W, 0);
        lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    k_work_init(&next_msg.work, page_work); next_msg.delta = 1;
    k_work_init(&prev_msg.work, page_work); prev_msg.delta = -1;
    k_work_init(&goto_work_item, goto_work);
    return track;
}

void asurada_screens_set_activate_cb(asurada_page_activate_cb cb) {
    activate_cb = cb;
    if (cb) {                       /* announce initial state */
        for (int i = 0; i < page_count; i++) {
            cb(i, i == active_page);
        }
    }
}

void asurada_screens_page_next(void) {
    k_work_submit_to_queue(zmk_display_work_q(), &next_msg.work);
}
void asurada_screens_page_prev(void) {
    k_work_submit_to_queue(zmk_display_work_q(), &prev_msg.work);
}

static void goto_work(struct k_work *w) {
    ARG_UNUSED(w);
    int p = (int)atomic_set(&goto_target, -1);
    if (p < 0 || p == active_page) {
        return;                       /* nothing to do / already there */
    }
    int64_t now = k_uptime_get();
    if (now - last_switch < AUTO_COOLDOWN_MS) {
        return;                       /* hysteresis: ignore rapid re-switch */
    }
    last_switch = now;
    go_to(p);
}

void asurada_screens_page_goto(int page) {
    atomic_set(&goto_target, page);
    k_work_submit_to_queue(zmk_display_work_q(), &goto_work_item);
}
