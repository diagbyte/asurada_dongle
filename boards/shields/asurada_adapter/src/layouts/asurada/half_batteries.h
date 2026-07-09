#pragma once
#include <lvgl.h>
#include <zephyr/kernel.h>

#define ASURADA_HALF_COUNT 2   /* left + right keyboard halves = split slots 0, 1 */

struct zmk_widget_asurada_half_batteries {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *fill[ASURADA_HALF_COUNT];   /* inner fill bar, width proportional to level */
    lv_obj_t *pct[ASURADA_HALF_COUNT];    /* "NN%" / "--" */
};

void zmk_widget_asurada_half_batteries_init(struct zmk_widget_asurada_half_batteries *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_half_batteries_obj(struct zmk_widget_asurada_half_batteries *w);
