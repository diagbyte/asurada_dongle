#pragma once

/*
 * Asurada four-eyes idle screensaver.
 *
 * The screensaver is driven by ZMK activity state: when the dongle goes idle
 * the animated four eyes are shown and the backlight dims; when activity
 * resumes the status screen returns. These functions let the touch handler
 * override that: a tap peeks at the status screen, a long-press forces sleep.
 *
 * All functions are safe to call from any thread; they marshal the LVGL work
 * onto ZMK's display work queue internally.
 */

/* Tap-to-wake: temporarily show the status screen even while idle. */
void asurada_screensaver_wake(void);

/* Long-press: show the four-eyes screensaver immediately. */
void asurada_screensaver_force_sleep(void);
