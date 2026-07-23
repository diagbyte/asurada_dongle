#include "batt_pulse.h"

/* Animate the battery body's border opacity so a critical (red) pack blinks. */
static void pulse_cb(void *obj, int32_t v) {
    lv_obj_set_style_border_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
}

void asurada_batt_pulse_set(lv_obj_t *body, bool critical) {
    if (!body) {
        return;
    }
    if (critical) {
        /* Only start if not already pulsing this object, so a repeat render
         * doesn't restart (and visibly reset) the blink. */
        if (lv_anim_get(body, pulse_cb) == NULL) {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, body);
            lv_anim_set_exec_cb(&a, pulse_cb);
            lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_30);
            lv_anim_set_time(&a, 550);
            lv_anim_set_playback_time(&a, 550);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_start(&a);
        }
    } else {
        lv_anim_del(body, pulse_cb);
        lv_obj_set_style_border_opa(body, LV_OPA_COVER, LV_PART_MAIN);
    }
}
