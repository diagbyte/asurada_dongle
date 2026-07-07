#pragma once
#include <lvgl.h>

#define BALL_SZ 130            /* widget/canvas square, px */

struct zmk_widget_asurada_ball {
    lv_obj_t *cont;            /* container */
    lv_obj_t *base;            /* static shaded red disc */
    lv_obj_t *canvas;          /* transparent overlay: rotating dots */
    lv_timer_t *timer;
    float rot[9];              /* 3x3 orientation matrix, row-major */
    float vx, vy;              /* angular momentum (rad/frame) */
};

void zmk_widget_asurada_ball_init(struct zmk_widget_asurada_ball *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_ball_obj(struct zmk_widget_asurada_ball *w);
