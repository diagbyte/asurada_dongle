# Dongle Display — Phase 2: Swipe Carousel (Keyboard ⟷ Trackball) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Turn the single Asurada status screen into a horizontally-swipeable carousel: page 0 = the existing keyboard status widgets, page 1 = the Phase-1 rolling ball. Left/right touch swipe changes page; the ball timer pauses when its page is off-screen.

**Architecture:** A screen-manager builds one persistent "track" object (width = N × 240) holding N full-screen page objects laid out left-to-right; changing page animates the track's x. Pages are never destroyed (avoids the Phase-1 final-review N1 lifecycle hazard), so the ball stays a session singleton. `touch.c`'s existing left/right swipe is rebound from BLE-profile switching to `asurada_screens_page_prev/next`. Each page gets an activate/deactivate hook; the ball uses it to pause/resume its `lv_timer` (final-review N2).

**Tech Stack:** ZMK (`main`) / Zephyr 4.1, LVGL 9.x, nRF52840, GC9A01 240×240, C.

## Global Constraints

- LVGL 9.x APIs only; GC9A01 **240×240**. Screen coordinate width/height = 240.
- `touch.c` reads the CST816S directly (not via an LVGL indev), so the carousel is driven MANUALLY from resolved gestures — do NOT rely on LVGL's built-in scroll/snap.
- All LVGL work on the display work queue. Screen-manager API called from `status_screen.c` (build, on the display WQ) and from `touch.c`'s gesture work handler (already marshalled to the system WQ → must re-marshal to the display WQ before touching LVGL, exactly as `touch.c` already does for brightness/screensaver).
- Pages are persistent (built once, never deleted). Keyboard status widgets keep their current construction; they just get reparented onto page 0 instead of directly on the screen.
- No local build. Verification = CI (push branch; a totem-cfg build pointed at `revision: dongle-display-ui` is already wired on branch `ci-display-phase1`; push there to trigger Actions; confirm `totem_dongle asurada_adapter` green) + deferred hardware checks.
- Follow module precedents: `src/touch.c` (gesture → work-queue), `src/screensaver.c` (lv_scr_load, work-queue marshalling), `src/layouts/asurada/status_screen.c` (screen assembly), LVGL anim usage in `battery_circles.c` / `radii/modifier_indicator.c`.

---

## File Structure

- `boards/shields/asurada_adapter/include/asurada_screens.h` — screen-manager API (new).
- `boards/shields/asurada_adapter/src/screens.c` — track/pages/paging + display-WQ marshalling (new, `src/`-level → explicit CMake source).
- `boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c` — build the track, place existing widgets on page 0, ball on page 1 (modify).
- `boards/shields/asurada_adapter/src/layouts/asurada/ball.h` / `ball.c` — add `set_active(bool)` to pause/resume the timer (modify).
- `boards/shields/asurada_adapter/src/touch.c` — rebind left/right swipe to page nav (modify).
- `boards/shields/asurada_adapter/CMakeLists.txt` — compile `src/screens.c` (modify).

---

## Task 1: Screen manager (persistent paged track)

**Files:**
- Create: `boards/shields/asurada_adapter/include/asurada_screens.h`
- Create: `boards/shields/asurada_adapter/src/screens.c`
- Modify: `boards/shields/asurada_adapter/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `lv_obj_t *asurada_screens_init(lv_obj_t *screen, int n_pages);` — builds the track on `screen`, returns the track; pages are `asurada_screens_page(i)`.
  - `lv_obj_t *asurada_screens_page(int i);` — the i-th page object (parent for that page's widgets).
  - `void asurada_screens_page_next(void);` / `void asurada_screens_page_prev(void);` — advance/retreat with wrap; animate; safe to call from any thread (re-marshals to the display WQ).
  - `typedef void (*asurada_page_activate_cb)(int page, bool active);` + `void asurada_screens_set_activate_cb(asurada_page_activate_cb cb);` — invoked on the display WQ when the active page changes (old page → active=false, new page → active=true).

- [ ] **Step 1: Header**

Create `include/asurada_screens.h`:
```c
#pragma once
#include <lvgl.h>
#include <stdbool.h>

