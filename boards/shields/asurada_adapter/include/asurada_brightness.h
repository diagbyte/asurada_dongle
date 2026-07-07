#pragma once

#include <stdint.h>

/*
 * Display backlight brightness control for the Asurada dongle.
 * Replaces Prospector's ambient-light auto-brightness with a fixed level that
 * the user adjusts via touch swipes, plus a dim/restore pair for the idle
 * screensaver.
 */

/* Set backlight brightness in percent (clamped to 1..100) and remember it as
 * the user's preferred level. */
void asurada_brightness_set(uint8_t percent);

/* Last applied brightness in percent. */
uint8_t asurada_brightness_get(void);

/* Adjust the preferred brightness by delta percent (up/down touch swipes),
 * clamped to [1, 100], and apply it. */
void asurada_brightness_adjust(int8_t delta);

/* Dim to `percent` without changing the preferred level (idle screensaver). */
void asurada_brightness_dim(uint8_t percent);

/* Restore brightness to the user's preferred level (screensaver exit). */
void asurada_brightness_restore(void);
