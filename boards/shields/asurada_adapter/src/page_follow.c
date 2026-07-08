#include <zephyr/kernel.h>
#include <zephyr/input/input.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

#include "asurada_screens.h"

#define KEYBOARD_PAGE  0
#define TRACKBALL_PAGE 1

/* Trackball motion -> trackball page. Separate INPUT_EV_REL tap, independent of
 * src/trackball_input.c (Zephyr allows multiple input callbacks). */
static void pf_input_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);
    if (evt->type == INPUT_EV_REL) {
        asurada_screens_page_goto(TRACKBALL_PAGE);
    }
}
INPUT_CALLBACK_DEFINE(NULL, pf_input_cb, NULL);

/* Key press -> keyboard page. */
static int pf_key_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev != NULL && ev->state) {           /* on press */
        asurada_screens_page_goto(KEYBOARD_PAGE);
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(asurada_page_follow, pf_key_listener);
ZMK_SUBSCRIPTION(asurada_page_follow, zmk_keycode_state_changed);
