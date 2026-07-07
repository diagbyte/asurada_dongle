# Asurada ZMK Dongle

A Cyber Formula **Asurada**-themed ZMK dongle display, forked from
[carrefinho/prospector-zmk-module](https://github.com/carrefinho/prospector-zmk-module)
(`feat/new-status-screens`). It runs on a **Seeed XIAO nRF52840** driving a
**Waveshare 1.28" round LCD (GC9A01, 240×240)** with a **CST816S** capacitive
touch panel, and acts as the BLE **split central** ("dongle") for an existing
split keyboard.

While you type, the rim of the round screen fills with a **typing-speed gauge**
and the current **layer name** shows large in the center. When idle, the display
switches to the animated **Asurada four eyes** instead of blanking. Touch adds
tap-to-wake, swipe-to-dim, and swipe-to-switch-profile.

> **Requires ZMK on Zephyr 4.1** (current `main`). The in-tree `galaxycore,gc9x01x`
> display driver and `hynitron,cst816s` touch driver only exist there.

## What changed vs. Prospector

| | Prospector | Asurada |
|---|---|---|
| Display | ST7789V 240×280 (1.69") | **GC9A01 240×240 (1.28")** |
| Touch | — | **CST816S (I2C)** |
| Brightness | APDS9960 auto | **fixed + touch swipe** |
| Typing UI | horizontal WPM bar | **border ring gauge** |
| Layer UI | roller / dots | **large centered name** |
| Idle | blank backlight | **four-eyes screensaver** |

## Hardware & wiring

XIAO nRF52840 pins (silkscreen `Dn`):

| Signal | Pin | | Signal | Pin |
|---|---|---|---|---|
| LCD SCK | D8 (P1.13) | | LCD CS | D9 |
| LCD MOSI | D10 (P1.15) | | LCD DC | D7 |
| LCD RST | D3 | | LCD BL | D6 (PWM, P1.11) |
| Touch SDA | D4 (P0.04) | | Touch SCL | D5 (P0.05) |
| Touch INT | D2 | | Touch RST | D1 |

Defined in `boards/shields/asurada_adapter/boards/xiao_ble_zmk.overlay`.

## Installation

In your **keyboard's** `zmk-config`, add this module to `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: diagbyte                            # <-- your GitHub user/org
      url-base: https://github.com/diagbyte
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main                            # Zephyr 4.1 line
      import: app/west.yml
    - name: asurada_dongle                      # <-- this repo
      remote: diagbyte
      revision: main
  self:
    path: config
```

Then build the dongle with the shield in `build.yaml`:

```yaml
include:
  - board: xiao_ble//zmk
    shield: [YOUR_KEYBOARD]_dongle asurada_adapter
```

`xiao_ble//zmk` selects the ZMK board variant whose overlay this shield targets.
See the ZMK [modules](https://zmk.dev/docs/features/modules) and
[dongle](https://zmk.dev/docs/development/hardware-integration) docs for the
`_dongle` central shield setup.

Give your keymap layers names so they show in the center:

```dts
keymap {
  compatible = "zmk,keymap";
  base { display-name = "Base"; bindings = < ... >; };
  nav  { display-name = "Nav";  bindings = < ... >; };
};
```

## Touch gestures

| Gesture | Action |
|---|---|
| Tap | Wake / peek at the status screen |
| Long press (≥0.6 s) | Show the four-eyes screensaver now |
| Swipe up / down | Brighter / dimmer backlight |
| Swipe left / right | Navigate display pages (asurada layout; BLE profile via keyboard ADJ) |

If a direction feels inverted on your build, flip the sign tests in
`src/touch.c` or set `CONFIG_ASURADA_ROTATE_DISPLAY_180=y`.

## Configuration (`.conf`)

| Option | Default | Notes |
|---|---|---|
| `CONFIG_ASURADA_FIXED_BRIGHTNESS` | 60 | Initial backlight (1–100) |
| `CONFIG_ASURADA_BRIGHTNESS_STEP` | 10 | Per-swipe brightness change |
| `CONFIG_ASURADA_TOUCH` | y | CST816S touch + gestures |
| `CONFIG_ASURADA_TOUCH_SWIPE_THRESHOLD` | 30 | Min swipe distance (px) |
| `CONFIG_ASURADA_SCREENSAVER` | y | Four-eyes idle screen |
| `CONFIG_ASURADA_SCREENSAVER_BRIGHTNESS` | 15 | Dim level while idle |
| `CONFIG_ASURADA_LAYER_NAME_UPPERCASE` | y | Uppercase layer names |
| `CONFIG_ASURADA_ROTATE_DISPLAY_180` | n | Flip if mounted upside down |
| `CONFIG_ZMK_IDLE_TIMEOUT` | 30000 | ms before idle → screensaver |

Other layouts from the Prospector fork (`classic`, `radii`, `field`, `operator`)
still exist and can be selected with e.g. `CONFIG_ASURADA_STATUS_SCREEN_RADII=y`,
but they were laid out for a 240×280 panel and are only partially adapted to the
240×240 round screen. The default is `CONFIG_ASURADA_STATUS_SCREEN_ASURADA`.

## 3D printing

`hardware/asurada_mount.scad` is the **parametric mechanical core**: LCD bezel,
XIAO tray, standoffs, and an outer registration ring. Verify the `(!)`-marked
dimensions against your parts, then export the `shell_ring` and build the
organic Asurada head around it in Blender/Fusion. See `hardware/README.md`.

## Firmware map

```
Kconfig                                   ASURADA_* options
boards/shields/asurada_adapter/
  boards/xiao_ble_zmk.overlay             GC9A01 + CST816S nodes, no ALS
  asurada_adapter.overlay                 chosen zephyr,display = &gc9a01
  Kconfig.defconfig                       display/LVGL + blank-on-idle defaults
  include/asurada_brightness.h            backlight control API
  include/asurada_screensaver.h           wake / force-sleep API
  src/brightness.c                        fixed + touch-driven brightness
  src/touch.c                             CST816S gesture handling
  src/screensaver.c                       four-eyes idle screen
  src/layouts/asurada/
    wpm_border.{c,h}                       typing-speed border gauge (lv_arc)
    layer_center.{c,h}                     large centered layer name
    battery_circles.{c,h}                  small battery indicator (reused)
    status_screen.c                        assembles the screen
    display_colors.h                       Asurada palette
```

## Verification

See `docs/VERIFY.md` for the full build + on-device checklist.

## Credits

Forked from **Prospector** by carrefinho
([hardware](https://github.com/carrefinho/prospector),
[firmware](https://github.com/carrefinho/prospector-zmk-module)), which is in
turn derived from englmaxi's `zmk-dongle-display`. Original module licensed
CERN-OHL-P-2.0 / MIT as noted in `LICENSE`.
