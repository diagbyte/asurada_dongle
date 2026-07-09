# Dongle Display — Phase 6: Trackball-page battery + bigger ball — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** (1) Show the **trackball's own battery** (icon + %) on the trackball page — the original "트랙볼 모드에선 트랙볼 배터리만" requirement, currently only present on the Connections page. (2) Make the **rolling ball noticeably bigger** by dropping the static ARGB8888 canvas buffer (RAM-capped the ball at 84px) in favor of an `LV_EVENT_DRAW_MAIN` draw like `field/line_segments.c`.

**Architecture:** Task 1 adds a small self-contained `trackball_battery.{c,h}` widget that mirrors `connections.c`'s two ZMK listeners but filters to a single slot (`CONFIG_ASURADA_TRACKBALL_SLOT`, default 2) and renders a battery glyph + %. Task 2 rewrites `ball.c`'s rotating-dot overlay: the transparent ARGB8888 canvas becomes a transparent child object with a `LV_EVENT_DRAW_MAIN` callback that draws each visible surface point as a tiny filled rect straight onto the draw layer (no static buffer) — exactly the pattern `line_segments.c` uses — so `BALL_SZ` can grow.

**Tech Stack:** ZMK (`main`) / Zephyr 4.1, LVGL 9.x, C.

## Global Constraints

- LVGL 9.x. Widgets built on the display work queue; new widgets in `src/layouts/asurada/` are auto-globbed by `CMakeLists.txt` (no CMake edit). `LV_USE_ARC`/`LV_USE_FLEX` are already selected for the asurada layout.
- Per-peripheral battery + connection events are ALREADY used by `connections.c` and `battery_circles.c` — mirror `connections.c`'s two `ZMK_DISPLAY_WIDGET_LISTENER`/`ZMK_SUBSCRIPTION` blocks and its `source`/`level`/`slot`/`connected` field usage EXACTLY (that file is the proven precedent; do not hand-write the macro/event lines).
- The DRAW_MAIN mechanics (get the layer via `lv_event_get_layer`, get obj coords, offset primitives by `obj_coords.x1/y1`, draw with `lv_draw_*`, drive redraws by `lv_obj_invalidate` from a timer) MUST mirror `src/layouts/field/line_segments.c` — it is the in-repo proof this compiles and performs on this hardware.
- No local build; verify via CI (`ci-display-phase1` → **all 5 targets incl. `totem_dongle` green**). The bigger ball must not overflow RAM — dropping the 28 KB static canvas frees memory, so CI RAM is expected to IMPROVE; confirm green. The trackball is connected on the user's current hardware, so the battery + ball are both hardware-verifiable immediately after flashing.
- Slot→device index (which split slot is the trackball) is a runtime/bonding fact, exposed as `CONFIG_ASURADA_TRACKBALL_SLOT` (default 2, matching the Connections page's row 2 = Trackball) so it is tunable without code changes.

---

## Task 1: Trackball-page battery widget

**Files:**
- Modify: `Kconfig` (repo root, inside `if SHIELD_ASURADA_ADAPTER`)
- Create: `boards/shields/asurada_adapter/src/layouts/asurada/trackball_battery.h`
- Create: `boards/shields/asurada_adapter/src/layouts/asurada/trackball_battery.c`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c`

**Interfaces:**
- Produces: `void zmk_widget_asurada_tb_battery_init(struct zmk_widget_asurada_tb_battery *w, lv_obj_t *parent);` + `lv_obj_t *zmk_widget_asurada_tb_battery_obj(struct zmk_widget_asurada_tb_battery *w);`

- [ ] **Step 1: Kconfig for the trackball slot.** Add inside `if SHIELD_ASURADA_ADAPTER` (near the other ASURADA_* options):
```kconfig
config ASURADA_TRACKBALL_SLOT
    int "Split peripheral slot index that is the trackball (its battery shows on the trackball page)"
    default 2
    help
      Which split-central peripheral slot the trackball bonds to (same index as
      the Connections page row). Confirm on hardware; change if the trackball
      battery shows on the wrong slot.
```

- [ ] **Step 2: Header** — `trackball_battery.h`:
```c
#pragma once
#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_asurada_tb_battery {
    sys_snode_t node;
    lv_obj_t *obj;      /* container (battery glyph + % label) */
    lv_obj_t *fill;     /* inner fill bar, width ∝ level */
    lv_obj_t *pct;      /* "NN%" / "--" label */
};

