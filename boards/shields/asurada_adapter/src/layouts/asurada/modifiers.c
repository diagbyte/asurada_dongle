#include "modifiers.h"

#include <zmk/display.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/hid.h>

#include <fonts.h>
#include "display_colors.h"

/*
 * English modifier indicator for the keyboard page: SHIFT CTRL ALT GUI.
 * Reads the central's explicit HID modifier flags directly (no dependency on
 * the config-gated modifier_order helper) and colors each label active/inactive.
 * Order is fixed. Mirrors operator/modifier_indicator.c's HID read.
 */

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static const char *const mod_text[ASURADA_MOD_COUNT] = {"SHIFT", "CTRL", "ALT", "GUI"};

struct asurada_mod_state {
    bool active[ASURADA_MOD_COUNT];   /* [0]=shift [1]=ctrl [2]=alt [3]=gui */
};

static void modifiers_update_cb(struct asurada_mod_state state) {
    struct zmk_widget_asurada_modifiers *w;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, w, node) {
        for (int i = 0; i < ASURADA_MOD_COUNT; i++) {
            lv_color_t c = lv_color_hex(state.active[i] ? DISPLAY_COLOR_MOD_ACTIVE
                                                        : DISPLAY_COLOR_MOD_INACTIVE);
            lv_obj_set_style_text_color(w->labels[i], c, LV_PART_MAIN);
        }
    }
}

static struct asurada_mod_state modifiers_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();
    struct asurada_mod_state state = {.active = {false, false, false, false}};
    state.active[0] = (mods & (MOD_LSFT | MOD_RSFT)) != 0;
    state.active[1] = (mods & (MOD_LCTL | MOD_RCTL)) != 0;
    state.active[2] = (mods & (MOD_LALT | MOD_RALT)) != 0;
    state.active[3] = (mods & (MOD_LGUI | MOD_RGUI)) != 0;
    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_modifiers, struct asurada_mod_state,
                            modifiers_update_cb, modifiers_get_state)
ZMK_SUBSCRIPTION(widget_asurada_modifiers, zmk_keycode_state_changed);

void zmk_widget_asurada_modifiers_init(struct zmk_widget_asurada_modifiers *w, lv_obj_t *parent) {
    w->obj = lv_obj_create(parent);
    lv_obj_set_size(w->obj, 200, 26);
    lv_obj_set_style_bg_opa(w->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(w->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(w->obj, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(w->obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(w->obj, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < ASURADA_MOD_COUNT; i++) {
        w->labels[i] = lv_label_create(w->obj);
        lv_label_set_text(w->labels[i], mod_text[i]);
        lv_obj_set_style_text_font(w->labels[i], &FG_Medium_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(w->labels[i], lv_color_hex(DISPLAY_COLOR_MOD_INACTIVE), LV_PART_MAIN);
    }

    sys_slist_append(&widgets, &w->node);
    widget_asurada_modifiers_init();
}

lv_obj_t *zmk_widget_asurada_modifiers_obj(struct zmk_widget_asurada_modifiers *w) {
    return w->obj;
}
