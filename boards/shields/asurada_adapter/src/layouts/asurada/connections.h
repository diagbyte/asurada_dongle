#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

#define ASURADA_CONN_ROWS 3   /* Left, Right, Trackball */

struct zmk_widget_asurada_connections {
    sys_snode_t node;
    lv_obj_t *obj;                 /* container */
    lv_obj_t *dot[ASURADA_CONN_ROWS];
    lv_obj_t *pct[ASURADA_CONN_ROWS];
};

void zmk_widget_asurada_connections_init(struct zmk_widget_asurada_connections *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_connections_obj(struct zmk_widget_asurada_connections *w);
