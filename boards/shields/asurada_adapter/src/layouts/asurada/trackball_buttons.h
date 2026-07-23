#pragma once
#include <lvgl.h>
#include <zephyr/kernel.h>

#define ASURADA_TB_BTN_COUNT 6   /* trackball buttons = split key positions 38..43 */

struct zmk_widget_asurada_tb_buttons {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *arc[ASURADA_TB_BTN_COUNT];   /* pie/arc segment ("button") */
    lv_obj_t *lbl[ASURADA_TB_BTN_COUNT];   /* label inside the segment */
};

void zmk_widget_asurada_tb_buttons_init(struct zmk_widget_asurada_tb_buttons *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_tb_buttons_obj(struct zmk_widget_asurada_tb_buttons *w);
