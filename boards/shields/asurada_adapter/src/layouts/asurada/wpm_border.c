#include "wpm_border.h"

#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/wpm.h>

#include <math.h>
#include <fonts.h>

#include "display_colors.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/*
 * Typing-speed tachometer gauge.
 *
 * A 240-degree lv_arc sweeping from 8 o'clock over the top to 4 o'clock, leaving
 * the bottom open for the battery. Its fill tracks the current WPM, and the fill
 * color ramps like an automotive tach from a calm cyan (slow) through amber to a
 * red redline (fast). As in Prospector's wpm_meter, the ~1 Hz WPM event only
 * sets a target; a ~30 fps work item smooths toward it with a fast attack and
 * slow release so the arc rises quickly and drains gently.
 */

#define ARC_SIZE 240
#define ARC_WIDTH 14
#define ARC_RANGE 1000

#define TICK_COUNT   8
#define ARC_R        ((ARC_SIZE - ARC_WIDTH) / 2)   /* arc centre-line radius, px */
#define A0_DEG       150.0f                          /* 8 o'clock */
#define SWEEP_DEG    240.0f

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

static lv_color_t tach_color(float ratio) {
    if (ratio < 0.5f) {
        return lerp_color(DISPLAY_COLOR_WPM_FILL_LOW, DISPLAY_COLOR_WPM_FILL_MID, ratio * 2.0f);
    }
    return lerp_color(DISPLAY_COLOR_WPM_FILL_MID, DISPLAY_COLOR_WPM_FILL_HIGH, (ratio - 0.5f) * 2.0f);
}

static void ticks_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    float cx = coords.x1 + ARC_SIZE / 2.0f;
    float cy = coords.y1 + ARC_SIZE / 2.0f;

    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.width = 2;

    for (int i = 0; i < TICK_COUNT; i++) {
        float deg = A0_DEG + SWEEP_DEG * (float)i / (float)(TICK_COUNT - 1);
        float a = deg * (float)M_PI / 180.0f;
        float ca = cosf(a), sa = sinf(a);
        d.color = (i >= 6) ? lv_color_hex(DISPLAY_COLOR_WPM_FILL_HIGH)
                           : lv_color_hex(DISPLAY_COLOR_TACH_TICK);
        d.p1.x = cx + ca * (ARC_R - 8);
        d.p1.y = cy + sa * (ARC_R - 8);
        d.p2.x = cx + ca * (ARC_R + 3);
        d.p2.y = cy + sa * (ARC_R + 3);
        lv_draw_line(layer, &d);
    }
}

static void wpm_border_render(void) {
    float ratio = displayed_wpm / (float)WPM_BORDER_MAX;
    if (ratio > 1.0f) {
        ratio = 1.0f;
    }
    int value = (int)(ratio * ARC_RANGE + 0.5f);
    lv_color_t fill = tach_color(ratio);

    struct zmk_widget_wpm_border *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        lv_arc_set_value(widget->arc, value);
        lv_obj_set_style_arc_color(widget->arc, fill, LV_PART_INDICATOR);

        char buf[6];
        snprintf(buf, sizeof(buf), "%d", (int)(displayed_wpm + 0.5f));
        lv_label_set_text(widget->wpm_num, buf);
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
    /* Tachometer: 8 o'clock (150°) clockwise over the top to 4 o'clock (30°),
     * leaving the bottom 120° open for the battery. rotation offsets nominal 0. */
    lv_arc_set_bg_angles(widget->arc, 0, 240);
    lv_arc_set_rotation(widget->arc, 150);

    lv_obj_set_style_arc_width(widget->arc, ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_color(widget->arc, lv_color_hex(DISPLAY_COLOR_WPM_RING_BG), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(widget->arc, true, LV_PART_MAIN);

    lv_obj_set_style_arc_width(widget->arc, ARC_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(widget->arc, lv_color_hex(DISPLAY_COLOR_WPM_FILL_LOW), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(widget->arc, true, LV_PART_INDICATOR);

    lv_obj_remove_style(widget->arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(widget->arc, LV_OBJ_FLAG_CLICKABLE);

    /* Redline band: a second, static, translucent-red arc over the top 15% of
     * the sweep (value 0.85..1.0 -> 354deg..390deg). rotation offsets nominal 0. */
    widget->redline = lv_arc_create(widget->obj);
    lv_obj_set_size(widget->redline, ARC_SIZE, ARC_SIZE);
    lv_obj_center(widget->redline);
    lv_obj_remove_style(widget->redline, NULL, LV_PART_KNOB);
    lv_obj_remove_style(widget->redline, NULL, LV_PART_INDICATOR);
    lv_obj_clear_flag(widget->redline, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(widget->redline, (int)(A0_DEG + SWEEP_DEG * 0.85f));  /* 354 */
    lv_arc_set_bg_angles(widget->redline, 0, (int)(SWEEP_DEG * 0.15f));       /* 36 */
    lv_obj_set_style_arc_width(widget->redline, ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_color(widget->redline, lv_color_hex(DISPLAY_COLOR_WPM_FILL_HIGH), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(widget->redline, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(widget->redline, false, LV_PART_MAIN);

    /* Tick marks: transparent overlay with a DRAW_MAIN handler. */
    widget->ticks = lv_obj_create(widget->obj);
    lv_obj_remove_style_all(widget->ticks);
    lv_obj_set_size(widget->ticks, ARC_SIZE, ARC_SIZE);
    lv_obj_center(widget->ticks);
    lv_obj_clear_flag(widget->ticks, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(widget->ticks, ticks_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    /* Numeric WPM readout near the top of the dial, with a small "WPM" unit. */
    widget->wpm_num = lv_label_create(widget->obj);
    lv_label_set_text(widget->wpm_num, "0");
    lv_obj_set_style_text_font(widget->wpm_num, &FG_Medium_26, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->wpm_num, lv_color_hex(DISPLAY_COLOR_TACH_NUM), LV_PART_MAIN);
    lv_obj_align(widget->wpm_num, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *unit = lv_label_create(widget->obj);
    lv_label_set_text(unit, "WPM");
    lv_obj_set_style_text_font(unit, &FG_Medium_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(unit, lv_color_hex(DISPLAY_COLOR_TACH_TICK), LV_PART_MAIN);
    lv_obj_align(unit, LV_ALIGN_TOP_MID, 0, 58);

    sys_slist_append(&widgets, &widget->node);
    widget_wpm_border_init();

    k_work_init_delayable(&wpm_smooth_work, wpm_smooth_work_handler);

    return 0;
}

lv_obj_t *zmk_widget_wpm_border_obj(struct zmk_widget_wpm_border *widget) {
    return widget->obj;
}
