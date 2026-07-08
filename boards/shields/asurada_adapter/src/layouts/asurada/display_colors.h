#pragma once

/*
 * Asurada (Cyber Formula) palette.
 * The screen is a black round face with a cyan typing-speed border gauge and a
 * large centered layer name. Battery colors are reused from the Prospector
 * operator theme.
 */

/* WPM border gauge: dark background ring plus a speed-reactive fill that ramps
 * from a calm cyan (slow) to a bright cyan (fast). */
#define DISPLAY_COLOR_WPM_RING_BG   0x14242A
#define DISPLAY_COLOR_WPM_FILL_LOW  0x146A78

/* Tachometer fill ramp: calm cyan (slow) -> amber -> red redline (fast). */
#define DISPLAY_COLOR_WPM_FILL_MID  0xF5A623
#define DISPLAY_COLOR_WPM_FILL_HIGH 0xFF2D2D

/* Centered layer name */
#define DISPLAY_COLOR_LAYER_TEXT    0xEAFDFF

/* Modifier indicator: active = bright cyan, inactive = dim slate. */
#define DISPLAY_COLOR_MOD_ACTIVE    0x35E0FF
#define DISPLAY_COLOR_MOD_INACTIVE  0x35505C

/* Battery widget (reused Prospector operator palette) */
#define DISPLAY_COLOR_BATTERY_FILL     0x54806c
#define DISPLAY_COLOR_BATTERY_RING     0x2a4036
#define DISPLAY_COLOR_BATTERY_BG       0x505050
#define DISPLAY_COLOR_BATTERY_LABEL    0xffffff

#define DISPLAY_COLOR_BATTERY_DISCONNECTED_FILL  0x383c42
#define DISPLAY_COLOR_BATTERY_DISCONNECTED_RING  0x282c30
#define DISPLAY_COLOR_BATTERY_DISCONNECTED_LABEL 0x000000

#define DISPLAY_COLOR_BATTERY_LOW_FILL  0xC08040
#define DISPLAY_COLOR_BATTERY_LOW_RING  0x584028