void zmk_widget_asurada_tb_battery_init(struct zmk_widget_asurada_tb_battery *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_tb_battery_obj(struct zmk_widget_asurada_tb_battery *w);
```

- [ ] **Step 3: Read `connections.c` first**, then create `trackball_battery.c` — copy its include block, its two `ZMK_DISPLAY_WIDGET_LISTENER`/`ZMK_SUBSCRIPTION` blocks and the `battery_update_state`/`connection_update_state` getters VERBATIM (only rename `connections_*`→`tb_battery_*`, `widget_asurada_connections_*`→`widget_asurada_tb_battery_*`), and keep the same `PERIPHERAL_COUNT` guard. Only the slot filter + rendering differ:
```c
#include "trackball_battery.h"

#include <zmk/display.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <zmk/event_manager.h>

#include <fonts.h>
#include "display_colors.h"

/* Battery of the trackball peripheral (CONFIG_ASURADA_TRACKBALL_SLOT), shown on
 * the trackball page as a small battery glyph + percentage. Mirrors
 * connections.c's dual ZMK listeners; only this one slot is displayed. */

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

#ifndef PERIPHERAL_COUNT
#define PERIPHERAL_COUNT ZMK_SPLIT_BLE_PERIPHERAL_COUNT
#endif

#define TB_SLOT      CONFIG_ASURADA_TRACKBALL_SLOT
#define BATT_HIGH    0x35C46B   /* >50% green  */
#define BATT_MID     0xF5A623   /* 20–50% amber */
#define BATT_LOW     0xF0564D   /* <20% red    */
#define BATT_OFF     0x505050   /* disconnected */
#define FILL_MAX_W   20         /* px, inner fill at 100% */

static uint8_t tb_level = 0;
static bool tb_connected = false;

struct battery_update_state { uint8_t source; uint8_t level; };
struct connection_update_state { uint8_t source; bool connected; };

static uint32_t level_color(uint8_t level) {
    if (level > 50) return BATT_HIGH;
    if (level > 20) return BATT_MID;
    return BATT_LOW;
}

static void tb_battery_render(void) {
    struct zmk_widget_asurada_tb_battery *w;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, w, node) {
        if (tb_connected) {
            int fw = (int)tb_level * FILL_MAX_W / 100;
            if (fw < 1) fw = 1;
            lv_obj_set_width(w->fill, fw);
            lv_obj_set_style_bg_color(w->fill, lv_color_hex(level_color(tb_level)), LV_PART_MAIN);
            lv_obj_set_style_bg_color(w->pct, lv_color_hex(level_color(tb_level)), LV_PART_MAIN); /* no-op if pct has no bg */
            lv_obj_set_style_text_color(w->pct, lv_color_hex(level_color(tb_level)), LV_PART_MAIN);
            char t[5];
            snprintf(t, sizeof(t), "%d%%", tb_level);
            lv_label_set_text(w->pct, t);
        } else {
            lv_obj_set_width(w->fill, 1);
            lv_obj_set_style_bg_color(w->fill, lv_color_hex(BATT_OFF), LV_PART_MAIN);
            lv_obj_set_style_text_color(w->pct, lv_color_hex(BATT_OFF), LV_PART_MAIN);
            lv_label_set_text(w->pct, "--");
        }
    }
}

