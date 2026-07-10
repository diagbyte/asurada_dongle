#include "layer_center.h"

#include <zephyr/kernel.h>
#include <ctype.h>
#include <zmk/display.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#include <fonts.h>
#include "display_colors.h"

/*
 * Large centered layer name. Resolves the active layer's `display-name` from
 * the keymap (falling back to "Layer N"), mirroring Prospector's approach.
 */

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_center_state {
    uint8_t index;
};

static void layer_center_update_cb(struct layer_center_state state) {
    struct zmk_widget_layer_center *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        const char *layer_name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state.index));
        char display_name[32];

        if (layer_name && *layer_name) {
            snprintf(display_name, sizeof(display_name), "%s", layer_name);
        } else {
            snprintf(display_name, sizeof(display_name), "Layer %d", state.index);
        }

#if IS_ENABLED(CONFIG_ASURADA_LAYER_NAME_UPPERCASE)
        for (int i = 0; display_name[i]; i++) {
            display_name[i] = toupper((unsigned char)display_name[i]);
        }
#endif

        lv_label_set_text(widget->obj, display_name);
    }
}

#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
/* SCROLL / SCROLL-keep / SNIPE are trackball pointing modes -- they have their
 * own page (rolling ball + SCROLL/SNIPE indicator), so they should NOT hijack
 * the keyboard page's layer name. */
static bool is_trackball_layer(int i) {
    return i == CONFIG_ASURADA_SCROLL_LAYER || i == CONFIG_ASURADA_SCROLL_LAYER2 ||
           i == CONFIG_ASURADA_SNIPE_LAYER;
}
#endif

/* Highest active layer, skipping the trackball pointing-mode layers, so the
 * keyboard page keeps showing the real keyboard layer (BASE/CMK-DH/NAV/...)
 * underneath while the trackball is scrolling/sniping. Layer 0 is always active
 * and never a trackball layer, so this always resolves. */
static uint8_t highest_keyboard_layer(void) {
    for (int i = zmk_keymap_highest_layer_active(); i >= 0; i--) {
        if (!zmk_keymap_layer_active(i)) {
            continue;
        }
#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
        if (is_trackball_layer(i)) {
            continue;
        }
#endif
        return (uint8_t)i;
    }
    return 0;
}

static struct layer_center_state layer_center_get_state(const zmk_event_t *eh) {
    return (struct layer_center_state){.index = highest_keyboard_layer()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_center, struct layer_center_state, layer_center_update_cb,
                            layer_center_get_state)
ZMK_SUBSCRIPTION(widget_layer_center, zmk_layer_state_changed);

int zmk_widget_layer_center_init(struct zmk_widget_layer_center *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    /* DINishExpanded_Light_36 -- DIN gauge face (was PPF_NarrowThin_64 at 64px,
     * then FR_Regular_48); small enough to clear the WPM readout, modifiers,
     * CAPS/NUM and the L/R batteries on one round face. */
    lv_obj_set_style_text_font(widget->obj, &DINishExpanded_Light_36, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->obj, lv_color_hex(DISPLAY_COLOR_LAYER_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(widget->obj, 200);
    lv_label_set_long_mode(widget->obj, LV_LABEL_LONG_WRAP);
    lv_label_set_text(widget->obj, "");

    sys_slist_append(&widgets, &widget->node);
    widget_layer_center_init();

    return 0;
}

lv_obj_t *zmk_widget_layer_center_obj(struct zmk_widget_layer_center *widget) {
    return widget->obj;
}
