#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

#define ASURADA_CONN_ROWS 4   /* max rows / label capacity; rendered = min(PERIPHERAL_COUNT, this) */

struct zmk_widget_asurada_connections {
    sys_snode_t node;
    lv_obj_t *obj;                 /* container */
    lv_obj_t *dot[ASURADA_CONN_ROWS];
    lv_obj_t *body[ASURADA_CONN_ROWS];  /* battery body; border tinted by level */
    lv_obj_t *fill[ASURADA_CONN_ROWS];  /* inner fill bar, width proportional to level */
};

void zmk_widget_asurada_connections_init(struct zmk_widget_asurada_connections *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_connections_obj(struct zmk_widget_asurada_connections *w);
