# Dongle Display — Phase 3a: Pointing-mode indicator (SCROLL / SNIPE) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Show `SCROLL` or `SNIPE` on the trackball page while the corresponding pointing layer is active (the trackball's scroll/snipe buttons engage them), so the page reflects the live pointing mode.

**Architecture:** A tiny label widget on the trackball page that subscribes to ZMK layer-state changes and shows text based on which of the keymap's pointing layers is active: SCRLM(6)/SCRLH(7) → "SCROLL", SNIPE(8) → "SNIPE", else hidden. Same widget/listener pattern as the existing `layer_center.c`.

**Tech Stack:** ZMK (`main`) / Zephyr 4.1, LVGL 9.x, C.

## Global Constraints

- LVGL 9.x only; the widget is built on the trackball page (parent passed to its `_init`) on the display work queue.
- The pointing-layer indices are the totem keymap's `SCRLM=6 / SCRLH=7 / SNIPE=8` (see `config/totem.keymap` in the keyboard config). They're build-specific → expose as Kconfig with those defaults so the module isn't hard-wired to one keymap.
- No local build; verify via CI (`ci-display-phase1` → `totem_dongle` green) + hardware (hold a trackball scroll/snipe button → text appears on the trackball page).
- Follow `src/layouts/asurada/layer_center.c` for the exact layer-state event/header/`ZMK_DISPLAY_WIDGET_LISTENER`/`ZMK_SUBSCRIPTION` spelling and the layer-active query.

---

## Task 1: Kconfig for the pointing-layer indices

**Files:**
- Modify: `Kconfig` (repo root, inside `if SHIELD_ASURADA_ADAPTER`)

- [ ] **Step 1: Add the options**
```kconfig
config ASURADA_SCROLL_LAYER
    int "Keymap layer index that means SCROLL (shown on the trackball page)"
    default 6

config ASURADA_SCROLL_LAYER2
    int "Second SCROLL layer index (scroll-keep); set = SCROLL layer to disable"
    default 7

config ASURADA_SNIPE_LAYER
    int "Keymap layer index that means SNIPE"
    default 8
```

- [ ] **Step 2: Verify — CI green** (nothing uses them yet). Commit.
```bash
git add Kconfig
git commit -m "feat(display): Kconfig for pointing-mode layer indices"
```

---

## Task 2: Pointing-mode widget + hook onto the trackball page

**Files:**
- Create: `src/layouts/asurada/pointing_mode.h`
- Create: `src/layouts/asurada/pointing_mode.c`
- Modify: `src/layouts/asurada/status_screen.c`

**Interfaces:**
- Produces: `void zmk_widget_asurada_pointing_mode_init(struct zmk_widget_asurada_pointing_mode *w, lv_obj_t *parent);`

- [ ] **Step 1: Read `layer_center.c`** (and its `.h`) to copy the exact layer-state listener pattern: the include (`<zmk/events/layer_state_changed.h>`), `ZMK_DISPLAY_WIDGET_LISTENER`, `ZMK_SUBSCRIPTION(..., zmk_layer_state_changed)`, and how it reads the active layer (`zmk_keymap_layer_active(index)` from `<zmk/keymap.h>`). Use the same spellings below.

- [ ] **Step 2: Header**

Create `src/layouts/asurada/pointing_mode.h`:
```c
#pragma once
#include <lvgl.h>
#include <zephyr/sys/util.h>

struct zmk_widget_asurada_pointing_mode {
    sys_snode_t node;
    lv_obj_t *obj;
};

void zmk_widget_asurada_pointing_mode_init(struct zmk_widget_asurada_pointing_mode *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_pointing_mode_obj(struct zmk_widget_asurada_pointing_mode *w);
```

- [ ] **Step 3: Implementation**

