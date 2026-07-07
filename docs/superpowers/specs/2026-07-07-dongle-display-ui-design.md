# Asurada Dongle Display UI — Design Spec

**Date:** 2026-07-07
**Status:** Approved (design confirmed); ready for implementation planning
**Module:** `asurada_dongle` (Prospector fork), shield `asurada_adapter`
**Interactive mockup:** https://claude.ai/code/artifact/99a60d75-7338-4222-b304-8ab356f36498

## 1. Goal

Turn the single Asurada status screen into a **three-screen, automotive-HUD
(Cyber Formula) instrument cluster** driven by live keyboard + trackball state,
navigated by touch swipe, plus a **redesigned standby "eyes" screensaver** that
matches the real Asurada head.

This runs on the `totem_dongle` central (the dongle built as
`totem_dongle asurada_adapter`). It is a display/UX feature inside this module;
it does not change the keymap or split topology, which already exist.

## 2. Constraints & platform facts

- **Panel:** GC9A01, 240×240 round, RGB565, MIPI-DBI SPI 4-wire @ 30 MHz.
- **UI:** LVGL 9.x on Zephyr 4.1 / ZMK `main`. Dedicated display work queue.
- **Budget:** full-frame redraw ≈ 31 ms (~30 fps ceiling); partial/dirty-rect
  flushes are far cheaper. `LV_Z_DOUBLE_VDB` + `LV_Z_FLUSH_THREAD` on.
  Confirmed ~30 Hz procedural-draw headroom (see `field/line_segments.c`, which
  does trig-heavy per-frame drawing at 33 ms and logs its own cost).
- **Widget pattern:** `struct zmk_widget_*` + `_init(widget,parent)` +
  `ZMK_DISPLAY_WIDGET_LISTENER`/`ZMK_SUBSCRIPTION`. A new `*.c` dropped in
  `src/layouts/asurada/` is auto-compiled (CMake glob); call its `_init()` from
  `src/layouts/asurada/status_screen.c`.
