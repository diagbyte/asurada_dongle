#pragma once
#include <lvgl.h>
#include <stdbool.h>

typedef void (*asurada_page_activate_cb)(int page, bool active);

/* Build a horizontally-paged track filling `screen`; returns the track obj. */
lv_obj_t *asurada_screens_init(lv_obj_t *screen, int n_pages);
/* The i-th full-screen page object (use as parent for that page's widgets). */
lv_obj_t *asurada_screens_page(int i);
/* Change page (wraps). Safe from any thread — marshals to the display WQ. */
void asurada_screens_page_next(void);
void asurada_screens_page_prev(void);
/* Registered cb fires on the display WQ when the active page changes. */
void asurada_screens_set_activate_cb(asurada_page_activate_cb cb);
