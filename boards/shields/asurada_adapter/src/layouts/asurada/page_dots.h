#pragma once
#include <lvgl.h>
#include <zephyr/kernel.h>

#define ASURADA_MAX_PAGE_DOTS 4

struct zmk_widget_asurada_page_dots {
    lv_obj_t *obj;
    lv_obj_t *dots[ASURADA_MAX_PAGE_DOTS];
    int n;
};

void zmk_widget_asurada_page_dots_init(struct zmk_widget_asurada_page_dots *w, lv_obj_t *parent, int n_pages);
void zmk_widget_asurada_page_dots_set_active(struct zmk_widget_asurada_page_dots *w, int page);
lv_obj_t *zmk_widget_asurada_page_dots_obj(struct zmk_widget_asurada_page_dots *w);
