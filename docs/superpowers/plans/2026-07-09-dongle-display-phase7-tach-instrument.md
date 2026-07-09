# Dongle Display — Phase 7: Tachometer instrument accents + WPM number + wake text — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Close the remaining gaps between the approved HTML mockup and the firmware keyboard page: give the WPM tachometer the automotive-instrument look it has in the mockup — **8 tick marks around the arc, a red "redline" band on the top ~15%, and a numeric "NN WPM" readout** — and add the **"TAP TO WAKE"** hint text to the standby eyes screen. (The convex-dome eyes background is intentionally NOT done.)

**Architecture:** All tach work is folded into the existing `wpm_border.c` (which already owns the arc + WPM subscription + smoothing). The redline band is a second static translucent `lv_arc`; the 8 ticks are drawn with `lv_draw_line` in an `LV_EVENT_DRAW_MAIN` overlay (mirroring `field/line_segments.c`); the WPM number is a label updated in the existing render path. The wake text is one label added to `screensaver.c`'s eyes screen.

**Tech Stack:** ZMK (`main`) / Zephyr 4.1, LVGL 9.x, C.

## Global Constraints

- Mirror the approved mockup's tach geometry EXACTLY: arc starts at **8 o'clock (150°)** and sweeps **240°** clockwise to **4 o'clock (30°)** — already implemented as `lv_arc_set_rotation(150)` + `lv_arc_set_bg_angles(0,240)`. Ticks at `150° + 240°*(i/7)` for `i=0..7` (8 ticks); the last two (`i>=6`) are red. Redline band = the top **15%** of the sweep (value fraction 0.85→1.0 → `354°→390°`).
- LVGL angle convention (confirmed by `wpm_border.c`'s own working `rotation(270)`="12 o'clock"): **0°=3 o'clock, 90°=6 o'clock, 180°=9 o'clock, 270°=12 o'clock, increasing = clockwise**; effective angle = nominal + rotation. For `lv_draw_line` tick math use standard `cosf/sinf` (screen y grows down, so `+sin` matches this same clockwise convention).
- The `LV_EVENT_DRAW_MAIN` mechanics (`lv_event_get_layer`, `lv_obj_get_coords`, offset by `coords.x1/y1`, `lv_draw_line(layer,&dsc)`) MUST mirror `src/layouts/field/line_segments.c` — the proven in-repo precedent.
- No local build; verify via CI (`ci-display-phase1` → all 5 targets incl. `totem_dongle` green). Pixel positions/point sizes are tunable on hardware.
- Colors are `0xRRGGBB` in `layouts/asurada/display_colors.h`.

---

## Task 1: Tachometer instrument accents + WPM number

**Files:**
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/display_colors.h`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/wpm_border.h`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/wpm_border.c`

**Interfaces:** public API (`_init`/`_obj`) unchanged; `status_screen.c` NOT touched.

- [ ] **Step 1: Colors.** Add to `display_colors.h` (after the existing WPM/tach block):
```c
/* Tachometer accents: tick marks, redline band, and the numeric readout. */
#define DISPLAY_COLOR_TACH_TICK   0x5F8C96   /* normal tick / unit text (slate) */
#define DISPLAY_COLOR_TACH_NUM    0x35E0FF   /* big WPM number (cyan) */
/* (redline band + redline ticks reuse DISPLAY_COLOR_WPM_FILL_HIGH = 0xFF2D2D) */
```

- [ ] **Step 2: Struct fields.** In `wpm_border.h`, add to `struct zmk_widget_wpm_border` (alongside `arc`):
```c
    lv_obj_t *redline;     /* static translucent red band on the top ~15% */
    lv_obj_t *ticks;       /* transparent DRAW_MAIN overlay: 8 tick marks */
    lv_obj_t *wpm_num;     /* "NN" numeric readout */
```

- [ ] **Step 3: Redline band + ticks overlay + WPM label** in `wpm_border.c`.

Add the geometry defines near the top (after `ARC_RANGE`):
```c
#define TICK_COUNT   8
#define ARC_R        ((ARC_SIZE - ARC_WIDTH) / 2)   /* arc centre-line radius, px */
#define A0_DEG       150.0f                          /* 8 o'clock */
#define SWEEP_DEG    240.0f
```

Add the tick draw callback (mirror `line_segments.c`'s draw mechanics) above `wpm_border_render`:
```c
static void ticks_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    float cx = coords.x1 + ARC_SIZE / 2.0f;
    float cy = coords.y1 + ARC_SIZE / 2.0f;

    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.width = 2;

    for (int i = 0; i < TICK_COUNT; i++) {
        float deg = A0_DEG + SWEEP_DEG * (float)i / (float)(TICK_COUNT - 1);
        float a = deg * (float)M_PI / 180.0f;
        float ca = cosf(a), sa = sinf(a);
        d.color = (i >= 6) ? lv_color_hex(DISPLAY_COLOR_WPM_FILL_HIGH)
                           : lv_color_hex(DISPLAY_COLOR_TACH_TICK);
        d.p1.x = cx + ca * (ARC_R - 8);
        d.p1.y = cy + sa * (ARC_R - 8);
        d.p2.x = cx + ca * (ARC_R + 3);
        d.p2.y = cy + sa * (ARC_R + 3);
        lv_draw_line(layer, &d);
    }
}
```

In `zmk_widget_wpm_border_init`, AFTER the existing `widget->arc` is fully set up (after its KNOB removal / before `sys_slist_append`), add the redline arc, the ticks overlay, and the WPM number label:
```c
    /* Redline band: a second, static, translucent-red arc over the top 15% of
     * the sweep (value 0.85..1.0 -> 354deg..390deg). rotation offsets nominal 0. */
    widget->redline = lv_arc_create(widget->obj);
    lv_obj_set_size(widget->redline, ARC_SIZE, ARC_SIZE);
    lv_obj_center(widget->redline);
    lv_obj_remove_style(widget->redline, NULL, LV_PART_KNOB);
    lv_obj_remove_style(widget->redline, NULL, LV_PART_INDICATOR);
    lv_obj_clear_flag(widget->redline, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(widget->redline, (int)(A0_DEG + SWEEP_DEG * 0.85f));  /* 354 */
    lv_arc_set_bg_angles(widget->redline, 0, (int)(SWEEP_DEG * 0.15f));       /* 36 */
    lv_obj_set_style_arc_width(widget->redline, ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_color(widget->redline, lv_color_hex(DISPLAY_COLOR_WPM_FILL_HIGH), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(widget->redline, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(widget->redline, false, LV_PART_MAIN);

    /* Tick marks: transparent overlay with a DRAW_MAIN handler. */
    widget->ticks = lv_obj_create(widget->obj);
    lv_obj_remove_style_all(widget->ticks);
    lv_obj_set_size(widget->ticks, ARC_SIZE, ARC_SIZE);
    lv_obj_center(widget->ticks);
    lv_obj_clear_flag(widget->ticks, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(widget->ticks, ticks_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    /* Numeric WPM readout near the top of the dial, with a small "WPM" unit. */
    widget->wpm_num = lv_label_create(widget->obj);
    lv_label_set_text(widget->wpm_num, "0");
    lv_obj_set_style_text_font(widget->wpm_num, &FG_Medium_26, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->wpm_num, lv_color_hex(DISPLAY_COLOR_TACH_NUM), LV_PART_MAIN);
    lv_obj_align(widget->wpm_num, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *unit = lv_label_create(widget->obj);
    lv_label_set_text(unit, "WPM");
    lv_obj_set_style_text_font(unit, &FG_Medium_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(unit, lv_color_hex(DISPLAY_COLOR_TACH_TICK), LV_PART_MAIN);
    lv_obj_align(unit, LV_ALIGN_TOP_MID, 0, 58);
```
NOTE: include `<fonts.h>` if `wpm_border.c` doesn't already (it uses `FG_Medium_26`/`FG_Medium_20`); check the top of the file and add `#include <fonts.h>` if missing (these fonts are declared for this layout and used by sibling widgets).

- [ ] **Step 4: Update the number on render.** In `wpm_border_render()`, after the existing per-widget `lv_arc_set_value` / color loop, set the number text (use the smoothed `displayed_wpm`):
```c
        char buf[6];
        snprintf(buf, sizeof(buf), "%d", (int)(displayed_wpm + 0.5f));
        lv_label_set_text(widget->wpm_num, buf);
```
(Place it inside the existing `SYS_SLIST_FOR_EACH_CONTAINER` loop in `wpm_border_render`, next to where `widget->arc` is updated.)

- [ ] **Step 5: Verify — CI green.** Re-trigger `ci-display-phase1`; all 5 targets. If `lv_draw_line`/`lv_event_get_layer` differ, match `field/line_segments.c`; if `lv_arc_set_rotation` int args differ, match the existing `widget->arc` setup. Commit.
```bash
git add boards/shields/asurada_adapter/src/layouts/asurada/display_colors.h \
        boards/shields/asurada_adapter/src/layouts/asurada/wpm_border.h \
        boards/shields/asurada_adapter/src/layouts/asurada/wpm_border.c
git commit -m "feat(display): tachometer instrument accents (ticks, redline band, WPM readout)"
```

---

## Task 2: "TAP TO WAKE" text on the standby eyes

**Files:**
- Modify: `boards/shields/asurada_adapter/src/screensaver.c`

- [ ] **Step 1: Add the label** in `build_eyes_screen()`, after the eyes loop (before the `cur_angle`/`place_eyes` reset at the end). It sits top-centre, dim green, static:
```c
    lv_obj_t *wake = lv_label_create(eyes_screen);
    lv_label_set_text(wake, "TAP TO WAKE");
    lv_obj_set_style_text_font(wake, &FG_Medium_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(wake, lv_color_hex(0x3A8A46), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(wake, 2, LV_PART_MAIN);
    lv_obj_align(wake, LV_ALIGN_TOP_MID, 0, 30);
```
NOTE: `screensaver.c` must have `#include <fonts.h>` for `FG_Medium_20`; check the includes and add it if missing (it's declared for this layout). The label is parented to `eyes_screen`, so it only shows on the eyes screen and needs no per-tick update.

- [ ] **Step 2: Verify — CI green.** Re-trigger `ci-display-phase1`. Commit.
```bash
git add boards/shields/asurada_adapter/src/screensaver.c
git commit -m "feat(display): TAP TO WAKE hint on the standby eyes screen"
```

## Phase 7 done

The keyboard tach now reads like an automotive instrument (ticks + redline + numeric
WPM) and the eyes screen shows the wake hint — matching the approved mockup except the
(intentionally skipped) convex-dome eyes background. Tune tick length / label positions
on hardware.

## Self-Review

- Spec coverage: tach ticks + redline + WPM number (mockup gaps 1-3) → Task 1;
  TAP TO WAKE (gap 5) → Task 2. Convex dome (gap 4) intentionally excluded per the user.
- Placeholders: none; full code. The "mirror line_segments.c" (draw) + "match the
  existing arc setup" notes name the proven precedents for the version-specific LVGL API.
- Types: three new `lv_obj_t*` struct fields consistent header↔source; `ticks_draw_cb`
  is a standard `lv_event_cb`; the WPM label update reuses the existing `displayed_wpm`.
- Geometry: tick/redline angles derived from the same 150°/240° constants the arc
  already uses; redline rotation 354° + 36° span = top 15% of the sweep.
