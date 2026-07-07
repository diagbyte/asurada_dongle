#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

/* WPM value that fully fills the border ring. */
#define WPM_BORDER_MAX 120

struct zmk_widget_wpm_border {
    sys_snode_t node;
    lv_obj_t *obj; /* transparent full-screen container */
    lv_obj_t *arc; /* the rim gauge */
};

int zmk_widget_wpm_border_init(struct zmk_widget_wpm_border *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_wpm_border_obj(struct zmk_widget_wpm_border *widget);