Create `src/layouts/asurada/pointing_mode.c` (mirror `layer_center.c`'s listener wiring; only the state selection differs):
```c
#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/keymap.h>
#include <zmk/events/layer_state_changed.h>

#include "pointing_mode.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static const char *mode_text(void) {
    if (zmk_keymap_layer_active(CONFIG_ASURADA_SNIPE_LAYER)) {
        return "SNIPE";
    }
    if (zmk_keymap_layer_active(CONFIG_ASURADA_SCROLL_LAYER) ||
        zmk_keymap_layer_active(CONFIG_ASURADA_SCROLL_LAYER2)) {
        return "SCROLL";
    }
    return "";
}

static void set_text(struct zmk_widget_asurada_pointing_mode *w, const char *t) {
    lv_label_set_text(w->obj, t);
}

static void update_cb(const char *t) {
    struct zmk_widget_asurada_pointing_mode *w;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, w, node) {
        set_text(w, t);
    }
}

static const char *get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    return mode_text();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_pointing_mode, const char *, update_cb, get_state)
ZMK_SUBSCRIPTION(widget_asurada_pointing_mode, zmk_layer_state_changed);

lv_obj_t *zmk_widget_asurada_pointing_mode_obj(struct zmk_widget_asurada_pointing_mode *w) {
    return w->obj;
}

void zmk_widget_asurada_pointing_mode_init(struct zmk_widget_asurada_pointing_mode *w, lv_obj_t *parent) {
    w->obj = lv_label_create(parent);
    lv_obj_set_style_text_color(w->obj, lv_color_hex(0x35E0FF), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(w->obj, 4, LV_PART_MAIN);
    lv_label_set_text(w->obj, "");
    lv_obj_align(w->obj, LV_ALIGN_CENTER, 0, 64);   /* below the ball */

    sys_slist_append(&widgets, &w->node);
    widget_asurada_pointing_mode_init();            /* start the listener */
}
```
NOTE: `ZMK_DISPLAY_WIDGET_LISTENER`'s exact macro shape (callback arg type, the generated `_init()` name) must match how `layer_center.c` uses it — if `layer_center.c` differs (e.g. passes the widget, not a plain value), mirror THAT signature exactly instead of the sketch above.

- [ ] **Step 4: Add it to the trackball page**

In `status_screen.c`, add the include, a static instance, and init it on the trackball page `tb` (page 1), after the ball:
```c
#include "pointing_mode.h"
```
```c
static struct zmk_widget_asurada_pointing_mode pointing_mode_widget;
```
```c
    zmk_widget_asurada_pointing_mode_init(&pointing_mode_widget, tb);
```

- [ ] **Step 5: Verify — CI green.** Re-trigger `ci-display-phase1`; expect `totem_dongle` green. If `ZMK_DISPLAY_WIDGET_LISTENER` / `zmk_keymap_layer_active` spellings differ, match `layer_center.c`.

- [ ] **Step 6: Verify — hardware.** On the trackball page, hold the trackball's SCROLL button (D4, engages layer 6) → "SCROLL" shows; hold the SNIPE toggle (D6 → layer 8) → "SNIPE" shows; release → text clears.

- [ ] **Step 7: Commit**
```bash
git add boards/shields/asurada_adapter/src/layouts/asurada/pointing_mode.h \
        boards/shields/asurada_adapter/src/layouts/asurada/pointing_mode.c \
        boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c
git commit -m "feat(display): SCROLL/SNIPE pointing-mode indicator on the trackball page"
```

## Phase 3a done

The trackball page shows the live pointing mode. Phase 3b: the Connections page
(3rd carousel page) + tap-to-cycle navigation (so it's reachable while touch swipe
is unavailable) + context-dependent battery.

## Self-Review

- Spec coverage: pointing-mode text (spec §5) → Task 2; layer indices configurable
  (§ Global Constraints).
- Placeholders: none; the two "match layer_center.c's exact macro" notes name the
  precedent file for CI-confirmed symbols (the ZMK display listener macro varies by
  ZMK version, so mirroring the in-repo user is the safe move).
- Types: `zmk_widget_asurada_pointing_mode` struct + init/obj consistent across
  header, source, and the status_screen call.
