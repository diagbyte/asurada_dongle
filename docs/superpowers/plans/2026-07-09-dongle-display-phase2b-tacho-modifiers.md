# Dongle Display — Phase 2b: Keyboard-page tachometer + English modifiers — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Finish the keyboard page (page 0) to match the approved Asurada design: (1) reshape the WPM gauge from a full ring into an automotive **tachometer** arc that spans **8 o'clock → over the top → 4 o'clock** (leaving the bottom open for the battery) with a **speed-reactive redline color** (calm cyan when slow → amber → red at the top); (2) add an **English modifier indicator** (SHIFT / CTRL / ALT / GUI) that lights the active modifiers.

**Architecture:** Task 1 edits the existing `wpm_border.c` in place — only the arc geometry (rotation + bg angles) and the fill-color ramp change; its name, header, WPM subscription, smoothing work item, and the `status_screen.c` call all stay. Task 2 adds a new **self-contained** `modifiers.c` widget (does NOT depend on the config-gated `src/modifier_order.c`; it reads `zmk_hid_get_explicit_mods()` directly like `operator/modifier_indicator.c`'s core does) and hooks it onto the keyboard page. New palette entries go in `layouts/asurada/display_colors.h`.

**Tech Stack:** ZMK (`main`) / Zephyr 4.1, LVGL 9.x, C.

## Global Constraints

- LVGL 9.x arc angle convention (confirmed by the existing `wpm_border.c` comment "start at 12 o'clock, fill clockwise" with `rotation(270)`): **0° = 3 o'clock, 90° = 6 o'clock (bottom), 180° = 9 o'clock, 270° = 12 o'clock**, and increasing angle / value fills **clockwise**. Effective angle = nominal angle + rotation.
- The keyboard page is page 0; widgets are re-parented onto `kb` in `status_screen.c`. `LV_USE_FLEX` is now selected for the asurada layout (fixed in commit `37d130e`), so flex layout is available.
- No local build; verify via CI (`ci-display-phase1` → **all 5 targets incl. `totem_dongle` green**). Not hardware-testable without the keyboard halves — the WPM event and modifier state only change when a keymap is active, so verification is CI-green + a later hardware pass once the halves are built. Pixel positions are explicitly tunable later.
- Colors are `0xRRGGBB` literals in `layouts/asurada/display_colors.h`. Reuse the existing `DISPLAY_COLOR_WPM_*` names where they still apply; add new ones rather than renaming existing ones referenced elsewhere.
- Modifier state source: mirror `src/layouts/operator/modifier_indicator.c` lines 61-73 exactly for the `zmk_hid_get_explicit_mods()` read and the `MOD_LSFT|MOD_RSFT` (etc.) masks, and its `<zmk/hid.h>` / `<zmk/events/keycode_state_changed.h>` includes and `ZMK_SUBSCRIPTION(..., zmk_keycode_state_changed)`. Do NOT use `modifier_order.h`/`modifier_order_get*` (that helper only compiles under `CONFIG_ASURADA_SHOW_MODIFIERS`, and `modifiers.c` is auto-globbed unconditionally → would link-fail). Fixed label order is fine.

---

## Task 1: Tachometer gauge (reshape `wpm_border.c`)

**Files:**
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/display_colors.h`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/wpm_border.c`

**Interfaces:** unchanged — `zmk_widget_wpm_border_init` / `_obj` keep their signatures; `status_screen.c` is NOT touched in this task.

- [ ] **Step 1: Add the tach redline palette** to `layouts/asurada/display_colors.h`. Keep `DISPLAY_COLOR_WPM_RING_BG` and `DISPLAY_COLOR_WPM_FILL_LOW` as-is; add a mid + redline stop after the existing WPM block (around line 14):
```c
/* Tachometer fill ramp: calm cyan (slow) -> amber -> red redline (fast). */
#define DISPLAY_COLOR_WPM_FILL_MID  0xF5A623
#define DISPLAY_COLOR_WPM_FILL_HIGH 0xFF2D2D
```
NOTE: `DISPLAY_COLOR_WPM_FILL_HIGH` already exists (currently `0x35E0FF`). **Change** its value to `0xFF2D2D` (redline red); do not add a duplicate. `grep DISPLAY_COLOR_WPM_FILL_HIGH` across the tree first — it must be referenced only in `wpm_border.c`; if it is used anywhere else, stop and report instead of changing it.

- [ ] **Step 2: Reshape the arc geometry.** In `wpm_border.c`'s `zmk_widget_wpm_border_init`, replace the two lines
```c
    lv_arc_set_bg_angles(widget->arc, 0, 360);
    lv_arc_set_rotation(widget->arc, 270); /* start at 12 o'clock, fill clockwise */
```
with (8 o'clock start, 240° clockwise sweep over the top to 4 o'clock, bottom 120° open for the battery):
```c
    /* Tachometer: 8 o'clock (150°) clockwise over the top to 4 o'clock (30°),
     * leaving the bottom 120° open for the battery. rotation offsets nominal 0. */
    lv_arc_set_bg_angles(widget->arc, 0, 240);
    lv_arc_set_rotation(widget->arc, 150);
```

- [ ] **Step 3: 3-stop color ramp.** In `wpm_border.c`, replace the single `lerp_color(...)` call inside `wpm_border_render()`:
```c
    lv_color_t fill = lerp_color(DISPLAY_COLOR_WPM_FILL_LOW, DISPLAY_COLOR_WPM_FILL_HIGH, ratio);
```
with a piecewise cyan→amber→red ramp (keep the existing `lerp_color` helper; add this small helper just above `wpm_border_render`):
```c
static lv_color_t tach_color(float ratio) {
    if (ratio < 0.5f) {
        return lerp_color(DISPLAY_COLOR_WPM_FILL_LOW, DISPLAY_COLOR_WPM_FILL_MID, ratio * 2.0f);
    }
    return lerp_color(DISPLAY_COLOR_WPM_FILL_MID, DISPLAY_COLOR_WPM_FILL_HIGH, (ratio - 0.5f) * 2.0f);
}
```
and use it:
```c
    lv_color_t fill = tach_color(ratio);
```

- [ ] **Step 4: Verify — CI green.** Re-trigger `ci-display-phase1`; expect all 5 targets green. Commit.
```bash
git add boards/shields/asurada_adapter/src/layouts/asurada/display_colors.h \
        boards/shields/asurada_adapter/src/layouts/asurada/wpm_border.c
git commit -m "feat(display): tachometer WPM gauge (8->4 o'clock arc, cyan->amber->red redline)"
```

---

## Task 2: English modifier indicator (new self-contained widget)

**Files:**
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/display_colors.h`
- Create: `boards/shields/asurada_adapter/src/layouts/asurada/modifiers.h`
- Create: `boards/shields/asurada_adapter/src/layouts/asurada/modifiers.c`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c`

**Interfaces:**
- Produces: `void zmk_widget_asurada_modifiers_init(struct zmk_widget_asurada_modifiers *w, lv_obj_t *parent);` and `lv_obj_t *zmk_widget_asurada_modifiers_obj(struct zmk_widget_asurada_modifiers *w);`

- [ ] **Step 1: Modifier palette** — add to `layouts/asurada/display_colors.h` (values mirror the operator theme's intent, Asurada-tinted):
```c
/* Modifier indicator: active = bright cyan, inactive = dim slate. */
#define DISPLAY_COLOR_MOD_ACTIVE    0x35E0FF
#define DISPLAY_COLOR_MOD_INACTIVE  0x35505C
```

- [ ] **Step 2: Header** — `modifiers.h`:
```c
#pragma once
#include <lvgl.h>
#include <zephyr/sys/util.h>

#define ASURADA_MOD_COUNT 4   /* SHIFT, CTRL, ALT, GUI */

struct zmk_widget_asurada_modifiers {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *labels[ASURADA_MOD_COUNT];
};

void zmk_widget_asurada_modifiers_init(struct zmk_widget_asurada_modifiers *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_modifiers_obj(struct zmk_widget_asurada_modifiers *w);
```
NOTE: if `<zephyr/sys/util.h>` fails to provide `sys_snode_t` at CI time, use `#include <zephyr/kernel.h>` instead (that is what `battery_circles.h`/`connections.h` in this same layout do). Prefer whichever the sibling headers already use — check `connections.h` and match it.

- [ ] **Step 3: Implementation** — `modifiers.c`. Mirror `operator/modifier_indicator.c`'s state read (lines 61-73) and its `ZMK_DISPLAY_WIDGET_LISTENER`/`ZMK_SUBSCRIPTION` shape, but self-contained with 4 fixed labels and per-instance label storage:
```c
#include "modifiers.h"

#include <zmk/display.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/hid.h>

#include <fonts.h>
#include "display_colors.h"

/*
 * English modifier indicator for the keyboard page: SHIFT CTRL ALT GUI.
 * Reads the central's explicit HID modifier flags directly (no dependency on
 * the config-gated modifier_order helper) and colors each label active/inactive.
 * Order is fixed. Mirrors operator/modifier_indicator.c's HID read.
 */

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static const char *const mod_text[ASURADA_MOD_COUNT] = {"SHIFT", "CTRL", "ALT", "GUI"};

struct asurada_mod_state {
    bool active[ASURADA_MOD_COUNT];   /* [0]=shift [1]=ctrl [2]=alt [3]=gui */
};

static void modifiers_update_cb(struct asurada_mod_state state) {
    struct zmk_widget_asurada_modifiers *w;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, w, node) {
        for (int i = 0; i < ASURADA_MOD_COUNT; i++) {
            lv_color_t c = lv_color_hex(state.active[i] ? DISPLAY_COLOR_MOD_ACTIVE
                                                        : DISPLAY_COLOR_MOD_INACTIVE);
            lv_obj_set_style_text_color(w->labels[i], c, LV_PART_MAIN);
        }
    }
}

static struct asurada_mod_state modifiers_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();
    struct asurada_mod_state state = {.active = {false, false, false, false}};
    state.active[0] = (mods & (MOD_LSFT | MOD_RSFT)) != 0;
    state.active[1] = (mods & (MOD_LCTL | MOD_RCTL)) != 0;
    state.active[2] = (mods & (MOD_LALT | MOD_RALT)) != 0;
    state.active[3] = (mods & (MOD_LGUI | MOD_RGUI)) != 0;
    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_modifiers, struct asurada_mod_state,
                            modifiers_update_cb, modifiers_get_state)
ZMK_SUBSCRIPTION(widget_asurada_modifiers, zmk_keycode_state_changed);

void zmk_widget_asurada_modifiers_init(struct zmk_widget_asurada_modifiers *w, lv_obj_t *parent) {
    w->obj = lv_obj_create(parent);
    lv_obj_set_size(w->obj, 200, 26);
    lv_obj_set_style_bg_opa(w->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(w->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(w->obj, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(w->obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(w->obj, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < ASURADA_MOD_COUNT; i++) {
        w->labels[i] = lv_label_create(w->obj);
        lv_label_set_text(w->labels[i], mod_text[i]);
        lv_obj_set_style_text_font(w->labels[i], &FG_Medium_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(w->labels[i], lv_color_hex(DISPLAY_COLOR_MOD_INACTIVE), LV_PART_MAIN);
    }

    sys_slist_append(&widgets, &w->node);
    widget_asurada_modifiers_init();
}

lv_obj_t *zmk_widget_asurada_modifiers_obj(struct zmk_widget_asurada_modifiers *w) {
    return w->obj;
}
```
NOTE: `zmk_mod_flags_t` and the `MOD_LSFT`/`MOD_LCTL`/`MOD_LALT`/`MOD_LGUI` (+ `R*`) masks come from `<zmk/hid.h>` transitively, exactly as in `operator/modifier_indicator.c` (which includes only `<zmk/hid.h>` for them). If CI reports any of those undeclared, add the include `operator/modifier_indicator.c` uses — match that file, do not guess a dt-bindings path.

- [ ] **Step 4: Hook onto the keyboard page + make room.** In `status_screen.c`:
  - add `#include "modifiers.h"`;
  - add `static struct zmk_widget_asurada_modifiers modifiers_widget;`
  - nudge the layer name up and place the modifier row just under it, keeping the battery in the open bottom gap. Replace the existing layer-center align line
```c
    lv_obj_align(zmk_widget_layer_center_obj(&layer_center_widget), LV_ALIGN_CENTER, 0, -8);
```
with
```c
    lv_obj_align(zmk_widget_layer_center_obj(&layer_center_widget), LV_ALIGN_CENTER, 0, -34);

    zmk_widget_asurada_modifiers_init(&modifiers_widget, kb);
    lv_obj_align(zmk_widget_asurada_modifiers_obj(&modifiers_widget), LV_ALIGN_CENTER, 0, 2);
```
  (These offsets are a first pass — layer name ~y66-106, modifiers ~y108-134, battery top ~y152; tune on hardware.)

- [ ] **Step 5: Verify — CI green.** Re-trigger `ci-display-phase1`; expect all 5 targets green. If any modifier symbol is undeclared, match `operator/modifier_indicator.c`'s includes.

- [ ] **Step 6: Commit**
```bash
git add boards/shields/asurada_adapter/src/layouts/asurada/display_colors.h \
        boards/shields/asurada_adapter/src/layouts/asurada/modifiers.h \
        boards/shields/asurada_adapter/src/layouts/asurada/modifiers.c \
        boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c
git commit -m "feat(display): English modifier indicator (SHIFT/CTRL/ALT/GUI) on the keyboard page"
```

## Phase 2b done

The keyboard page shows the tachometer WPM gauge (8→4 o'clock, redline color) + layer
name + live modifier indicator + L/R battery. Combined with Phase 3a (pointing mode on
the trackball page) and Phase 3b (connections page + tap-cycle nav), the 3-page Asurada
carousel + eyes screensaver match the approved design. Remaining is optional polish only
(Phase 1.1 bigger ball via DRAW_MAIN; convex-dome eyes) and hardware tuning of pixel
positions once the keyboard halves are built.

## Self-Review

- Spec coverage: 8→4 o'clock tach with bottom open (spec: "8시~4시, 아래 배터리 부분 제외") → Task 1 Step 2; speed color / redline ("속도에 따라 색상 차이") → Task 1 Steps 1,3; English modifiers ("모디파이어 출력") → Task 2. Battery L/R on kb page already present (battery_circles).
- Placeholders: none; all code is concrete. The two "match operator/modifier_indicator.c" notes name the precedent for the version-specific HID symbols + display-listener macro.
- Types: `zmk_widget_asurada_modifiers` struct + init/obj consistent across header, source, and the status_screen call; `tach_color` returns `lv_color_t`; `ASURADA_MOD_COUNT` fixes all array sizes.
- Risk: `modifiers.c` is auto-globbed so it must NOT reference the config-gated `modifier_order` helper — Step 3 is self-contained by design (Global Constraints). Arc angle math re-derived from the existing file's own rotation(270)="12 o'clock" anchor.
