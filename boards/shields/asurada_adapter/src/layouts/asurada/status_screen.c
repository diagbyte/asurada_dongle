#include <lvgl.h>

#include "asurada_screens.h"
#include "wpm_border.h"
#include "layer_center.h"
#include "battery_circles.h"
#include "ball.h"
#include "pointing_mode.h"

#include <fonts.h>

static struct zmk_widget_wpm_border wpm_border_widget;
static struct zmk_widget_layer_center layer_center_widget;
static struct zmk_widget_battery_circles battery_circles_widget;
static struct zmk_widget_asurada_ball ball_widget;
static struct zmk_widget_asurada_pointing_mode pointing_mode_widget;

static void on_page_active(int page, bool active) {
    if (page == 1) {                 /* trackball page */
        zmk_widget_asurada_ball_set_active(&ball_widget, active);
    }
}

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

    asurada_screens_init(screen, 2);
    lv_obj_t *kb = asurada_screens_page(0);
    lv_obj_t *tb = asurada_screens_page(1);

    /* Page 0: existing keyboard status widgets, re-parented onto `kb`. */
    zmk_widget_wpm_border_init(&wpm_border_widget, kb);
    lv_obj_center(zmk_widget_wpm_border_obj(&wpm_border_widget));

    zmk_widget_battery_circles_init(&battery_circles_widget, kb);
    lv_obj_align(zmk_widget_battery_circles_obj(&battery_circles_widget), LV_ALIGN_BOTTOM_MID, 0, -26);

    zmk_widget_layer_center_init(&layer_center_widget, kb);
    lv_obj_align(zmk_widget_layer_center_obj(&layer_center_widget), LV_ALIGN_CENTER, 0, -8);

    /* Page 1: the rolling ball. */
    zmk_widget_asurada_ball_init(&ball_widget, tb);

    zmk_widget_asurada_pointing_mode_init(&pointing_mode_widget, tb);

    asurada_screens_set_activate_cb(on_page_active);
    return screen;
}
