# Dongle Display — Phase 3b: Connections page + tap-to-cycle navigation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Add a third carousel page — Connections (Left / Right / Trackball, each a green/red connection dot + battery %) — and make it reachable while the touch **swipe** is unavailable by making a **tap cycle pages** (tap already works; it wakes the screensaver, and when already awake it advances the page).

**Architecture:** A new `connections.c` widget mirrors `battery_circles.c`'s two ZMK listeners (`zmk_peripheral_battery_state_changed` for per-source level, `zmk_split_central_status_changed` for per-source connected) but renders three simple rows. The carousel grows to 3 pages. `touch.c`'s tap handler asks the screensaver whether it's showing (new `asurada_screensaver_is_active()`); if so, tap wakes it; otherwise tap advances the carousel (`asurada_screens_page_next`).

**Tech Stack:** ZMK (`main`) / Zephyr 4.1, LVGL 9.x, C.

## Global Constraints

- LVGL 9.x only; display work queue. Touch/gesture code runs on the system WQ (already marshalled); `asurada_screens_page_next` is thread-safe.
- Per-peripheral battery + connection events are ALREADY used by `src/layouts/asurada/battery_circles.c` — mirror its listener wiring and the `source`/`slot` field usage exactly. Slot→device mapping (0/1/2 → Left/Right/Trackball) is the split-central slot order; label them and confirm on hardware.
- No local build; verify via CI (`ci-display-phase1` → `totem_dongle` green) + hardware (tap cycles keyboard→trackball→connections; the Trackball row shows connected + battery; the halves show disconnected until built).
- Auto-follow (page_follow.c) still targets pages 0/1 only — connections (2) is reachable by tap, not auto-follow. That is correct.

---

## Task 1: Tap cycles pages (reachable without swipe)

**Files:**
- Modify: `boards/shields/asurada_adapter/include/asurada_screensaver.h`
- Modify: `boards/shields/asurada_adapter/src/screensaver.c`
- Modify: `boards/shields/asurada_adapter/src/touch.c`

**Interfaces:**
- Produces: `bool asurada_screensaver_is_active(void);` — true while the eyes screen is showing.

- [ ] **Step 1: Declare the query** in `include/asurada_screensaver.h` (next to `asurada_screensaver_wake`):
```c
/* True while the idle eyes screen is showing (not the status screen). */
bool asurada_screensaver_is_active(void);
```

- [ ] **Step 2: Implement it** in `src/screensaver.c` (near the other public functions; `showing_eyes` is the existing file-scope flag):
```c
bool asurada_screensaver_is_active(void) {
    return showing_eyes;
}
```

- [ ] **Step 3: Tap cycles the page when awake.** In `src/touch.c`, add the include and change ONLY the `G_TAP` case in `gesture_work_handler()`:
```c
#include "asurada_screens.h"
```
Current `G_TAP` case wakes the screensaver unconditionally. Replace it with:
```c
    case G_TAP:
#if IS_ENABLED(CONFIG_ASURADA_SCREENSAVER)
        if (asurada_screensaver_is_active()) {
            asurada_screensaver_wake();
            break;
        }
#endif
#if IS_ENABLED(CONFIG_ASURADA_STATUS_SCREEN_ASURADA)
        asurada_screens_page_next();
#endif
        break;
```
Leave long-press / up-down / left-right cases unchanged. (`asurada_screensaver.h` is already included by touch.c.)

- [ ] **Step 4: Verify — CI green.** Re-trigger `ci-display-phase1`.

- [ ] **Step 5: Verify — hardware.** With the status screen showing, tap → advances keyboard→trackball→connections→keyboard. With the eyes showing, tap → wakes to the status screen (does not also skip a page).

- [ ] **Step 6: Commit**
```bash
git add boards/shields/asurada_adapter/include/asurada_screensaver.h \
        boards/shields/asurada_adapter/src/screensaver.c \
        boards/shields/asurada_adapter/src/touch.c
git commit -m "feat(display): tap cycles carousel pages when awake (reach pages without swipe)"
```

---

## Task 2: Connections page (3rd carousel page)

