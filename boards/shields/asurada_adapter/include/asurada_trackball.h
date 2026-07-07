#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Fetch-and-clear trackball motion accumulated from forwarded INPUT_EV_REL
 * events since the last call. Returns true if there was motion. */
bool asurada_trackball_fetch(int32_t *dx, int32_t *dy);
