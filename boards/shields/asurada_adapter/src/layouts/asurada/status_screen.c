#include <lvgl.h>

#include "wpm_border.h"
#include "layer_center.h"
#include "battery_circles.h"

#include <fonts.h>

static struct zmk_widget_wpm_border wpm_border_widget;
static struct zmk_widget_layer_center layer_center_widget;
static struct zmk_widget_battery_circles battery_circles_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

    /* Typing-speed gauge around the rim of the round display. */
    zmk_widget_wpm_border_init(&wpm_border_widget, screen);
    lv_obj_center(zmk_widget_wpm_border_obj(&wpm_border_widget));

    /* Small battery indicator near the bottom, inside the ring. */
    zmk_widget_battery_circles_init(&battery_circles_widget, screen);
    lv_obj_align(zmk_widget_battery_circles_obj(&battery_circles_widget), LV_ALIGN_BOTTOM_MID, 0, -26);

    /* Large layer name in the center, drawn on top. */
    zmk_widget_layer_center_init(&layer_center_widget, screen);
    lv_obj_align(zmk_widget_layer_center_obj(&layer_center_widget), LV_ALIGN_CENTER, 0, -8);

    return screen;
}
