# Dongle Display — Phase 5: Auto Page-Follow (input-driven navigation) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Navigate the carousel by input-device activity instead of the (hardware-flaky) touch swipe: rolling the trackball shows the trackball page, typing shows the keyboard page.

**Architecture:** A new `src/page_follow.c` taps the trackball's forwarded `INPUT_EV_REL` (an extra `INPUT_CALLBACK_DEFINE`, independent of `trackball_input.c`) and subscribes to ZMK key events; each requests an absolute page via a new `asurada_screens_page_goto(page)`. The screen manager applies a short hysteresis cooldown so interleaved input doesn't flicker. Reliable because the trackball→display path is already proven (the ball renders/rolls on hardware); it does not depend on touch. Default **on** because the CST816S touch swipe is unreliable on the target Waveshare module.

**Tech Stack:** ZMK (`main`) / Zephyr 4.1, LVGL 9.x, nRF52840, C.

## Global Constraints

- LVGL 9.x only; work on the display work queue. Input callbacks (driver thread) and ZMK listeners (system WQ) must marshal to `zmk_display_work_q()` before touching LVGL — `asurada_screens_page_goto` does this internally.
- Pages: keyboard = index 0, trackball = index 1 (as assembled in `status_screen.c`). Auto-follow is only meaningful for the asurada carousel → gate on `CONFIG_ASURADA_STATUS_SCREEN_ASURADA`.
- No local build; verify via CI (push branch; totem-cfg `ci-display-phase1` build → `totem_dongle` green) + hardware.
- Follow precedents: `src/trackball_input.c` (INPUT_CALLBACK), `src/screensaver.c` (ZMK_LISTENER/ZMK_SUBSCRIPTION, work-queue), any widget using key events for the ZMK key event name.

---

## Task 1: `asurada_screens_page_goto` + hysteresis

**Files:**
- Modify: `include/asurada_screens.h`
- Modify: `src/screens.c`

**Interfaces:**
- Produces: `void asurada_screens_page_goto(int page);` — absolute page switch (wraps handled by go_to), safe from any thread, idempotent (no-op if already there), rate-limited by a cooldown so rapid alternating requests don't flicker.

- [ ] **Step 1: Declare in the header**

In `include/asurada_screens.h`, add after `asurada_screens_page_prev`:
```c
/* Absolute page switch (auto-follow). Any thread; idempotent; hysteresis-limited. */
void asurada_screens_page_goto(int page);
```

- [ ] **Step 2: Implement in `src/screens.c`**

Add the include (if not present) and, near the other static state, a cooldown + atomic target. Add the goto work + function; init the work in `asurada_screens_init`.
```c
#include <zephyr/sys/atomic.h>
```
Add to the static state block (near `active_page`):
```c
#define AUTO_COOLDOWN_MS 400
static int64_t last_switch;
static atomic_t goto_target = ATOMIC_INIT(-1);
static struct k_work goto_work_item;
```
Add the work handler + public function (place next to `page_work` / the `page_next/prev` functions):
```c
static void goto_work(struct k_work *w) {
    ARG_UNUSED(w);
    int p = (int)atomic_set(&goto_target, -1);
    if (p < 0 || p == active_page) {
        return;                       /* nothing to do / already there */
    }
    int64_t now = k_uptime_get();
    if (now - last_switch < AUTO_COOLDOWN_MS) {
        return;                       /* hysteresis: ignore rapid re-switch */
    }
    last_switch = now;
    go_to(p);
}

void asurada_screens_page_goto(int page) {
    atomic_set(&goto_target, page);
    k_work_submit_to_queue(zmk_display_work_q(), &goto_work_item);
}
```
In `asurada_screens_init`, alongside `k_work_init(&next_msg.work, page_work); ...`, add:
```c
    k_work_init(&goto_work_item, goto_work);
```

- [ ] **Step 3: Verify — CI green.** (Nothing calls it yet.) Re-trigger the `ci-display-phase1` build; expect `totem_dongle` green.

- [ ] **Step 4: Commit**
```bash
git add boards/shields/asurada_adapter/include/asurada_screens.h \
        boards/shields/asurada_adapter/src/screens.c
git commit -m "feat(display): asurada_screens_page_goto with hysteresis"
```

---

## Task 2: `page_follow.c` + Kconfig + CMake

