#include <zephyr/kernel.h>   /* IS_ENABLED() used below, before other headers pull it in */
#include <lvgl.h>

#include "asurada_screens.h"
#include "wpm_border.h"
#include "layer_center.h"
#include "half_batteries.h"
#include "lock_status.h"
#include "page_dots.h"
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
static struct zmk_widget_asurada_half_batteries half_batteries_widget;
static struct zmk_widget_asurada_lock_status lock_status_widget;
static struct zmk_widget_asurada_page_dots page_dots_widget;
#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
static struct zmk_widget_asurada_ball ball_widget;
static struct zmk_widget_asurada_tb_battery tb_battery_widget;
static struct zmk_widget_asurada_pointing_mode pointing_mode_widget;
#endif
static struct zmk_widget_asurada_connections connections_widget;
static struct zmk_widget_asurada_modifiers modifiers_widget;

static void on_page_active(int page, bool active) {
    if (active) {
        zmk_widget_asurada_page_dots_set_active(&page_dots_widget, page);
    }
#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
    if (page == 1) {                 /* trackball page */
        zmk_widget_asurada_ball_set_active(&ball_widget, active);
    }
#endif
}

/* --- Boot splash: a full-screen Asurada image shown once when the display first
 * powers on, then fades out to reveal the status screen underneath. --- */
extern const lv_image_dsc_t asurada_splash_img;  /* 240x240 RGB565, own .c */

static void splash_opa_cb(void *var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, LV_PART_MAIN);
}
static void splash_done_cb(lv_anim_t *a) {
    lv_obj_del((lv_obj_t *)a->var);
}
static void show_boot_splash(lv_obj_t *screen) {
    lv_obj_t *ov = lv_obj_create(screen);
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, LV_PCT(100), LV_PCT(100));
    lv_obj_center(ov);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, LV_PART_MAIN);

    /* Full-screen Asurada image, centered (the round panel crops the corners). */
    lv_obj_t *img = lv_image_create(ov);
    lv_image_set_src(img, &asurada_splash_img);
    lv_obj_center(img);

    /* Hold, then fade the whole card (bg + text) out and delete it. */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ov);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 500);
    lv_anim_set_delay(&a, 1600);
    lv_anim_set_exec_cb(&a, splash_opa_cb);
    lv_anim_set_ready_cb(&a, splash_done_cb);
    lv_anim_start(&a);
}

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

    /* Page 0: keyboard status widgets, re-parented onto `kb`. Vertical order
     * top->bottom: WPM readout (inside the tach), layer name, held modifiers,
     * CAPS/NUM locks, then the L/R half batteries. The 64px layer glyphs are
     * ~72px tall so gaps are tight; pixel-tune from a hardware photo. */
    zmk_widget_wpm_border_init(&wpm_border_widget, kb);
    lv_obj_center(zmk_widget_wpm_border_obj(&wpm_border_widget));

    zmk_widget_layer_center_init(&layer_center_widget, kb);
    lv_obj_align(zmk_widget_layer_center_obj(&layer_center_widget), LV_ALIGN_CENTER, 0, -18);

    zmk_widget_asurada_modifiers_init(&modifiers_widget, kb);
    lv_obj_align(zmk_widget_asurada_modifiers_obj(&modifiers_widget), LV_ALIGN_CENTER, 0, 22);

    zmk_widget_asurada_lock_status_init(&lock_status_widget, kb);
    lv_obj_align(zmk_widget_asurada_lock_status_obj(&lock_status_widget), LV_ALIGN_CENTER, 0, 46);

    /* Keyboard-half batteries only (L/R); the trackball has its own page. */
    zmk_widget_asurada_half_batteries_init(&half_batteries_widget, kb);
    lv_obj_align(zmk_widget_asurada_half_batteries_obj(&half_batteries_widget), LV_ALIGN_BOTTOM_MID, 0, -32);

#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
    /* Page 1: the rolling ball. */
    zmk_widget_asurada_ball_init(&ball_widget, tb);

    zmk_widget_asurada_pointing_mode_init(&pointing_mode_widget, tb);

    zmk_widget_asurada_tb_battery_init(&tb_battery_widget, tb);
    lv_obj_align(zmk_widget_asurada_tb_battery_obj(&tb_battery_widget), LV_ALIGN_BOTTOM_MID, 0, -32);
#endif

    /* Last page: connections (one row per split peripheral; labels via Kconfig). */
    zmk_widget_asurada_connections_init(&connections_widget, conn);

    /* Carousel position dots on the root screen (fixed; persists across pages).
     * Must exist before set_activate_cb, which announces the initial page. */
#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
    zmk_widget_asurada_page_dots_init(&page_dots_widget, screen, 3);
#else
    zmk_widget_asurada_page_dots_init(&page_dots_widget, screen, 2);
#endif
    lv_obj_align(zmk_widget_asurada_page_dots_obj(&page_dots_widget), LV_ALIGN_BOTTOM_MID, 0, -8);

    asurada_screens_set_activate_cb(on_page_active);

    /* Overlay the boot splash last so it sits on top of every page + the dots. */
    show_boot_splash(screen);
    return screen;
}
