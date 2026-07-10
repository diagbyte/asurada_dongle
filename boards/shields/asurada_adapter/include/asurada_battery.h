#pragma once

#include <stdint.h>

/*
 * Cosmetic battery-percentage rescale for the display.
 *
 * ZMK's lithium curve reports 100% only at 4.20 V, but a fully-charged LiPo
 * settles to ~4.15 V at rest (and the XIAO divider reads a touch low), so the
 * reported state-of-charge tops out around 94% -- the gauge never shows 100.
 * Stretch the value so that ASURADA_BATT_FULL_PCT maps to 100 (low values are
 * barely affected), letting a charged pack read 100%. This is display-only and
 * intentionally optimistic vs. the true SoC.
 */
#define ASURADA_BATT_FULL_PCT 94

static inline uint8_t asurada_battery_display_pct(uint8_t raw) {
    uint32_t scaled = (uint32_t)raw * 100u / ASURADA_BATT_FULL_PCT;
    return scaled > 100u ? 100u : (uint8_t)scaled;
}
