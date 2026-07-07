#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_layer_center {
    sys_snode_t node;
    lv_obj_t *obj; /* label */
};

int zmk_widget_layer_center_init(struct zmk_widget_layer_center *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_layer_center_obj(struct zmk_widget_layer_center *widget);
