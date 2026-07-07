#include "wpm_border.h"

#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/wpm.h>

#include "display_colors.h"

/*
 * Typing-speed border gauge.
 *
 * A single full-screen lv_arc drawn around the rim of the round display. Its
 * fill tracks the current WPM, and the fill color ramps from a calm cyan (slow)
 * to a bright cyan (fast). As in Prospector's wpm_meter, the ~1 Hz WPM event
 * only sets a target; a ~30 fps work item smooths toward it with a fast attack
 * and slow release so the ring rises quickly and drains gently.
 */

#define ARC_SIZE 240
#define ARC_WIDTH 14
#define ARC_RANGE 1000

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static struct k_work_delayable wpm_smooth_work;

static float displayed_wpm = 0.0f;
static float target_wpm = 0.0f;
static const float smoothing_factor_up = 0.3f;
static const float smoothing_factor_down = 0.05f;

struct wpm_border_state {
    uint8_t wpm;
};

static lv_color_t lerp_color(uint32_t a, uint32_t b, float t) {
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    uint8_t r = (uint8_t)(ar + (int)((br - ar) * t));
    uint8_t g = (uint8_t)(ag + (int)((bg - ag) * t));
    uint8_t bl = (uint8_t)(ab + (int)((bb - ab) * t));
    return lv_color_make(r, g, bl);
}

static void wpm_border_render(void) {
    float ratio = displayed_wpm / (float)WPM_BORDER_MAX;
    if (ratio > 1.0f) {
        ratio = 1.0f;
    }
    int value = (int)(ratio * ARC_RANGE + 0.5f);
    lv_color_t fill = lerp_color(DISPLAY_COLOR_WPM_FILL_LOW, DISPLAY_COLOR_WPM_FILL_HIGH, ratio);

    struct zmk_widget_wpm_border *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        lv_arc_set_value(widget->arc, value);
        lv_obj_set_style_arc_color(widget->arc, fill, LV_PART_INDICATOR);
    }
}

static void wpm_smooth_work_handler(struct k_work *work) {
    float diff = target_wpm - displayed_wpm;
    bool at_target = (diff > -0.5f && diff < 0.5f);

    if (at_target) {
        displayed_wpm = target_wpm;
    } else {
        float factor = (diff > 0) ? smoothing_factor_up : smoothing_factor_down;
        displayed_wpm += diff * factor;
    }

    wpm_border_render();

    if (!at_target) {
        k_work_schedule(&wpm_smooth_work, K_MSEC(33));
    }
}

static void wpm_border_update_cb(struct wpm_border_state state) {
    target_wpm = (float)state.wpm;
    k_work_schedule(&wpm_smooth_work, K_NO_WAIT);
}

static struct wpm_border_state wpm_border_get_state(const zmk_event_t *eh) {
    return (struct wpm_border_state){.wpm = zmk_wpm_get_state()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_border, struct wpm_border_state, wpm_border_update_cb,
                            wpm_border_get_state)
ZMK_SUBSCRIPTION(widget_wpm_border, zmk_wpm_state_changed);

int zmk_widget_wpm_border_init(struct zmk_widget_wpm_border *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, ARC_SIZE, ARC_SIZE);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);

    widget->arc = lv_arc_create(widget->obj);
    lv_obj_set_size(widget->arc, ARC_SIZE, ARC_SIZE);
    lv_obj_center(widget->arc);
    lv_obj_set_style_pad_all(widget->arc, 0, LV_PART_MAIN);
    lv_arc_set_range(widget->arc, 0, ARC_RANGE);
    lv_arc_set_value(widget->arc, 0);
    lv_arc_set_bg_angles(widget->arc, 0, 360);
    lv_arc_set_rotation(widget->arc, 270); /* start at 12 o'clock, fill clockwise */

    lv_obj_set_style_arc_width(widget->arc, ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_color(widget->arc, lv_color_hex(DISPLAY_COLOR_WPM_RING_BG), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(widget->arc, true, LV_PART_MAIN);

    lv_obj_set_style_arc_width(widget->arc, ARC_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(widget->arc, lv_color_hex(DISPLAY_COLOR_WPM_FILL_LOW), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(widget->arc, true, LV_PART_INDICATOR);

    lv_obj_remove_style(widget->arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(widget->arc, LV_OBJ_FLAG_CLICKABLE);

    sys_slist_append(&widgets, &widget->node);
    widget_wpm_border_init();

    k_work_init_delayable(&wpm_smooth_work, wpm_smooth_work_handler);

    return 0;
}

lv_obj_t *zmk_widget_wpm_border_obj(struct zmk_widget_wpm_border *widget) {
    return widget->obj;
}
