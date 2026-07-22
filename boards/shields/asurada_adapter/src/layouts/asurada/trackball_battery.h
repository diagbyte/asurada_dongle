#pragma once
#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_asurada_tb_battery {
    sys_snode_t node;
    lv_obj_t *obj;      /* container (battery glyph) */
    lv_obj_t *body;     /* battery body; border tinted by level */
    lv_obj_t *fill;     /* inner fill bar, width ∝ level */
};

void zmk_widget_asurada_tb_battery_init(struct zmk_widget_asurada_tb_battery *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_tb_battery_obj(struct zmk_widget_asurada_tb_battery *w);
