# Asurada dongle — hardware / 3D

`asurada_mount.scad` is the **functional mechanical core** of the dongle. It is
intentionally *not* the finished Asurada head — the organic head shape is meant
to be sculpted in Blender/Fusion around the `shell_ring` this file exports.

## Parts it targets

- **Display:** Waveshare 1.28" round LCD, GC9A01, 240×240 (touch version).
- **MCU:** Seeed XIAO nRF52840.

## Before printing — verify dimensions

Every value marked `(!)` in the `.scad` is a best-guess default. Measure your
actual parts (calipers, or the Waveshare mechanical drawing) and update:

- `disp_glass_dia`, `disp_board_dia`, `disp_board_th`, `disp_stack_th`
- `xiao_usb_w`, `xiao_usb_h`, `xiao_usb_over`

The active-area diameter (`disp_active_dia = 32.4`) is standard for 240px GC9A01
and rarely needs changing.

## Usage (OpenSCAD)

1. Install [OpenSCAD](https://openscad.org/).
2. Open `asurada_mount.scad`. Set `part` at the top:
   - `"assembly"` — exploded view of all parts (sanity check).
   - `"bezel"` — front LCD holder (print face-down).
   - `"tray"` — XIAO carrier with USB-C slot.
   - `"shell_ring"` — the datum ring for the head shell.
3. Render (F6) → Export STL (F7).

## Integrating the Asurada head

1. Export `shell_ring` (and optionally `bezel`) as STL/STEP.
2. Import into Blender/Fusion as a fixed datum.
3. Sculpt the four-eyed Asurada head so its inner cavity mates to the ring and
   its front opening frames the round display aperture.
4. Keep the four eye positions consistent with the firmware screensaver
   (`src/screensaver.c`, `eye_x/eye_y/eye_w/eye_h`) if you want the printed face
   and the on-screen eyes to line up.

## Notes

- Standoff stack height is `standoff_h` (LCD back → tray). Increase it if your
  LCD flex/tail or wiring needs more room.
- Screw posts are sized for **M2 self-tapping** screws (`screw_d = 2.2`).
- This core has **no LED provisions** yet (LEDs were deferred). When added, route
  a WS2812 data line and leave a channel around the `shell_ring`.
