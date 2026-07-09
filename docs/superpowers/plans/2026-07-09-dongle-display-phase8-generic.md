# Dongle Display — Phase 8: Make the module generic/reusable — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Turn the Asurada display shield into a **drop-in module for any ZMK split keyboard, with an optional trackball**. Two axes of genericity: (1) a `CONFIG_ASURADA_TRACKBALL` toggle that adds/removes the whole trackball page + input + widgets, and (2) a Connections page that adapts to the actual peripheral count with configurable labels. The keyboard side is already generic (WPM / layer name / modifiers / battery all read live ZMK state — `layer_center.c` shows each keymap's own `display-name`), so no keymap is hard-wired; this phase makes the trackball optional and the peripheral list configurable, plus a reuse README.

**Architecture:** Compile-time gating. When `CONFIG_ASURADA_TRACKBALL=n`, the trackball layout sources (`ball.c`, `pointing_mode.c`, `trackball_battery.c`) and `trackball_input.c` are excluded in CMake, `status_screen.c` builds a 2-page carousel (keyboard, connections), and the auto-follow REL→trackball branch is guarded out. The Connections widget renders `min(PERIPHERAL_COUNT, ASURADA_CONN_ROWS)` rows labeled from `CONFIG_ASURADA_CONN_LABEL_n` (defaults Left/Right/Trackball/…), so it already reflects a 1-/2-/3-/4-peripheral build without further wiring.

**Tech Stack:** ZMK (`main`) / Zephyr 4.1, LVGL 9.x, C, Kconfig, CMake.

## Global Constraints

- **Default `CONFIG_ASURADA_TRACKBALL=y`** — the current 3-page behavior must be byte-for-byte unchanged for the existing build (verified by CI staying green with defaults). A generic user sets `=n` for a keyboard-only dongle.
- Trackball widgets (`ball.c`/`pointing_mode.c`/`trackball_battery.c`) are auto-globbed from `src/layouts/asurada/`; exclude them via `list(FILTER ... EXCLUDE ...)` when the toggle is off. `status_screen.c` must guard EVERY reference to those widgets (includes, statics, init calls, the `on_page_active` ball line) with `#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)` so it compiles when they're absent.
- `asurada_trackball_fetch()` (in `trackball_input.c`) is called by `ball.c` only; when the toggle is off BOTH are excluded, so no dangling reference — do not leave one without the other.
- Connections rows are driven by `PERIPHERAL_COUNT` (= `ZMK_SPLIT_BLE_PERIPHERAL_COUNT`, the same compile-time macro `battery_circles.c`/`connections.c` already use for array sizing), NOT by the trackball toggle — a trackball is just one more peripheral.
- No local build; verify via CI (`ci-display-phase1` → all 5 targets green with defaults). A `=n` build is not in the CI matrix, so reason carefully about the guards; the review will check the `=n` path compiles by inspection.
- No behavior change to the keyboard page, eyes, or tach.

---

## Task 1: `CONFIG_ASURADA_TRACKBALL` toggle + gate the trackball page/input

