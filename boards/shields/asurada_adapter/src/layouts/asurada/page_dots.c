#include "page_dots.h"

#include <fonts.h>
#include "display_colors.h"

/*
 * Carousel position indicator: a row of dots (one per page), the active one
 * bright, the rest dim. Parented on the root screen (not the scrolling track)
 * so it stays fixed at the bottom while pages slide. Driven by the screens
 * activate callback (see status_screen.c). Dot sizes stay constant so the row
 * never re-flows; only the colour changes.
 */

#define DOT_SIZE     6
#define DOT_ON       DISPLAY_COLOR_LAYER_TEXT   /* active page */
#define DOT_OFF      0x33474F                   /* inactive: dim slate */

void zmk_widget_asurada_page_dots_init(struct zmk_widget_asurada_page_dots *w, lv_obj_t *parent,
                                       int n_pages) {
    w->n = (n_pages > ASURADA_MAX_PAGE_DOTS) ? ASURADA_MAX_PAGE_DOTS : n_pages;

    w->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(w->obj);
    lv_obj_set_size(w->obj, LV_SIZE_CONTENT, DOT_SIZE + 2);
    lv_obj_set_flex_flow(w->obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(w->obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(w->obj, 8, LV_PART_MAIN);
    lv_obj_clear_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < w->n; i++) {
        lv_obj_t *d = lv_obj_create(w->obj);
        w->dots[i] = d;
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, DOT_SIZE, DOT_SIZE);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(d, lv_color_hex(i == 0 ? DOT_ON : DOT_OFF), LV_PART_MAIN);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
    }
}

void zmk_widget_asurada_page_dots_set_active(struct zmk_widget_asurada_page_dots *w, int page) {
    for (int i = 0; i < w->n; i++) {
        lv_obj_set_style_bg_color(w->dots[i], lv_color_hex(i == page ? DOT_ON : DOT_OFF),
                                  LV_PART_MAIN);
    }
}

lv_obj_t *zmk_widget_asurada_page_dots_obj(struct zmk_widget_asurada_page_dots *w) {
    return w->obj;
}
