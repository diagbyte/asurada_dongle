#include <zephyr/kernel.h>   /* IS_ENABLED() used below, before other headers pull it in */
#include <lvgl.h>

#include "asurada_screens.h"
#include "wpm_border.h"
#include "layer_center.h"
#include "battery_circles.h"
#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
#include "ball.h"
#include "trackball_battery.h"
#include "pointing_mode.h"
#endif
#include "connections.h"
#include "modifiers.h"

#include <fonts.h>

static struct zmk_widget_wpm_border wpm_border_widget;
static struct zmk_widget_layer_center layer_center_widget;
static struct zmk_widget_battery_circles battery_circles_widget;
#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
static struct zmk_widget_asurada_ball ball_widget;
static struct zmk_widget_asurada_tb_battery tb_battery_widget;
static struct zmk_widget_asurada_pointing_mode pointing_mode_widget;
#endif
static struct zmk_widget_asurada_connections connections_widget;
static struct zmk_widget_asurada_modifiers modifiers_widget;

#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
static void on_page_active(int page, bool active) {
    if (page == 1) {                 /* trackball page */
        zmk_widget_asurada_ball_set_active(&ball_widget, active);
    }
}
#endif

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
    asurada_screens_init(screen, 3);
    lv_obj_t *kb = asurada_screens_page(0);
    lv_obj_t *tb = asurada_screens_page(1);
    lv_obj_t *conn = asurada_screens_page(2);
#else
    asurada_screens_init(screen, 2);
    lv_obj_t *kb = asurada_screens_page(0);
    lv_obj_t *conn = asurada_screens_page(1);
#endif

    /* Page 0: existing keyboard status widgets, re-parented onto `kb`. */
    zmk_widget_wpm_border_init(&wpm_border_widget, kb);
    lv_obj_center(zmk_widget_wpm_border_obj(&wpm_border_widget));

    zmk_widget_battery_circles_init(&battery_circles_widget, kb);
    lv_obj_align(zmk_widget_battery_circles_obj(&battery_circles_widget), LV_ALIGN_BOTTOM_MID, 0, -26);

    zmk_widget_layer_center_init(&layer_center_widget, kb);
    lv_obj_align(zmk_widget_layer_center_obj(&layer_center_widget), LV_ALIGN_CENTER, 0, -34);

    zmk_widget_asurada_modifiers_init(&modifiers_widget, kb);
    lv_obj_align(zmk_widget_asurada_modifiers_obj(&modifiers_widget), LV_ALIGN_CENTER, 0, 8);

#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
    /* Page 1: the rolling ball. */
    zmk_widget_asurada_ball_init(&ball_widget, tb);

    zmk_widget_asurada_pointing_mode_init(&pointing_mode_widget, tb);

    zmk_widget_asurada_tb_battery_init(&tb_battery_widget, tb);
    lv_obj_align(zmk_widget_asurada_tb_battery_obj(&tb_battery_widget), LV_ALIGN_TOP_MID, 0, 10);
#endif

    /* Last page: connections (one row per split peripheral; labels via Kconfig). */
    zmk_widget_asurada_connections_init(&connections_widget, conn);

#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
    asurada_screens_set_activate_cb(on_page_active);
#endif
    return screen;
}
