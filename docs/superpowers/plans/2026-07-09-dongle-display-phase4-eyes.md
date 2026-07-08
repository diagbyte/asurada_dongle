# Dongle Display — Phase 4: Asurada green-LED standby eyes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Replace the placeholder four cyan capsule "eyes" in the screensaver with the confirmed Asurada design: four **green LED** eyes in a 2×2 cluster that idly **glance to a random angle (±30°) then hold** ("looking around"), with a green ring + glow.

**Architecture:** Only `src/screensaver.c` changes, and only its eye geometry + per-tick animation — the whole lifecycle (enter/exit on `zmk_activity_state_changed`, tap-to-wake/long-press via `touch.c`, brightness dim, the display work-queue marshalling, the PRNG) is kept as-is. Capsule blink → circular-LED render + a random-glance rotation of the 2×2 cluster (positions recomputed from an eased angle each tick).

**Tech Stack:** ZMK (`main`) / Zephyr 4.1, LVGL 9.x, nRF52840, C, `<math.h>` (FPU).

## Global Constraints

- LVGL 9.x only; 240×240 round face (centre at 120,120). All LVGL work stays on the display work queue where `blink_timer`/`glance_timer` already runs.
- Keep every non-eye part of `screensaver.c` unchanged: `enter_eyes`/`exit_eyes`/`peek_*`, the `k_work` handlers, `activity_listener`, `ZMK_LISTENER`/`ZMK_SUBSCRIPTION`, `screensaver_init`, `asurada_screensaver_wake/force_sleep`, `xrand()`.
- `M_PI` needs the repo's guard (this module's libc lacks it — see `radii/layer_indicator.c` / `field/line_segments.c`).
- No local build; verify via CI (`ci-display-phase1` build → `totem_dongle` green) + hardware (idle → new eyes).

---

## Task 1: Green-LED glance eyes in `screensaver.c`

**Files:**
- Modify: `boards/shields/asurada_adapter/src/screensaver.c`

**Interfaces:** none exported changes — internal rewrite. The timer is still created in `enter_eyes` (rename `blink_timer`→`glance_timer` is optional; keep the name to minimize churn, just change its callback).

- [ ] **Step 1: Read the current file** to anchor the exact blocks being replaced (`#define EYE_*` + `eye_x/y/w/h` arrays; the `blink_phase/blink_countdown/glow` statics; `apply_eyes`; `blink_tick`; `build_eyes_screen`; and the `lv_timer_create(blink_tick, ...)` call).

- [ ] **Step 2: Replace the geometry defines + per-eye arrays**

Replace the block:
```c
#define EYE_COUNT 4
#define EYE_COLOR 0x35E0FF
#define BLINK_FRAMES 8
#define TICK_MS 45

/* Top-left position, width and (open) height of each eye on the 240x240 face. */
static const int16_t eye_x[EYE_COUNT] = { 74, 124,  82, 128 };
static const int16_t eye_y[EYE_COUNT] = { 126, 126, 92,  92 };
static const int16_t eye_w[EYE_COUNT] = { 42,  42,  30,  30 };
static const int16_t eye_h[EYE_COUNT] = { 24,  24,  16,  16 };
```
with:
```c
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define EYE_COUNT   4
#define EYE_GREEN   0x5FE23F   /* LED core */
#define EYE_RING    0xA8FF88   /* brighter ring */
#define TICK_MS     40
#define FACE_C      120        /* 240/2 face centre */
#define EYE_R       24         /* LED radius, px */
#define CLUSTER_R   40         /* centre -> each LED centre (2x2 spread) */
#define GLANCE_DEG  30         /* random glance amplitude, +/- degrees */
```

- [ ] **Step 3: Replace the animation statics**

Replace:
```c
static int blink_phase;
static int blink_countdown = 40;
static uint32_t glow;
```
with:
```c
static uint32_t glow;
static float cur_angle, tgt_angle;   /* radians; eased current + random target */
static int glance_countdown = 25;
```

- [ ] **Step 4: Replace `apply_eyes` with `place_eyes`**

Replace the whole `apply_eyes` function with:
```c
/* Position the four LEDs at the corners of a square rotated by `ang`. */
static void place_eyes(float ang) {
    for (int i = 0; i < EYE_COUNT; i++) {
        float a = ang + (float)(M_PI / 4.0 + i * (M_PI / 2.0));
        int cx = FACE_C + (int)(CLUSTER_R * cosf(a));
        int cy = FACE_C + (int)(CLUSTER_R * sinf(a));
        lv_obj_set_pos(eyes[i], cx - EYE_R, cy - EYE_R);
    }
}
```

