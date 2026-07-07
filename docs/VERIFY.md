# Verification

Because flashing needs the real dongle, verify in stages: get a clean build
first, optionally preview the UI on a PC, then confirm behavior on-device.

## 1. Build

Fastest path is GitHub Actions in your `zmk-config` (the standard ZMK build
workflow) after adding this module and the `asurada_adapter` shield.

Locally, with a ZMK west workspace on the **Zephyr 4.1** line:

```sh
west build -p -b xiao_ble//zmk -s zmk/app -- \
  -DSHIELD="[YOUR_KEYBOARD]_dongle asurada_adapter"
```

Fix build errors before anything else. Most likely culprits:

- **LVGL API drift** — this fork targets ZMK/Zephyr 4.1 (LVGL 9.3). If your ZMK
  revision differs, a few `lv_*` calls in `src/screensaver.c`
  (`lv_scr_act` / `lv_scr_load`) or the widgets may need the 9.x spelling
  (`lv_screen_active` / `lv_screen_load`). Pin ZMK to a Zephyr-4.1 commit.
- **`region 'RAM' overflowed`** — lower the display buffer in your `.conf`:
  `CONFIG_LV_Z_VDB_SIZE=25`.
- **CST816S device not found** — confirm `CONFIG_ASURADA_TOUCH=y` (it selects
  `CONFIG_INPUT_CST816S`) and the `cst816s` node is `okay`.

## 2. Optional: preview the UI on a PC (no hardware)

The status screen and the four-eyes animation are plain LVGL. To iterate on look
before flashing, drop `wpm_border.c`, `layer_center.c`, and the eye-drawing from
`screensaver.c` into an [LVGL simulator](https://github.com/lvgl/lv_port_pc_eclipse)
(240×240, feed a fake WPM/layer value). This lets you tune colors, the ring
width, fonts, and eye geometry with a fast edit loop.

## 3. Flash

Double-tap the XIAO reset to enter the UF2 bootloader, then drag the built
`zmk.uf2` onto the mounted drive.

## 4. On-device checklist

- [ ] Display lights up; colors look correct.
      If inverted, remove `display-inversion;` in `xiao_ble_zmk.overlay`
      (or add it, depending on your panel).
- [ ] Orientation is upright. If not, set `CONFIG_ASURADA_ROTATE_DISPLAY_180=y`.
- [ ] Center shows the active layer name; switching layers updates it.
- [ ] Typing fills the border ring; it rises fast and drains slowly, and the
      fill color brightens with speed.
- [ ] Swipe up/down changes brightness.
- [ ] Swipe left/right changes the BLE profile (host).
      If left/right or up/down feel swapped, adjust the sign tests in
      `src/touch.c` `classify_and_post()`.
- [ ] After `CONFIG_ZMK_IDLE_TIMEOUT` ms of no typing, the four eyes appear and
      the screen dims; they blink at random intervals.
- [ ] A tap peeks at the status screen; typing returns to it automatically.
- [ ] A long press shows the eyes immediately.

## 5. 3D core

- [ ] `openscad hardware/asurada_mount.scad` renders without errors for each
      `part` value.
- [ ] Cross-section / measure the bezel aperture (~`disp_active_dia`) and the
      board pocket against your real module before printing.