- **Input on the dongle:** trackball motion arrives over the split and is
  re-emitted locally by the `zmk,input-split` node (`&trackball_split`, defined
  in the user's `totem_dongle.overlay`) as `INPUT_EV_REL` / `INPUT_REL_X/Y`.
  Reachable exactly like `src/touch.c` uses `INPUT_CALLBACK_DEFINE` for the
  CST816S touch panel.
- **Screen switching:** `lv_scr_load` already used by `src/screensaver.c`
  (status screen ↔ eyes).
- **Touch gestures:** `src/touch.c` already classifies tap / long-press /
  up-down / left-right swipe.

## 3. Screens & navigation

Three screens in a horizontal, wrapping carousel:

```
[ Keyboard ] ⟷ [ Trackball ] ⟷ [ Connections ] ⟷ (wrap to Keyboard)
```

- **Left / right swipe** = page navigation. This **repurposes** the gesture
  currently bound to BLE-profile prev/next in `touch.c`. Profile switching is
  redundant here (the keyboard's ADJ layer has `&bt BT_NXT/BT_PRV/BT_CLR`), so
  it moves off touch.
- **Unchanged gestures:** up/down swipe = brightness; tap = wake; long-press =
  force standby.
- A small **screen manager** owns the three status screens and drives
  transitions (either three `lv_obj` screens via `lv_scr_load`, or one paged
  container). It must interoperate with `screensaver.c` (standby saves/restores
  the *current* page).

## 4. Screen 1 — Keyboard

- **Tachometer WPM (the automotive centerpiece):** a rim arc spanning **8
  o'clock → 4 o'clock (≈240°, open at the bottom)** — not a full ring. Tick
  marks around the arc, a **redline zone** on the top ~15%, and a fill whose
  **color tracks speed**: cyan (`0x35E0FF`, low) → amber → red (redline). This
  extends the existing `wpm_border.c` (which already lerps teal→cyan) with the
  arc-cut and the amber/red high end.
- **Layer name**, centered — reuse `layer_center.c` (BASE / NAV / SYM / ADJ /
  TVP1 / TVP2). "Keyboard layout" here means the **layer name**, not a rendered
  key grid.
- **Modifiers as English text** (`SHIFT` / `CTRL` / `ALT` / `GUI`), shown only
  while held, in the HUD cyan.
- **Battery:** Left + Right halves at the **bottom** — `L`/`R` abbreviation +
  battery-cell icon + `%`.
- **Removed:** output/host indicator (not shown anywhere).

## 5. Screen 2 — Trackball

- **Red 3D rolling sphere**, centered — the marquee element. Procedural
  approach (mirrors `field/line_segments.c`):
  - Accumulate forwarded trackball `dx/dy` into a virtual sphere orientation
    (yaw/pitch).
  - Each frame: rotate a set of surface grid points, orthographically project,
    back-face cull, depth-shade; draw a red radial-gradient base + specular
    highlight + contact shadow.
  - ~30 Hz timer, `lv_obj_invalidate()` on the ball's bounding box only.
  - Momentum decay when input stops; gentle idle drift optional.
  - **Fallback** if perf/appearance disappoint: in-plane image rotation
    (`radii/layer_indicator.c` pattern) — spins toward the drag vector rather
    than a true roll.
  - Physical ball is **red**, so the on-screen sphere is red (ties to the HUD's
    red accents).
- **Pointing-mode text** (`SCROLL` / `SNIPE`), shown while the corresponding
  layer is active (SCRLM=6 / SCRLH=7 → "SCROLL"; SNIPE=8 → "SNIPE").
- **Battery:** Trackball only.

## 6. Screen 3 — Connections

- Three rows — **Left**, **Right**, **Trackball** — each prefixed by a
  **status dot: green = connected, red = disconnected**, followed by battery %.
  Disconnected shows `--`.
- No host/output row. Wording is "Left"/"Right"/"Trackball" (not "half").

## 7. Battery display — context-dependent

| Screen | Batteries shown |
|---|---|
| Keyboard | Left + Right (bottom, `L`/`R` + %) |
| Trackball | Trackball only |
| Connections | All three + connection dots |

The dongle already knows each peripheral's level via
`ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING` (enabled in the totem config).
**[Risk to confirm in implementation]** the exact per-source battery API/event
symbol for reading an individual peripheral's charge on the central.

## 8. Trackball input tap

New source (e.g. `src/trackball_input.c`, `src/`-level like `touch.c`):

- `INPUT_CALLBACK_DEFINE(NULL, cb, NULL)` tapping all input devices and
  filtering `INPUT_EV_REL` (decoupled from the user's node name), or
  `DEVICE_DT_GET(DT_NODELABEL(trackball_split))` for the specific node.
- Accumulate `dx/dy` on the driver thread; marshal to `zmk_display_work_q()`
  before touching LVGL (same discipline as `touch.c` / `screensaver.c`).
- Feed the ball widget and (if `AUTO_PAGE_FOLLOW`) the screen manager.
- Ensure `CONFIG_INPUT=y` on the dongle independent of `ASURADA_TOUCH`.

## 9. Standby "eyes" — replaces the current screensaver

Redesign `src/screensaver.c`'s current **four cyan blinking capsules** into the
**Asurada face**:

- **4 green LED "eyes"** in a 2×2 cluster, **centered**, generously spaced,
  each with a **bright green ring** + glow, on a **convex-dome background**
  (off-center top-left highlight + edge vignette so the black face reads as a
  bulging lens). **Eyes only** inside the round screen — the red side panels and
  grey housing of the reference are the physical head *outside* the display.
- **Motion (Option B):** eyes rest still, then **glance to a random angle within
  ±30° every ~1.2–4.5 s and hold** — an "AI looking around" feel — plus a subtle
  brightness pulse. (No continuous spin; no per-eye blink.)
- **Wake hint text** at **top-center**.
- Keeps the existing lifecycle: entered/exited by `zmk_activity_state_changed`,
  plus tap-to-wake / long-press-to-sleep from `touch.c`, and screensaver
  brightness dim.

## 10. Auto-follow-input option

`CONFIG_ASURADA_AUTO_PAGE_FOLLOW` (bool, **default n**):

- Trackball motion (REL events) → switch to the **Trackball** page.
- Key activity (keypress events, as WPM already consumes) → switch to the
  **Keyboard** page.
- **Connections** page is manual-only (not an auto target).
- Short cooldown (~a few seconds) after a manual swipe suppresses auto-switching
  to avoid flicker.

## 11. Data sources (all existing ZMK events)

| Datum | Source |
|---|---|
| WPM | ZMK WPM state (as `wpm_border.c`) |
| Layer name | `zmk_layer_state_changed` (as `layer_center.c`) |
| Modifiers | HID modifier state / modifier-indicator widget |
| Pointing mode | `zmk_layer_state_changed` on layers 6/7/8 |
| Batteries (per device) | split-central battery state *(symbol TBD)* |
| Connection up/down | peripheral connection state |
| Standby enter/exit | `zmk_activity_state_changed` |
| Trackball motion | Zephyr input `INPUT_EV_REL` on `&trackball_split` |

## 12. Kconfig & build

- `CONFIG_ASURADA_AUTO_PAGE_FOLLOW` (bool, default n) in the shield Kconfig.
- If the ball uses an LVGL canvas/image: `select LV_USE_CANVAS` / `LV_USE_IMAGE`
  in `src/layouts/asurada/Kconfig.defconfig` (as `radii` does).
- Guarantee `CONFIG_INPUT=y` for the trackball tap.
- New layout widgets are auto-globbed; the `src/`-level input source needs an
  explicit `zephyr_library_sources(...)` in the shield `CMakeLists.txt`.

## 13. Performance notes

- Ball and tach each confined to a small bounding box; invalidate only that
  region so LVGL reflows/flushes a sub-rect, not the whole 240×240.
- Ball ~30 Hz; surface-point density and grid step tuned on hardware to stay in
  the ~30 fps budget (borrow `line_segments.c`'s microsecond self-logging while
  tuning).
- Eyes glance is a cheap transform on 4 small objects.

## 14. Implementation phasing

1. **Trackball page + red 3D ball + input tap** (the original ask; end-to-end
   proof that forwarded REL reaches the display).
2. **Carousel + screen manager + Keyboard page** (tachometer, layer, modifiers)
   + rebind left/right swipe to page nav.
3. **Connections page + context-dependent battery + pointing-mode indicator.**
4. **Standby eyes redesign** in `screensaver.c`.
5. **`AUTO_PAGE_FOLLOW` option.**

Each phase should build green in CI (GitHub Actions) before the next.

## 15. Out of scope / deferred

- Standalone (direct-BLE) trackball firmware — dongle mode only.
- Exact pixel/geometry/color tuning — done on hardware against the mockup.
- Rendering an actual per-key keymap grid (explicitly not wanted).

## 16. Open items to resolve during implementation

- Exact per-peripheral battery-level API on the split central.
- Whether the screen manager uses three `lv_scr_load` screens or one paged
  container (interaction with `screensaver.c` save/restore).
- Final tach geometry (arc thickness, tick count, redline start) and ball point
  density — tuned on hardware.