static void set_battery_level(uint8_t source, uint8_t level) {
    if (source != TB_SLOT) return;
    tb_level = level;
    tb_battery_render();
}

static void set_connection_status(uint8_t source, bool connected) {
    if (source != TB_SLOT) return;
    tb_connected = connected;
    tb_battery_render();
}

void tb_battery_battery_update_cb(struct battery_update_state state) {
    set_battery_level(state.source, state.level);
}

static struct battery_update_state tb_battery_get_battery_state(const zmk_event_t *eh) {
    if (eh == NULL) return (struct battery_update_state){0, 0};
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    if (ev == NULL) return (struct battery_update_state){0, 0};
    return (struct battery_update_state){.source = ev->source, .level = ev->state_of_charge};
}

void tb_battery_connection_update_cb(struct connection_update_state state) {
    set_connection_status(state.source, state.connected);
}

static struct connection_update_state tb_battery_get_connection_state(const zmk_event_t *eh) {
    if (eh == NULL) return (struct connection_update_state){0, false};
    const struct zmk_split_central_status_changed *ev = as_zmk_split_central_status_changed(eh);
    if (ev == NULL) return (struct connection_update_state){0, false};
    return (struct connection_update_state){.source = ev->slot, .connected = ev->connected};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_tb_battery_battery, struct battery_update_state,
                            tb_battery_battery_update_cb, tb_battery_get_battery_state);
ZMK_SUBSCRIPTION(widget_asurada_tb_battery_battery, zmk_peripheral_battery_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_tb_battery_connection, struct connection_update_state,
                            tb_battery_connection_update_cb, tb_battery_get_connection_state);
ZMK_SUBSCRIPTION(widget_asurada_tb_battery_connection, zmk_split_central_status_changed);

