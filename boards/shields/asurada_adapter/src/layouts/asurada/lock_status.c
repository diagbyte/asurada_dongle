#include "lock_status.h"

#include <zmk/display.h>
#include <zmk/hid_indicators.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/event_manager.h>

#include <fonts.h>
#include "display_colors.h"

/*
 * Host lock indicators for the keyboard page: shows CAPS / NUM while the host
 * has caps lock / num lock on, driven by the HID keyboard-LED output report the
 * host sends to the dongle. HID LED bitmask: bit0 = Num Lock, bit1 = Caps Lock.
 * Only the active ones are shown (hidden otherwise), centered, like the modifiers.
 */

#define LED_NUM_LOCK  0x01
#define LED_CAPS_LOCK 0x02

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct lock_state { bool caps; bool num; };

static void lock_update_cb(struct lock_state s) {
    struct zmk_widget_asurada_lock_status *w;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, w, node) {
        if (s.caps) {
            lv_obj_clear_flag(w->caps, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(w->caps, LV_OBJ_FLAG_HIDDEN);
        }
        if (s.num) {
            lv_obj_clear_flag(w->num, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(w->num, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static struct lock_state lock_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    zmk_hid_indicators_t ind = zmk_hid_indicators_get_current_profile();
    return (struct lock_state){
        .caps = (ind & LED_CAPS_LOCK) != 0,
        .num = (ind & LED_NUM_LOCK) != 0,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_lock_status, struct lock_state,
                            lock_update_cb, lock_get_state)
ZMK_SUBSCRIPTION(widget_asurada_lock_status, zmk_hid_indicators_changed);

static lv_obj_t *make_lock_label(lv_obj_t *parent, const char *text) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &FG_Medium_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(DISPLAY_COLOR_MOD_ACTIVE), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(l, 1, LV_PART_MAIN);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);   /* shown only while the lock is on */
    return l;
}

void zmk_widget_asurada_lock_status_init(struct zmk_widget_asurada_lock_status *w, lv_obj_t *parent) {
    w->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(w->obj);
    lv_obj_set_size(w->obj, LV_SIZE_CONTENT, 18);
    lv_obj_set_flex_flow(w->obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(w->obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(w->obj, 12, LV_PART_MAIN);
    lv_obj_clear_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

    w->caps = make_lock_label(w->obj, "CAPS");
    w->num = make_lock_label(w->obj, "NUM");

    sys_slist_append(&widgets, &w->node);
    widget_asurada_lock_status_init();
}

lv_obj_t *zmk_widget_asurada_lock_status_obj(struct zmk_widget_asurada_lock_status *w) {
    return w->obj;
}