- [ ] **Step 5: Replace `blink_tick` with `glance_tick`**

Replace the whole `blink_tick` function with:
```c
static void glance_tick(lv_timer_t *t) {
    ARG_UNUSED(t);

    /* Soft brightness pulse (triangle wave). */
    glow += 3;
    int g = glow % 120;
    int tri = (g < 60) ? g : (120 - g);
    lv_opa_t opa = (lv_opa_t)(205 + tri / 3);   /* ~205..225 */
    for (int i = 0; i < EYE_COUNT; i++) {
        lv_obj_set_style_bg_opa(eyes[i], opa, LV_PART_MAIN);
    }

    /* Ease the cluster toward the current glance target. */
    cur_angle += (tgt_angle - cur_angle) * 0.12f;
    place_eyes(cur_angle);

    /* Option B: hold, then glance to a new random angle every ~1.2-4.5 s. */
    if (--glance_countdown <= 0) {
        float frac = (float)(xrand() % 2001) / 1000.0f - 1.0f;   /* -1..1 */
        tgt_angle = frac * (float)(GLANCE_DEG * M_PI / 180.0);
        glance_countdown = 30 + (xrand() % 82);                  /* 1.2..4.5 s */
    }
}
```

- [ ] **Step 6: Replace `build_eyes_screen`**

Replace the whole `build_eyes_screen` function with:
```c
static void build_eyes_screen(void) {
    eyes_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(eyes_screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(eyes_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(eyes_screen, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < EYE_COUNT; i++) {
        eyes[i] = lv_obj_create(eyes_screen);
        lv_obj_remove_style_all(eyes[i]);
        lv_obj_set_size(eyes[i], EYE_R * 2, EYE_R * 2);
        lv_obj_set_style_radius(eyes[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(eyes[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(eyes[i], lv_color_hex(EYE_GREEN), LV_PART_MAIN);
        lv_obj_set_style_border_color(eyes[i], lv_color_hex(EYE_RING), LV_PART_MAIN);
        lv_obj_set_style_border_width(eyes[i], 3, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(eyes[i], lv_color_hex(EYE_GREEN), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(eyes[i], 14, LV_PART_MAIN);
        lv_obj_set_style_shadow_spread(eyes[i], 2, LV_PART_MAIN);
        lv_obj_clear_flag(eyes[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    cur_angle = 0.0f;
    tgt_angle = 0.0f;
    glance_countdown = 25;
    place_eyes(0.0f);
}
```

- [ ] **Step 7: Point the timer at `glance_tick`**

In `enter_eyes`, change the timer creation from `blink_tick` to `glance_tick`:
```c
            blink_timer = lv_timer_create(glance_tick, TICK_MS, NULL);
```
(Keep the `blink_timer` variable name and the resume/pause logic as-is — only the callback changes.)

- [ ] **Step 8: Verify — CI green.** Re-trigger `ci-display-phase1`; expect `totem_dongle` green. If `lv_obj_set_style_shadow_*` / `LV_PART_MAIN` spellings differ, match a widget in `src/layouts/asurada/` that uses shadows/styles.

- [ ] **Step 9: Verify — hardware.** Leave the dongle idle (or long-press to force the screensaver). Expected: four green LEDs in a 2×2 cluster, glowing, that occasionally rotate a bit (±30°) and settle — "looking around." Tap or move the trackball wakes back to the status screen.

- [ ] **Step 10: Commit**
```bash
git add boards/shields/asurada_adapter/src/screensaver.c
git commit -m "feat(display): Asurada green-LED standby eyes with random glance"
```

## Phase 4 done

The screensaver matches the confirmed Asurada face (green LEDs, 2×2, random ±30°
glance). Convex-dome background is an optional later polish (LVGL radial gradient)
— the glowing LEDs on black already read well on the round panel.

## Self-Review

- Spec coverage: standby eyes (spec §9) → Task 1 (green LEDs, 2×2, Option-B random
  glance, glow). Convex dome explicitly deferred as polish.
- Placeholders: none; all replacement code complete. The one "match the shadow
  style spelling" note names in-repo files for a CI-confirmed symbol.
- Lifecycle untouched: enter/exit/peek/activity-listener/init and `xrand` are not
  modified; only geometry + the tick callback change. `M_PI` guarded.
