#include "pointing_mode.h"

#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

/*
 * SCROLL / SNIPE pointing-mode indicator for the trackball page.
 *
 * Same listener wiring as layer_center.c; only the state selection differs:
 * instead of resolving the single highest active layer, this checks whether
 * specific pointing-mode layers (Kconfig-selectable; keymap-specific) are
 * active.
 */

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct pointing_mode_state {
    const char *text;
};

static const char *pointing_mode_text(void) {
    if (zmk_keymap_layer_active(CONFIG_ASURADA_SNIPE_LAYER)) {
        return "SNIPE";
    }
    if (zmk_keymap_layer_active(CONFIG_ASURADA_SCROLL_LAYER) ||
        zmk_keymap_layer_active(CONFIG_ASURADA_SCROLL_LAYER2)) {
        return "SCROLL";
    }
    return "";
}

static void pointing_mode_update_cb(struct pointing_mode_state state) {
    struct zmk_widget_asurada_pointing_mode *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        lv_label_set_text(widget->obj, state.text);
    }
}

static struct pointing_mode_state pointing_mode_get_state(const zmk_event_t *eh) {
    return (struct pointing_mode_state){.text = pointing_mode_text()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_pointing_mode, struct pointing_mode_state,
                            pointing_mode_update_cb, pointing_mode_get_state)
ZMK_SUBSCRIPTION(widget_asurada_pointing_mode, zmk_layer_state_changed);

void zmk_widget_asurada_pointing_mode_init(struct zmk_widget_asurada_pointing_mode *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    lv_obj_set_style_text_font(widget->obj, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->obj, lv_color_hex(0xFFB733), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(widget->obj, 2, LV_PART_MAIN);
    lv_label_set_text(widget->obj, "");
    /* Centred over the ball: the SCROLL/SNIPE mode is the focal readout when
     * active (cyan on the red sphere is high-contrast). Empty text = nothing shows. */
    lv_obj_align(widget->obj, LV_ALIGN_CENTER, 0, 0);

    sys_slist_append(&widgets, &widget->node);
    widget_asurada_pointing_mode_init();
}

lv_obj_t *zmk_widget_asurada_pointing_mode_obj(struct zmk_widget_asurada_pointing_mode *widget) {
    return widget->obj;
}