**Files:**
- Create: `boards/shields/asurada_adapter/src/layouts/asurada/connections.h`
- Create: `boards/shields/asurada_adapter/src/layouts/asurada/connections.c`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c`

**Interfaces:**
- Consumes: the two ZMK events used by `battery_circles.c`.
- Produces: `void zmk_widget_asurada_connections_init(struct zmk_widget_asurada_connections *w, lv_obj_t *parent);`

- [ ] **Step 1: Read `battery_circles.c` in full** to copy the EXACT dual-listener wiring: the includes (`<zmk/events/battery_state_changed.h>`, `<zmk/events/split_peripheral_status_changed.h>` or whatever it actually uses for `zmk_peripheral_battery_state_changed` + `zmk_split_central_status_changed`), the two `ZMK_DISPLAY_WIDGET_LISTENER` + `ZMK_SUBSCRIPTION` blocks, the state structs (`source`/`level`, `slot`/`connected`), and `PERIPHERAL_COUNT`. Use those exact symbols.

- [ ] **Step 2: Header** — `connections.h`:
```c
#pragma once
#include <lvgl.h>
#include <zephyr/sys/util.h>

#define ASURADA_CONN_ROWS 3   /* Left, Right, Trackball */

struct zmk_widget_asurada_connections {
    sys_snode_t node;
    lv_obj_t *obj;                 /* container */
    lv_obj_t *dot[ASURADA_CONN_ROWS];
    lv_obj_t *pct[ASURADA_CONN_ROWS];
};

void zmk_widget_asurada_connections_init(struct zmk_widget_asurada_connections *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_connections_obj(struct zmk_widget_asurada_connections *w);
```

- [ ] **Step 3: Implementation** — `connections.c`. Build a title + 3 rows (each: a small circle `dot` + a static name label + a `pct` label). Keep per-source state arrays like `battery_circles.c`; on a battery event set that source's `pct`, on a connection event set that source's `dot` colour (green `0x35C46B` connected / red `0xF0564D` disconnected) and blank `pct` to "--" when disconnected. Mirror `battery_circles.c`'s two `ZMK_DISPLAY_WIDGET_LISTENER`/`ZMK_SUBSCRIPTION` blocks and its `SYS_SLIST_FOR_EACH_CONTAINER` update fan-out exactly — only the per-row rendering differs. Row labels: index 0 "Left", 1 "Right", 2 "Trackball" (confirm slot order on hardware). Build the rows with a vertical flex layout on the container.
  (Because the listener macro shape is version-specific, the concrete `ZMK_DISPLAY_WIDGET_LISTENER(...)` lines MUST be copied from `battery_circles.c`, not guessed. The plan intentionally does not hand-write them.)

- [ ] **Step 4: Grow the carousel + hook the page.** In `status_screen.c`:
  - change `asurada_screens_init(screen, 2)` → `asurada_screens_init(screen, 3)`.
  - `lv_obj_t *conn = asurada_screens_page(2);`
  - add `#include "connections.h"`, a `static struct zmk_widget_asurada_connections connections_widget;`, and `zmk_widget_asurada_connections_init(&connections_widget, conn);`.

- [ ] **Step 5: Verify — CI green.** Re-trigger `ci-display-phase1`. If any ZMK event symbol/header differs, match `battery_circles.c`.

- [ ] **Step 6: Verify — hardware.** Tap to the connections page: Trackball row shows a green dot + its battery %; Left/Right show red dots + "--" (halves not built yet).

- [ ] **Step 7: Commit**
```bash
git add boards/shields/asurada_adapter/src/layouts/asurada/connections.h \
        boards/shields/asurada_adapter/src/layouts/asurada/connections.c \
        boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c
git commit -m "feat(display): Connections page (Left/Right/Trackball dot + battery)"
```

## Phase 3b done

Three navigable pages (keyboard / trackball / connections), tap-cyclable without
touch swipe, connection + battery per device. Remaining: Phase 2b (keyboard-page
tachometer + English modifier text) and optional Phase 1.1 (bigger ball via
DRAW_MAIN) / convex-dome eyes.

## Self-Review

- Spec coverage: connections page + per-device dot/battery (spec §6) → Task 2;
  navigation to it (needed since swipe is broken) → Task 1 tap-cycle.
- Placeholders: none; the two "copy the listener block from battery_circles.c"
  notes name the precedent for the version-specific ZMK macro.
- Types: `asurada_screensaver_is_active()` bool; connections widget struct/init
  consistent across header/source/status_screen; page index 2 matches the
  3-page `asurada_screens_init`.
- Threading: tap runs on the gesture work handler → `asurada_screens_page_next`
  re-marshals to the display WQ; `showing_eyes` is a benign cross-context bool read.