typedef void (*asurada_page_activate_cb)(int page, bool active);

/* Build a horizontally-paged track filling `screen`; returns the track obj. */
lv_obj_t *asurada_screens_init(lv_obj_t *screen, int n_pages);
/* The i-th full-screen page object (use as parent for that page's widgets). */
lv_obj_t *asurada_screens_page(int i);
/* Change page (wraps). Safe from any thread — marshals to the display WQ. */
void asurada_screens_page_next(void);
void asurada_screens_page_prev(void);
/* Registered cb fires on the display WQ when the active page changes. */
void asurada_screens_set_activate_cb(asurada_page_activate_cb cb);
```

- [ ] **Step 2: Implementation**

Create `src/screens.c`:
```c
#include <zephyr/kernel.h>
#include <lvgl.h>
#include <zmk/display.h>

#include "asurada_screens.h"

#define SCREEN_W 240
#define MAX_PAGES 4

static lv_obj_t *track;
static lv_obj_t *pages[MAX_PAGES];
static int page_count;
static int active_page;
static asurada_page_activate_cb activate_cb;

static void anim_x_cb(void *obj, int32_t v) {
    lv_obj_set_x((lv_obj_t *)obj, v);
}

/* Runs on the display work queue. */
static void go_to(int page) {
    if (page_count == 0) {
        return;
    }
    page = ((page % page_count) + page_count) % page_count; /* wrap */
    int prev = active_page;
    active_page = page;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, track);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_set_values(&a, lv_obj_get_x(track), -page * SCREEN_W);
    lv_anim_set_duration(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    if (activate_cb && prev != page) {
        activate_cb(prev, false);
        activate_cb(page, true);
    }
}

struct page_msg {
    struct k_work work;
    int delta;
};
static struct page_msg next_msg, prev_msg;

static void page_work(struct k_work *w) {
    struct page_msg *m = CONTAINER_OF(w, struct page_msg, work);
    go_to(active_page + m->delta);
}

lv_obj_t *asurada_screens_page(int i) {
    return pages[i];
}

