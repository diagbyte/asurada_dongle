#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_asurada_pointing_mode {
    sys_snode_t node;
    lv_obj_t *obj; /* label */
};

void zmk_widget_asurada_pointing_mode_init(struct zmk_widget_asurada_pointing_mode *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_pointing_mode_obj(struct zmk_widget_asurada_pointing_mode *widget);
