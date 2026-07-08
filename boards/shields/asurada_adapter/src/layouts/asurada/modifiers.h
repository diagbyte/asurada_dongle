#pragma once
#include <lvgl.h>
#include <zephyr/kernel.h>

#define ASURADA_MOD_COUNT 4   /* SHIFT, CTRL, ALT, GUI */

struct zmk_widget_asurada_modifiers {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *labels[ASURADA_MOD_COUNT];
};

void zmk_widget_asurada_modifiers_init(struct zmk_widget_asurada_modifiers *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_modifiers_obj(struct zmk_widget_asurada_modifiers *w);