lv_obj_t *asurada_screens_init(lv_obj_t *screen, int n_pages) {
    page_count = (n_pages > MAX_PAGES) ? MAX_PAGES : n_pages;
    active_page = 0;

    track = lv_obj_create(screen);
    lv_obj_remove_style_all(track);
    lv_obj_set_size(track, SCREEN_W * page_count, SCREEN_W);
    lv_obj_set_pos(track, 0, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < page_count; i++) {
        pages[i] = lv_obj_create(track);
        lv_obj_remove_style_all(pages[i]);
        lv_obj_set_size(pages[i], SCREEN_W, SCREEN_W);
        lv_obj_set_pos(pages[i], i * SCREEN_W, 0);
        lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    k_work_init(&next_msg.work, page_work); next_msg.delta = 1;
    k_work_init(&prev_msg.work, page_work); prev_msg.delta = -1;
    return track;
}

void asurada_screens_set_activate_cb(asurada_page_activate_cb cb) {
    activate_cb = cb;
    if (cb) {                       /* announce initial state */
        for (int i = 0; i < page_count; i++) {
            cb(i, i == active_page);
        }
    }
}

void asurada_screens_page_next(void) {
    k_work_submit_to_queue(zmk_display_work_q(), &next_msg.work);
}
void asurada_screens_page_prev(void) {
    k_work_submit_to_queue(zmk_display_work_q(), &prev_msg.work);
}
```

- [ ] **Step 3: Compile the source**

In `boards/shields/asurada_adapter/CMakeLists.txt`, next to the other unconditional `zephyr_library_sources(...)` (where `src/trackball_input.c` was added), add:
```cmake
zephyr_library_sources(src/screens.c)
```

- [ ] **Step 4: Verify — CI build green**

Push the branch and re-trigger the totem-cfg `ci-display-phase1` build. Expected: **green** (nothing calls the new API yet — this task only adds it). If `lv_anim_set_duration` / `lv_anim_path_ease_out` / `zmk_display_work_q` spellings differ, match `battery_circles.c` (anim) and `screensaver.c` (`zmk_display_work_q`).

- [ ] **Step 5: Commit**
```bash
git add boards/shields/asurada_adapter/include/asurada_screens.h \
        boards/shields/asurada_adapter/src/screens.c \
        boards/shields/asurada_adapter/CMakeLists.txt
git commit -m "feat(display): paged-track screen manager for the carousel"
```

---

## Task 2: Ball pause/resume when its page is inactive

**Files:**
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/ball.h`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/ball.c`

**Interfaces:**
- Produces: `void zmk_widget_asurada_ball_set_active(struct zmk_widget_asurada_ball *w, bool active);` — pauses (`lv_timer_pause`) or resumes (`lv_timer_resume`) the ball's 33 ms timer.

- [ ] **Step 1: Header — declare the setter**

In `ball.h`, add after the existing prototypes:
```c
void zmk_widget_asurada_ball_set_active(struct zmk_widget_asurada_ball *w, bool active);
```

- [ ] **Step 2: Implementation**

In `ball.c`, add (after `zmk_widget_asurada_ball_obj`):
```c
void zmk_widget_asurada_ball_set_active(struct zmk_widget_asurada_ball *w, bool active) {
    if (!w->timer) {
        return;
    }
    if (active) {
        lv_timer_resume(w->timer);
    } else {
        lv_timer_pause(w->timer);
    }
}
```

- [ ] **Step 3: Verify — CI build green**

Re-trigger CI. Expected: green. `lv_timer_pause` / `lv_timer_resume` are used in `screensaver.c` — match those spellings.

- [ ] **Step 4: Commit**
```bash
git add boards/shields/asurada_adapter/src/layouts/asurada/ball.h \
        boards/shields/asurada_adapter/src/layouts/asurada/ball.c
git commit -m "feat(display): pause the ball timer when its page is inactive"
```

---

## Task 3: Assemble the carousel (keyboard page 0, trackball page 1)

**Files:**
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c`

**Interfaces:**
- Consumes: `asurada_screens_*` (Task 1), `zmk_widget_asurada_ball_set_active` (Task 2), the existing `zmk_widget_*_init` widgets.

- [ ] **Step 1: Read the current file**

READ `src/layouts/asurada/status_screen.c`. It currently inits wpm_border, battery_circles, layer_center (and the Phase-1 ball) directly on `screen`. You will re-parent them onto pages.

- [ ] **Step 2: Rewrite `zmk_display_status_screen()`**

Replace the body so it builds a 2-page track and places widgets per page. Keep every existing widget's `_init`, only changing the parent from `screen` to a page. Add the ball on page 1 and wire the activate callback to pause it off-page:
```c
#include "asurada_screens.h"
#include "ball.h"
/* ... existing widget includes ... */

static struct zmk_widget_asurada_ball ball_widget;
/* ... existing widget statics (wpm_border, battery_circles, layer_center) ... */

static void on_page_active(int page, bool active) {
    if (page == 1) {                 /* trackball page */
        zmk_widget_asurada_ball_set_active(&ball_widget, active);
    }
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);

    asurada_screens_init(screen, 2);
    lv_obj_t *kb = asurada_screens_page(0);
    lv_obj_t *tb = asurada_screens_page(1);

    /* Page 0: existing keyboard status widgets, re-parented onto `kb`. */
    zmk_widget_wpm_border_init(&wpm_border_widget, kb);
    zmk_widget_battery_circles_init(&battery_circles_widget, kb);
    zmk_widget_layer_center_init(&layer_center_widget, kb);

    /* Page 1: the rolling ball. */
    zmk_widget_asurada_ball_init(&ball_widget, tb);

    asurada_screens_set_activate_cb(on_page_active);
    return screen;
}
```
(Use the EXACT existing widget struct names / init signatures from the current file — do not rename them. If a widget's `_init` also aligns itself relative to its parent, that still works since each page is a full 240×240 object.)

- [ ] **Step 3: Verify — CI build green**

Re-trigger CI. Expected: green.

- [ ] **Step 4: Verify — hardware (deferred)**

When the dongle is built: on boot the keyboard status shows (page 0). No swipe wiring yet (Task 4) — this step only confirms page 0 still renders as before and the ball lives on page 1.

- [ ] **Step 5: Commit**
```bash
git add boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c
git commit -m "feat(display): assemble 2-page carousel (keyboard + trackball)"
```

---

## Task 4: Rebind left/right swipe to page navigation

**Files:**
- Modify: `boards/shields/asurada_adapter/src/touch.c`

**Interfaces:**
- Consumes: `asurada_screens_page_next/prev` (Task 1).

- [ ] **Step 1: Read the current handler**

READ `src/touch.c`. `gesture_work_handler()` currently maps `G_SWIPE_LEFT → zmk_ble_prof_prev()` and `G_SWIPE_RIGHT → zmk_ble_prof_next()` (~lines 78-83).

- [ ] **Step 2: Rebind the two swipe cases**

Add the include and change only the two swipe cases (leave tap / long-press / up-down brightness untouched):
```c
#include "asurada_screens.h"
```
```c
    case G_SWIPE_LEFT:
        asurada_screens_page_prev();
        break;
    case G_SWIPE_RIGHT:
        asurada_screens_page_next();
        break;
```
Remove the now-unused `#include <zmk/ble.h>` ONLY if nothing else in the file uses it (grep `zmk_ble` / `zmk_endpoints` in the file first; if the tap/long-press paths don't use it, drop it, else keep). BLE profile switching remains available from the keyboard ADJ layer (`&bt BT_NXT/BT_PRV`).

- [ ] **Step 3: Verify — CI build green**

Re-trigger CI. Expected: green.

- [ ] **Step 4: Verify — hardware (deferred)**

When built: swipe left/right cycles keyboard ⟷ trackball pages with an ease-out slide; the ball animates only while its page is active (pauses off-page). Swipe up/down still changes brightness; tap wakes; long-press → screensaver.

- [ ] **Step 5: Commit**
```bash
git add boards/shields/asurada_adapter/src/touch.c
git commit -m "feat(display): left/right swipe navigates the carousel (was BLE profile)"
```

---

## Phase 2 done

Swiping left/right moves between the keyboard status page and the rolling-ball
page, the ball pauses when off-screen, and pages are never destroyed (no
lifecycle hazard). **Phase 3** adds the Connections page (3rd page) + the
context-dependent battery; **Phase 2b** fills the keyboard page with the
tachometer WPM + English modifier text (both their own plans). N1 from the
Phase-1 review is neutralized by the persistent-page design; N2 is handled by
the pause-on-inactive hook.

## Self-Review

- **Spec coverage:** carousel + swipe rebind (spec §3) → Tasks 1,3,4; ball moved
  to its own page (spec §5) → Task 3; ball pause (final-review N2) → Tasks 2,3;
  N1 lifecycle hazard avoided by never destroying pages (design choice, §
  Architecture). Deferred by design: Connections page (Phase 3), keyboard-page
  tach + modifiers (Phase 2b), `AUTO_PAGE_FOLLOW` (Phase 5).
- **Placeholders:** none — every step has complete code. "Match the precedent
  spelling" notes name a specific in-repo file to copy from, for symbols CI will
  confirm (`lv_anim_*`, `lv_timer_pause/resume`, `zmk_display_work_q`).
- **Type consistency:** `asurada_screens_page(i)` returns `lv_obj_t*` used as the
  widget parent in Task 3; `asurada_page_activate_cb` signature matches
  `on_page_active`; `zmk_widget_asurada_ball_set_active(struct ..._ball*, bool)`
  declared in ball.h (Task 2) and called in status_screen.c (Task 3) with the
  static `ball_widget`.
- **Threading:** `page_next/prev` submit to `zmk_display_work_q()`; the anim +
  `go_to` run there; `touch.c` calls them from its own work handler → correct
  (they re-marshal). Activate cb fires on the display WQ.