void zmk_widget_asurada_tb_battery_init(struct zmk_widget_asurada_tb_battery *w, lv_obj_t *parent) {
    /* container: [ battery body (+nub) ] [ NN% ] laid out in a row */
    w->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(w->obj);
    lv_obj_set_size(w->obj, 74, 20);
    lv_obj_set_flex_flow(w->obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(w->obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(w->obj, 6, LV_PART_MAIN);
    lv_obj_clear_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

    /* battery body: bordered rounded rect */
    lv_obj_t *body = lv_obj_create(w->obj);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, 26, 14);
    lv_obj_set_style_radius(body, 3, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(body, lv_color_hex(0x9AB0B8), LV_PART_MAIN);
    lv_obj_set_style_border_opa(body, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 1, LV_PART_MAIN);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    /* inner fill bar, left-aligned, width ∝ level */
    w->fill = lv_obj_create(body);
    lv_obj_remove_style_all(w->fill);
    lv_obj_set_size(w->fill, 1, 8);
    lv_obj_align(w->fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(w->fill, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(w->fill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(w->fill, lv_color_hex(BATT_OFF), LV_PART_MAIN);

    /* positive nub on the right of the body */
    lv_obj_t *nub = lv_obj_create(w->obj);
    lv_obj_remove_style_all(nub);
    lv_obj_set_size(nub, 3, 7);
    lv_obj_set_style_radius(nub, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(nub, lv_color_hex(0x9AB0B8), LV_PART_MAIN);

    /* percentage label */
    w->pct = lv_label_create(w->obj);
    lv_label_set_text(w->pct, "--");
    lv_obj_set_style_text_font(w->pct, &FG_Medium_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(w->pct, lv_color_hex(BATT_OFF), LV_PART_MAIN);

    sys_slist_append(&widgets, &w->node);
    widget_asurada_tb_battery_battery_init();
    widget_asurada_tb_battery_connection_init();
    tb_battery_render();
}

lv_obj_t *zmk_widget_asurada_tb_battery_obj(struct zmk_widget_asurada_tb_battery *w) {
    return w->obj;
}
```
NOTE: the nub visually sits after the body in the flex row (right side) — acceptable. If `as_zmk_peripheral_battery_state_changed`/field names differ from what compiles, match `connections.c` exactly (it is known-green).

- [ ] **Step 4: Hook onto the trackball page.** In `status_screen.c`: add `#include "trackball_battery.h"`, a `static struct zmk_widget_asurada_tb_battery tb_battery_widget;`, and after the ball/pointing-mode inits on `tb`:
```c
    zmk_widget_asurada_tb_battery_init(&tb_battery_widget, tb);
    lv_obj_align(zmk_widget_asurada_tb_battery_obj(&tb_battery_widget), LV_ALIGN_TOP_MID, 0, 10);
```

- [ ] **Step 5: Verify — CI green.** Re-trigger `ci-display-phase1`; all 5 targets. Commit.
```bash
git add Kconfig boards/shields/asurada_adapter/src/layouts/asurada/trackball_battery.h \
        boards/shields/asurada_adapter/src/layouts/asurada/trackball_battery.c \
        boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c
git commit -m "feat(display): trackball-page battery (icon + %) for the trackball slot"
```

---

## Task 2: Bigger ball (drop the static canvas, draw via LV_EVENT_DRAW_MAIN)

**Files:**
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/ball.h`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/ball.c`

**Interfaces:** unchanged public API (`_init`/`_obj`/`_set_active`); `status_screen.c` is NOT touched here. Internal struct field `canvas` → `overlay`.

- [ ] **Step 1: Header** — in `ball.h`, bump the size and rename the overlay field:
```c
#define BALL_SZ 132            /* widget square, px. No static buffer anymore
                                * (dots drawn via LV_EVENT_DRAW_MAIN), so this is
                                * bounded by layout, not RAM. Tune to taste. */
```
and in the struct rename `lv_obj_t *canvas;` → `lv_obj_t *overlay;  /* transparent DRAW_MAIN layer for the dots */`.

- [ ] **Step 2: Rewrite the overlay rendering** in `ball.c`. Keep `build_points`, `mat_mul`, `apply_rot` unchanged. Remove the `canvas_buf` static array and the `redraw()` function; replace with a draw callback, and change `tick()` + `init()` to use it. Mirror `line_segments.c`'s draw-event mechanics exactly (`lv_event_get_layer`, `lv_obj_get_coords`, offset by `x1/y1`, `lv_draw_rect`, timer → `lv_obj_invalidate`):

Delete:
```c
/* Canvas backing buffer (ARGB8888), static like radii/layer_indicator.c. */
static uint8_t canvas_buf[LV_CANVAS_BUF_SIZE(BALL_SZ, BALL_SZ, 32, 1)];
```
Add near the other defines:
```c
#define DOT_R 1                 /* dot half-extent, px (3x3 filled dot) */
```
Replace `redraw()` with:
```c
static void ball_draw_cb(lv_event_t *e) {
    struct zmk_widget_asurada_ball *w = lv_event_get_user_data(e);
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int32_t ox = coords.x1, oy = coords.y1;

    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.bg_color = lv_color_make(255, 210, 200);
    dot.bg_opa = LV_OPA_COVER;

    const float *r = w->rot;
    for (int i = 0; i < n_pts; i++) {
        float x = pts[i][0], y = pts[i][1], z = pts[i][2];
        float Z = r[6] * x + r[7] * y + r[8] * z;
        if (Z <= 0.05f) continue;                    /* back-face cull */
        float X = r[0] * x + r[1] * y + r[2] * z;
        float Y = r[3] * x + r[4] * y + r[5] * z;
        int px = BALL_C + (int)(X * BALL_R);
        int py = BALL_C - (int)(Y * BALL_R);
        if (px < 0 || px >= BALL_SZ || py < 0 || py >= BALL_SZ) continue;
        dot.bg_opa = (lv_opa_t)(60 + Z * 195.0f);    /* depth shade */
        lv_area_t a = { ox + px - DOT_R, oy + py - DOT_R, ox + px + DOT_R, oy + py + DOT_R };
        lv_draw_rect(layer, &dot, &a);
    }
}
```
Change `tick()`'s final redraw from `redraw(w);` to:
```c
    lv_obj_invalidate(w->overlay);
```
(the rest of `tick()` — the fetch/decay/`apply_rot` — is unchanged, including the early `return` on idle so it stops invalidating when still.)

- [ ] **Step 3: Build the overlay object instead of the canvas** in `zmk_widget_asurada_ball_init`. Replace the canvas block:
```c
    /* Transparent canvas overlay for the rotating surface dots. */
    w->canvas = lv_canvas_create(w->cont);
    lv_canvas_set_buffer(w->canvas, canvas_buf, BALL_SZ, BALL_SZ, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_center(w->canvas);
    lv_canvas_fill_bg(w->canvas, lv_color_black(), LV_OPA_TRANSP);

    redraw(w);
    w->timer = lv_timer_create(tick, 33, w);
```
with:
```c
    /* Transparent overlay drawn on top of the base disc; dots are rendered in
     * its LV_EVENT_DRAW_MAIN handler (no static buffer). Created last -> on top. */
    w->overlay = lv_obj_create(w->cont);
    lv_obj_remove_style_all(w->overlay);
    lv_obj_set_size(w->overlay, BALL_SZ, BALL_SZ);
    lv_obj_center(w->overlay);
    lv_obj_clear_flag(w->overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(w->overlay, ball_draw_cb, LV_EVENT_DRAW_MAIN, w);

    w->timer = lv_timer_create(tick, 33, w);
```
Also enlarge the base disc's specular highlight offsets if they now look too small is NOT required — the highlight scales with `BALL_R` already.

- [ ] **Step 4: Verify — CI green.** Re-trigger `ci-display-phase1`; expect all 5 targets green and RAM usage DOWN (the 28 KB static buffer is gone). If `lv_draw_rect`'s signature or `lv_event_get_layer` differs, match `field/line_segments.c` exactly.

- [ ] **Step 5: Commit**
```bash
git add boards/shields/asurada_adapter/src/layouts/asurada/ball.h \
        boards/shields/asurada_adapter/src/layouts/asurada/ball.c
git commit -m "feat(display): bigger rolling ball via LV_EVENT_DRAW_MAIN (drop static canvas buffer)"
```

## Phase 6 done

The trackball page shows the trackball battery (top) and a larger rolling ball.
Both are hardware-verifiable immediately (the trackball is connected). If the ball
draw is too heavy at 132px/DOT_R 1 (watch for lag), raise `LAT_STEP`/`LON_STEP`
(fewer points) or keep `DOT_R` at 1. Pointing-mode text still sits below the ball
(CENTER +64); if the bigger ball crowds it, tune positions on hardware.

## Self-Review

- Spec coverage: trackball-page battery ("트랙볼 모드에선 트랙볼 배터리만") → Task 1;
  bigger ball ("구를 더 크게") → Task 2. Both were listed as remaining/optional and
  are now concrete.
- Placeholders: none; full code given. The "copy from connections.c" (events) and
  "mirror line_segments.c" (draw) notes name the proven in-repo precedents for the
  version-specific ZMK/LVGL symbols.
- Types: `zmk_widget_asurada_tb_battery` struct + init/obj consistent across header,
  source, and status_screen; ball struct field `canvas`→`overlay` updated in header +
  both use sites; `CONFIG_ASURADA_TRACKBALL_SLOT` int default 2.
- RAM: Task 2 REMOVES a 28 KB static buffer, so it strictly frees RAM — the bigger
  BALL_SZ has no buffer cost (DRAW_MAIN uses the shared display draw buffer).
- Risk: ball draw performance at ~360 dots/frame — mitigated by cheap square dots
  (no radius) and the LAT/LON step fallback; line_segments.c proves the pattern runs.
