#pragma once
#include <lvgl.h>

#define BALL_SZ 84             /* widget/canvas square, px (RAM-bounded: ARGB8888
                                * canvas is BALL_SZ^2*4 bytes of static noinit;
                                * 84 -> ~28.2 KB. Phase-1.1: switch to an
                                * LV_EVENT_DRAW_MAIN draw like line_segments.c to
                                * drop the static buffer and allow a larger ball) */

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