**Files:**
- Modify: `Kconfig` (repo root, inside `if SHIELD_ASURADA_ADAPTER`)
- Modify: `boards/shields/asurada_adapter/CMakeLists.txt`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c`
- Modify: `boards/shields/asurada_adapter/src/page_follow.c`

- [ ] **Step 1: Kconfig option.** Add inside `if SHIELD_ASURADA_ADAPTER` (near `ASURADA_TRACKBALL_SLOT`):
```kconfig
config ASURADA_TRACKBALL
    bool "Trackball features (rolling-ball page, pointing-mode text, trackball battery)"
    default y
    help
      Enable the trackball carousel page (the rolling red ball, the SCROLL/SNIPE
      pointing-mode text, and the trackball's battery) plus tapping the forwarded
      pointer input. Turn OFF for a keyboard-only dongle: the carousel becomes two
      pages (keyboard + connections) and no trackball code is built.
```
Also make the existing `ASURADA_TRACKBALL_SLOT` depend on it (read the file; add `depends on ASURADA_TRACKBALL` to that option) since a slot index is meaningless without the trackball page.

- [ ] **Step 2: CMake gating.** READ `CMakeLists.txt` first. (a) Wrap the trackball input source:
```cmake
  if(CONFIG_ASURADA_TRACKBALL)
    zephyr_library_sources(src/trackball_input.c)
  endif()
```
(replacing the current unconditional `zephyr_library_sources(src/trackball_input.c)`).
(b) In the `CONFIG_ASURADA_STATUS_SCREEN_ASURADA` glob branch, exclude the trackball widgets when the toggle is off — add right after the existing `status_screen.c` filter line:
```cmake
    if(NOT CONFIG_ASURADA_TRACKBALL)
      list(FILTER layout_sources EXCLUDE REGEX "(ball|pointing_mode|trackball_battery)\\.c$")
    endif()
```

- [ ] **Step 3: status_screen.c guards.** READ the current file. Wrap all trackball pieces in `#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)`:
  - the `#include "ball.h"`, `#include "trackball_battery.h"`, `#include "pointing_mode.h"` lines;
  - the `ball_widget` / `tb_battery_widget` / `pointing_mode_widget` statics;
  - the whole `on_page_active` function (it references `ball_widget`);
  - carousel size + page handles: use 3 pages + `tb = asurada_screens_page(1)` / `conn = page(2)` when enabled, else 2 pages + `conn = page(1)`;
  - the ball/pointing/tb_battery init+align calls;
  - the `asurada_screens_set_activate_cb(on_page_active)` call.
  The keyboard-page widgets and the `connections_widget` init stay unconditional. Concretely the body becomes:
```c
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
    asurada_screens_init(screen, 3);
    lv_obj_t *kb = asurada_screens_page(0);
    lv_obj_t *tb = asurada_screens_page(1);
    lv_obj_t *conn = asurada_screens_page(2);
#else
    asurada_screens_init(screen, 2);
    lv_obj_t *kb = asurada_screens_page(0);
    lv_obj_t *conn = asurada_screens_page(1);
#endif

    /* Page 0: keyboard status (always). */
    zmk_widget_wpm_border_init(&wpm_border_widget, kb);
    lv_obj_center(zmk_widget_wpm_border_obj(&wpm_border_widget));
    zmk_widget_battery_circles_init(&battery_circles_widget, kb);
    lv_obj_align(zmk_widget_battery_circles_obj(&battery_circles_widget), LV_ALIGN_BOTTOM_MID, 0, -26);
    zmk_widget_layer_center_init(&layer_center_widget, kb);
    lv_obj_align(zmk_widget_layer_center_obj(&layer_center_widget), LV_ALIGN_CENTER, 0, -34);
    zmk_widget_asurada_modifiers_init(&modifiers_widget, kb);
    lv_obj_align(zmk_widget_asurada_modifiers_obj(&modifiers_widget), LV_ALIGN_CENTER, 0, 8);

#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
    /* Page 1: trackball (rolling ball + pointing mode + battery). */
    zmk_widget_asurada_ball_init(&ball_widget, tb);
    zmk_widget_asurada_pointing_mode_init(&pointing_mode_widget, tb);
    zmk_widget_asurada_tb_battery_init(&tb_battery_widget, tb);
    lv_obj_align(zmk_widget_asurada_tb_battery_obj(&tb_battery_widget), LV_ALIGN_TOP_MID, 0, 10);
#endif

    /* Last page: connections. */
    zmk_widget_asurada_connections_init(&connections_widget, conn);

#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)
    asurada_screens_set_activate_cb(on_page_active);
#endif
    return screen;
```
Keep the exact keyboard-page offsets/values already in the file (don't retune them here).

- [ ] **Step 4: page_follow.c guard.** READ the file. It routes forwarded pointer motion (`INPUT_EV_REL`) to the trackball page (index 1). Guard the REL→trackball branch (and any `TRACKBALL_PAGE`/REL handling) with `#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL)`, leaving the keycode→keyboard-page branch intact, so a keyboard-only build doesn't reference a trackball page that no longer exists. If the whole file is trackball-motion-only, guard its body so it compiles to nothing when the toggle is off (the `CONFIG_ASURADA_AUTO_PAGE_FOLLOW` CMake gate stays as-is).

- [ ] **Step 5: Verify — CI green** (default `y` → unchanged 3-page build). Re-trigger `ci-display-phase1`. Commit.
```bash
git add Kconfig boards/shields/asurada_adapter/CMakeLists.txt \
        boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c \
        boards/shields/asurada_adapter/src/page_follow.c
git commit -m "feat(display): CONFIG_ASURADA_TRACKBALL toggle (optional trackball page/input)"
```

---

## Task 2: Connections page adapts to peripheral count + configurable labels

**Files:**
- Modify: `Kconfig` (repo root, inside `if SHIELD_ASURADA_ADAPTER`)
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/connections.h`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/connections.c`

- [ ] **Step 1: Kconfig labels.** Add inside `if SHIELD_ASURADA_ADAPTER`:
```kconfig
config ASURADA_CONN_LABEL_0
    string "Connections page label for split peripheral slot 0"
    default "Left"
config ASURADA_CONN_LABEL_1
    string "Connections page label for split peripheral slot 1"
    default "Right"
config ASURADA_CONN_LABEL_2
    string "Connections page label for split peripheral slot 2"
    default "Trackball"
config ASURADA_CONN_LABEL_3
    string "Connections page label for split peripheral slot 3"
    default "Periph 4"
```

- [ ] **Step 2: Header.** In `connections.h`, bump the max rows to cover the labels (this is now the array capacity, not the render count):
```c
#define ASURADA_CONN_ROWS 4   /* max rows / label capacity; rendered = min(PERIPHERAL_COUNT, this) */
```
(the `dot[ASURADA_CONN_ROWS]` / `pct[ASURADA_CONN_ROWS]` arrays grow to 4 automatically.)

- [ ] **Step 3: connections.c.** READ it first. Change the label table + row count:
  - labels from Kconfig:
```c
static const char *const row_names[ASURADA_CONN_ROWS] = {
    CONFIG_ASURADA_CONN_LABEL_0, CONFIG_ASURADA_CONN_LABEL_1,
    CONFIG_ASURADA_CONN_LABEL_2, CONFIG_ASURADA_CONN_LABEL_3,
};
```
  - number of rendered rows = `MIN(PERIPHERAL_COUNT, ASURADA_CONN_ROWS)` (`MIN` from `<zephyr/sys/util.h>`, already reachable via `<zephyr/kernel.h>`). Add near the other macros:
```c
#define CONN_N_ROWS  MIN(PERIPHERAL_COUNT, ASURADA_CONN_ROWS)
```
  - the row-build loop in `zmk_widget_asurada_connections_init`: change `for (int i = 0; i < ASURADA_CONN_ROWS; i++)` → `for (int i = 0; i < CONN_N_ROWS; i++)`. (Rows that aren't built leave `dot[i]`/`pct[i]` NULL; the render fan-out already null-checks them.)
  - the two bound guards in `set_battery_level`/`set_connection_status`: change `source >= ASURADA_CONN_ROWS` → `source >= CONN_N_ROWS` (so events for slots beyond the rendered rows are ignored; the fixed arrays are still sized `ASURADA_CONN_ROWS` so this is also memory-safe).
  Everything else (the dual ZMK listeners, the dot/pct rendering) is unchanged.

- [ ] **Step 4: Verify — CI green** (default PERIPHERAL_COUNT unchanged → 3 rows Left/Right/Trackball, same as before). Re-trigger `ci-display-phase1`. Commit.
```bash
git add Kconfig boards/shields/asurada_adapter/src/layouts/asurada/connections.h \
        boards/shields/asurada_adapter/src/layouts/asurada/connections.c
git commit -m "feat(display): Connections page adapts to peripheral count + configurable labels"
```

---

## Task 3: Reuse README

**Files:**
- Modify: `asurada_dongle/README.md`

- [ ] **Step 1: Add a section** titled "Reusing this display on your own split keyboard" that documents, concisely:
  - The keymap is YOURS — the display reads live ZMK state, so WPM, your layer names (via each layer's `display-name`), held modifiers, and per-half battery all appear automatically with no module changes.
  - `CONFIG_ASURADA_TRACKBALL` (default y): set `=n` for a keyboard-only dongle (2-page carousel, no trackball code).
  - If you DO have a trackball: set `CONFIG_ASURADA_TRACKBALL_SLOT` to its split slot, and the pointing-mode text follows `CONFIG_ASURADA_SCROLL_LAYER` / `SCROLL_LAYER2` / `SNIPE_LAYER` (default 6/7/8) — point those at your keymap's scroll/snipe layer indices.
  - Connections page: labels come from `CONFIG_ASURADA_CONN_LABEL_0..3` (default Left/Right/Trackball/Periph 4); it shows one row per split peripheral (`ZMK_SPLIT_BLE_PERIPHERAL_COUNT`).
  - Optional: `CONFIG_ASURADA_LAYER_NAME_UPPERCASE`, `CONFIG_ASURADA_AUTO_PAGE_FOLLOW`, the screensaver options.
  Match the README's existing heading style/tone (read it first).

- [ ] **Step 2: Commit** (docs only, no CI needed).
```bash
git add asurada_dongle/README.md
git commit -m "docs: how to reuse the Asurada display on any split keyboard"
```

## Phase 8 done

The shield is now a generic ZMK split-keyboard display: keyboard-only or with a
trackball (`CONFIG_ASURADA_TRACKBALL`), Connections adapts to the peripheral count
with configurable labels, and the README explains reuse. Defaults reproduce the
current 3-page build exactly.

## Self-Review

- Spec coverage: "choose trackball or not" → Task 1 toggle; "works with any keymap"
  → already true (documented in Task 3) + configurable pointing layers/labels; generic
  peripheral list → Task 2.
- Placeholders: none. CMake `list(FILTER)`, `#if IS_ENABLED`, and Kconfig `string`
  are all existing patterns in this repo (glob filter for status_screen.c; IS_ENABLED
  in touch.c; string configs are standard Kconfig).
- Types/consistency: `CONFIG_ASURADA_TRACKBALL` gates CMake + status_screen + page_follow
  coherently (widget sources and their sole caller excluded together); `CONN_N_ROWS`
  ≤ `ASURADA_CONN_ROWS` keeps array indexing in-bounds.
- Regression guard: every default is chosen so the existing `totem_dongle` build is
  unchanged (trackball y, 3 conn rows Left/Right/Trackball).
- The `=n` path has no CI coverage, so the reviewer must trace it by inspection:
  no trackball symbol referenced when the sources are excluded.
