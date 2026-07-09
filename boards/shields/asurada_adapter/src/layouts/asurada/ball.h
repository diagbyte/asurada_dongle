#pragma once
#include <lvgl.h>

#define BALL_SZ 132            /* widget square, px. No static buffer anymore
                                * (dots drawn via LV_EVENT_DRAW_MAIN), so this is
                                * bounded by layout, not RAM. Tune to taste. */

struct zmk_widget_asurada_ball {
    lv_obj_t *cont;            /* container */
    lv_obj_t *base;            /* static shaded red disc */
    lv_obj_t *overlay;         /* transparent DRAW_MAIN layer for the dots */
    lv_timer_t *timer;
    float rot[9];              /* 3x3 orientation matrix, row-major */
    float vx, vy;              /* angular momentum (rad/frame) */
};

void zmk_widget_asurada_ball_init(struct zmk_widget_asurada_ball *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_ball_obj(struct zmk_widget_asurada_ball *w);
void zmk_widget_asurada_ball_set_active(struct zmk_widget_asurada_ball *w, bool active);