**Files:**
- Create: `src/page_follow.c`
- Modify: `Kconfig` (repo root, in the `SHIELD_ASURADA_ADAPTER` guard)
- Modify: `boards/shields/asurada_adapter/CMakeLists.txt`

**Interfaces:**
- Consumes: `asurada_screens_page_goto` (Task 1).

- [ ] **Step 1: Kconfig option**

In the repo-root `Kconfig`, inside the `if SHIELD_ASURADA_ADAPTER` block, add:
```kconfig
config ASURADA_AUTO_PAGE_FOLLOW
    bool "Auto-switch the display page to the active input device"
    depends on ASURADA_STATUS_SCREEN_ASURADA
    default y
    help
      Trackball motion shows the trackball page; typing shows the keyboard
      page. Reliable input-driven navigation that does not depend on the touch
      panel (whose swipe support is flaky on some CST816S modules). Set to n if
      you prefer to stay on a page and navigate only by touch swipe.
```

- [ ] **Step 2: Implement `src/page_follow.c`**

Create it. First open a file that already subscribes to key events (e.g. the WPM widget under `src/layouts/asurada/`) to confirm the exact ZMK key event header/name to use; the plan below uses `zmk_keycode_state_changed` — if the WPM widget uses `zmk_position_state_changed` instead, match whichever fires on the dongle.
```c
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

#include "asurada_screens.h"

#define KEYBOARD_PAGE  0
#define TRACKBALL_PAGE 1

/* Trackball motion -> trackball page. Separate INPUT_EV_REL tap, independent of
 * src/trackball_input.c (Zephyr allows multiple input callbacks). */
static void pf_input_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);
    if (evt->type == INPUT_EV_REL) {
        asurada_screens_page_goto(TRACKBALL_PAGE);
    }
}
INPUT_CALLBACK_DEFINE(NULL, pf_input_cb, NULL);

/* Key press -> keyboard page. */
static int pf_key_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev != NULL && ev->state) {           /* on press */
        asurada_screens_page_goto(KEYBOARD_PAGE);
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(asurada_page_follow, pf_key_listener);
ZMK_SUBSCRIPTION(asurada_page_follow, zmk_keycode_state_changed);
```

- [ ] **Step 3: Compile it (only when the option is on)**

In `boards/shields/asurada_adapter/CMakeLists.txt`, next to the other `zephyr_library_sources(...)` lines, add:
```cmake
zephyr_library_sources_ifdef(CONFIG_ASURADA_AUTO_PAGE_FOLLOW src/page_follow.c)
```

- [ ] **Step 4: Verify — CI green.** Re-trigger `ci-display-phase1`; expect `totem_dongle` green. If `zmk_keycode_state_changed` / `as_zmk_keycode_state_changed` is the wrong symbol, switch to the one the WPM widget uses (checked in Step 2).

- [ ] **Step 5: Verify — hardware.** Flash the new `totem_dongle` .uf2. Expected: rolling the trackball switches to the ball page; pressing a key switches back to the keyboard page; brief cross-activity does not flicker (400 ms hysteresis). No touch swipe needed.

- [ ] **Step 6: Commit**
```bash
git add boards/shields/asurada_adapter/src/page_follow.c Kconfig \
        boards/shields/asurada_adapter/CMakeLists.txt
git commit -m "feat(display): auto page-follow (trackball->ball page, keys->keyboard page)"
```

---

## Phase 5 done

The carousel is navigable by input activity, independent of the flaky touch
swipe. Touch swipe remains wired (a no-op on modules where the CST816S doesn't
report drags) and can be revisited separately. Default-on so the target build
works out of the box.

## Self-Review

- Spec coverage: AUTO_PAGE_FOLLOW (spec §10) → Tasks 1-2; connections page is not
  a follow target (only 2 pages exist until Phase 3) — consistent.
- Placeholders: none; the one "match the WPM widget's key event name" note is a
  concrete verification against a named in-repo file, resolved before CI.
- Types: `asurada_screens_page_goto(int)` declared (Task 1) and called (Task 2);
  page indices match `status_screen.c` (kb=0, tb=1).
- Threading: input-thread + system-WQ callers both go through
  `asurada_screens_page_goto` → atomic target + display-WQ work; `active_page` /
  `last_switch` read & written only in `goto_work`/`go_to` on the display WQ.
