# Dongle Display — Phase 1: Trackball Input + Red Rolling Ball — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On the Asurada dongle, tap the forwarded trackball motion off the input subsystem and render a red 3D ball on the GC9A01 that rolls in the direction the trackball is moved.

**Architecture:** A `src/`-level Zephyr input callback (`trackball_input.c`) accumulates `INPUT_EV_REL` dx/dy on the driver thread and exposes a fetch-and-clear API. A new layout widget (`ball.c`) runs a ~30 Hz LVGL timer that fetches that motion, updates a virtual sphere orientation, and redraws: a static red base circle with a transparent LVGL canvas overlay carrying the rotating, depth-shaded surface dots. Phase 1 shows the ball as the asurada status screen's main content; the carousel that gives it a dedicated page is Phase 2.

**Tech Stack:** ZMK (`main`) / Zephyr 4.1, LVGL 9.x, nRF52840 (XIAO BLE), GC9A01 240×240, C.

## Global Constraints

- Panel: GC9A01 **240×240** round, RGB565, LVGL **9.x** APIs only (`lv_image_*`, `lv_canvas_set_px`, `lv_obj_*`).
- All LVGL calls run on the **display work queue** (`zmk_display_work_q()`); input callbacks run on the driver thread and must marshal, never touch LVGL directly. Mirror `src/touch.c` and `src/screensaver.c`.
- New files under `src/layouts/asurada/` are **auto-compiled** by the shield `CMakeLists.txt` glob (except `status_screen.c`, which is `#include`d). A `src/`-level source needs an explicit `zephyr_library_sources(...)`.
- No local build/test. **Verification build = CI:** push this branch, point a totem-cfg build's `config/west.yml` `asurada_dongle` project at `revision: dongle-display-ui`, push totem-cfg, and confirm the GitHub Actions `totem_dongle asurada_adapter` target is **green**. Hardware checks use RTT or `CONFIG_ZMK_USB_LOGGING` output and the eye.
- Keep every file focused; follow existing module patterns (`touch.c`, `radii/layer_indicator.c`, `field/line_segments.c`).

---

## File Structure

- `boards/shields/asurada_adapter/src/trackball_input.c` — input tap + accumulator (new, `src/`-level).
- `boards/shields/asurada_adapter/include/asurada_trackball.h` — fetch API (new).
- `boards/shields/asurada_adapter/src/layouts/asurada/ball.c` — the ball widget (new, auto-globbed).
- `boards/shields/asurada_adapter/src/layouts/asurada/ball.h` — widget struct + init/obj (new).
- `boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c` — add the ball (modify).
- `boards/shields/asurada_adapter/Kconfig` — `ASURADA_TRACKBALL_LOG` flag (modify).
- `boards/shields/asurada_adapter/CMakeLists.txt` — compile `trackball_input.c` (modify).
- `boards/shields/asurada_adapter/src/layouts/asurada/Kconfig.defconfig` — `select LV_USE_CANVAS` (modify).

---

## Task 1: Trackball input tap (motion accumulator)

**Files:**
- Create: `boards/shields/asurada_adapter/include/asurada_trackball.h`
- Create: `boards/shields/asurada_adapter/src/trackball_input.c`
- Modify: `boards/shields/asurada_adapter/Kconfig` (add `ASURADA_TRACKBALL_LOG`)
- Modify: `boards/shields/asurada_adapter/CMakeLists.txt` (compile the new source)

**Interfaces:**
- Produces: `bool asurada_trackball_fetch(int32_t *dx, int32_t *dy);` — writes accumulated motion since the previous call and returns `true` if there was any (else `*dx=*dy=0`, returns `false`). Safe to call from the display work queue.

- [ ] **Step 1: Write the fetch API header**

Create `include/asurada_trackball.h`:

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Fetch-and-clear trackball motion accumulated from forwarded INPUT_EV_REL
 * events since the last call. Returns true if there was motion. */
bool asurada_trackball_fetch(int32_t *dx, int32_t *dy);
```

- [ ] **Step 2: Write the input tap**

Create `src/trackball_input.c`:

```c
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

#include "asurada_trackball.h"

LOG_MODULE_REGISTER(asurada_tb, LOG_LEVEL_INF);

/* Accumulated on the input driver thread, drained on the display work queue. */
static atomic_t acc_x = ATOMIC_INIT(0);
static atomic_t acc_y = ATOMIC_INIT(0);
static atomic_t moved = ATOMIC_INIT(0);

static void tb_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);
    if (evt->type != INPUT_EV_REL) {
        return;
    }
    if (evt->code == INPUT_REL_X) {
        atomic_add(&acc_x, evt->value);
    } else if (evt->code == INPUT_REL_Y) {
        atomic_add(&acc_y, evt->value);
    } else {
        return;
    }
    atomic_set(&moved, 1);
#if IS_ENABLED(CONFIG_ASURADA_TRACKBALL_LOG)
    LOG_INF("trackball REL code=%u val=%d", evt->code, evt->value);
#endif
}

/* dev = NULL taps every input device; we filter on INPUT_EV_REL above so the
 * CST816S touch (INPUT_EV_ABS) is ignored. */
INPUT_CALLBACK_DEFINE(NULL, tb_cb, NULL);

bool asurada_trackball_fetch(int32_t *dx, int32_t *dy) {
    if (!atomic_get(&moved)) {
        *dx = 0;
        *dy = 0;
        return false;
    }
    *dx = (int32_t)atomic_set(&acc_x, 0);
    *dy = (int32_t)atomic_set(&acc_y, 0);
    atomic_set(&moved, 0);
    return true;
}
```

- [ ] **Step 3: Add the debug-log Kconfig flag**

In `boards/shields/asurada_adapter/Kconfig`, append (module-guarded like the neighbours):

```kconfig
config ASURADA_TRACKBALL_LOG
    bool "Log forwarded trackball REL events (Phase-1 bring-up)"
    default n
    help
      Emits an INFO log line per forwarded trackball INPUT_EV_REL event.
      Use to confirm motion reaches the dongle display, then turn off.
```

- [ ] **Step 4: Compile the source**

In `boards/shields/asurada_adapter/CMakeLists.txt`, alongside the existing
`zephyr_library_sources(...)` lines (next to `src/touch.c`), add:

```cmake
zephyr_library_sources(src/trackball_input.c)
```

- [ ] **Step 5: Verify — CI build green**

Point a totem-cfg build at this branch and confirm the `totem_dongle asurada_adapter` target compiles. Expected: **green**, no undefined-symbol / missing-header errors. `CONFIG_INPUT` is already pulled in on the dongle by `CONFIG_ZMK_POINTING`.

- [ ] **Step 6: Verify — motion reaches the dongle (hardware)**

Temporarily set `CONFIG_ASURADA_TRACKBALL_LOG=y` (dongle `.conf`) and
`CONFIG_ZMK_USB_LOGGING=y`, flash the dongle, pair the trackball, roll it.
Expected: `trackball REL code=… val=…` lines stream while rolling, stop when
still. If nothing logs, the split isn't forwarding REL to a local input device —
resolve before Task 2 (this is the single external dependency). Turn both flags
off afterward.

- [ ] **Step 7: Commit**

```bash
git add boards/shields/asurada_adapter/include/asurada_trackball.h \
        boards/shields/asurada_adapter/src/trackball_input.c \
        boards/shields/asurada_adapter/Kconfig \
        boards/shields/asurada_adapter/CMakeLists.txt
git commit -m "feat(display): tap forwarded trackball motion on the dongle"
```

---

## Task 2: Red rolling-ball widget

**Files:**
- Create: `boards/shields/asurada_adapter/src/layouts/asurada/ball.h`
- Create: `boards/shields/asurada_adapter/src/layouts/asurada/ball.c`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c`
- Modify: `boards/shields/asurada_adapter/src/layouts/asurada/Kconfig.defconfig` (`select LV_USE_CANVAS`)

**Interfaces:**
- Consumes: `asurada_trackball_fetch()` from Task 1.
- Produces: `void zmk_widget_asurada_ball_init(struct zmk_widget_asurada_ball *w, lv_obj_t *parent);` and `lv_obj_t *zmk_widget_asurada_ball_obj(struct zmk_widget_asurada_ball *w);`.

Design: a `BALL_SZ`×`BALL_SZ` container centred on the screen. A static red
circle `lv_obj` provides the shaded base (solid red + a small light specular
child). A transparent LVGL **canvas** of the same size sits on top and, every
33 ms, is cleared and repainted with the front-facing surface dots of a rotating
unit sphere (depth-shaded), giving the rolling illusion. This keeps per-frame
work to one `lv_canvas_fill_bg` plus ~`POINT_COUNT` `lv_canvas_set_px` calls —
within the confirmed ~30 fps budget (see `field/line_segments.c`). Point density
(`LAT_STEP`/`LON_STEP`) is the tuning knob if frames run long.

- [ ] **Step 1: Enable the canvas widget in LVGL**

In `src/layouts/asurada/Kconfig.defconfig`, add to the existing `select` list:

```kconfig
    select LV_USE_CANVAS
```

- [ ] **Step 2: Write the widget header**

Create `src/layouts/asurada/ball.h`:

```c
#pragma once
#include <lvgl.h>

#define BALL_SZ 130            /* widget/canvas square, px */

struct zmk_widget_asurada_ball {
    lv_obj_t *cont;            /* container */
    lv_obj_t *base;            /* static shaded red disc */
    lv_obj_t *canvas;          /* transparent overlay: rotating dots */
    lv_timer_t *timer;
    float rot[9];              /* 3x3 orientation matrix, row-major */
    float vx, vy;              /* angular momentum (rad/frame) */
};

void zmk_widget_asurada_ball_init(struct zmk_widget_asurada_ball *w, lv_obj_t *parent);
lv_obj_t *zmk_widget_asurada_ball_obj(struct zmk_widget_asurada_ball *w);
```

- [ ] **Step 3: Write the widget implementation**

Create `src/layouts/asurada/ball.c`:

```c
#include <zephyr/kernel.h>
#include <math.h>
#include <lvgl.h>

#include "ball.h"
#include "asurada_trackball.h"

#define BALL_R   (BALL_SZ / 2 - 4)     /* sphere radius, px */
#define BALL_C   (BALL_SZ / 2)         /* centre, px */
#define LAT_STEP 15                    /* degrees between parallels */
#define LON_STEP 12                    /* degrees between meridian samples */
#define K_ROT    0.010f                /* px of drag -> radians */
#define DECAY    0.92f

/* Precomputed unit-sphere surface points (lat/long grid). */
#define MAX_PTS 512
static float pts[MAX_PTS][3];
static int   n_pts;

/* Canvas backing buffer (ARGB8888), static like radii/layer_indicator.c. */
static uint8_t canvas_buf[LV_CANVAS_BUF_SIZE(BALL_SZ, BALL_SZ, 32, 1)];

static void build_points(void) {
    n_pts = 0;
    for (int lat = -75; lat <= 75 && n_pts < MAX_PTS; lat += LAT_STEP) {
        float rl = lat * (float)M_PI / 180.0f, cz = sinf(rl), cr = cosf(rl);
        for (int lon = 0; lon < 360 && n_pts < MAX_PTS; lon += LON_STEP) {
            float a = lon * (float)M_PI / 180.0f;
            pts[n_pts][0] = cr * cosf(a);
            pts[n_pts][1] = cz;
            pts[n_pts][2] = cr * sinf(a);
            n_pts++;
        }
    }
}

static void mat_mul(const float a[9], const float b[9], float out[9]) {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            out[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] +
                             a[i * 3 + 2] * b[6 + j];
}

/* rot = rotY(day) * rotX(dax) * rot  (screen-space roll). */
static void apply_rot(float rot[9], float dax, float day) {
    float cx = cosf(dax), sx = sinf(dax), cy = cosf(day), sy = sinf(day);
    float rx[9] = {1, 0, 0,   0, cx, -sx,  0, sx, cx};
    float ry[9] = {cy, 0, sy, 0, 1, 0,   -sy, 0, cy};
    float t[9], u[9];
    mat_mul(rx, rot, t);
    mat_mul(ry, t, u);
    for (int i = 0; i < 9; i++) rot[i] = u[i];
}

static void redraw(struct zmk_widget_asurada_ball *w) {
    lv_canvas_fill_bg(w->canvas, lv_color_black(), LV_OPA_TRANSP);
    const float *r = w->rot;
    for (int i = 0; i < n_pts; i++) {
        float x = pts[i][0], y = pts[i][1], z = pts[i][2];
        float Z = r[6] * x + r[7] * y + r[8] * z;
        if (Z <= 0.05f) continue;                    /* back-face cull */
        float X = r[0] * x + r[1] * y + r[2] * z;
        float Y = r[3] * x + r[4] * y + r[5] * z;
        int px = BALL_C + (int)(X * BALL_R);
        int py = BALL_C - (int)(Y * BALL_R);
        lv_opa_t opa = (lv_opa_t)(60 + Z * 195.0f); /* depth shade */
        lv_color_t col = lv_color_make(255, 210, 200);
        lv_canvas_set_px(w->canvas, px, py, col, opa);
    }
}

static void tick(lv_timer_t *t) {
    struct zmk_widget_asurada_ball *w = lv_timer_get_user_data(t);
    int32_t dx, dy;
    if (asurada_trackball_fetch(&dx, &dy)) {
        w->vx = dy * K_ROT;                          /* drag down -> pitch */
        w->vy = dx * K_ROT;                          /* drag right -> yaw  */
    } else {
        w->vx *= DECAY;
        w->vy *= DECAY;
        if (fabsf(w->vx) < 0.0008f && fabsf(w->vy) < 0.0008f) {
            return;                                  /* idle: skip redraw */
        }
    }
    apply_rot(w->rot, w->vx, w->vy);
    redraw(w);
}

lv_obj_t *zmk_widget_asurada_ball_obj(struct zmk_widget_asurada_ball *w) {
    return w->cont;
}

void zmk_widget_asurada_ball_init(struct zmk_widget_asurada_ball *w, lv_obj_t *parent) {
    build_points();
    for (int i = 0; i < 9; i++) w->rot[i] = (i % 4 == 0) ? 1.0f : 0.0f;
    w->vx = 0.02f;                                   /* tiny initial spin */
    w->vy = 0.01f;

    w->cont = lv_obj_create(parent);
    lv_obj_remove_style_all(w->cont);
    lv_obj_set_size(w->cont, BALL_SZ, BALL_SZ);
    lv_obj_center(w->cont);

    /* Static shaded red base disc. */
    w->base = lv_obj_create(w->cont);
    lv_obj_remove_style_all(w->base);
    lv_obj_set_size(w->base, BALL_R * 2, BALL_R * 2);
    lv_obj_center(w->base);
    lv_obj_set_style_radius(w->base, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(w->base, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(w->base, lv_color_hex(0xE5322A), 0);
    lv_obj_set_style_bg_grad_color(w->base, lv_color_hex(0x7D120C), 0);
    lv_obj_set_style_bg_grad_dir(w->base, LV_GRAD_DIR_VER, 0);

    /* Small specular highlight top-left. */
    lv_obj_t *hi = lv_obj_create(w->base);
    lv_obj_remove_style_all(hi);
    lv_obj_set_size(hi, BALL_R, BALL_R);
    lv_obj_set_pos(hi, BALL_R / 5, BALL_R / 6);
    lv_obj_set_style_radius(hi, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hi, lv_color_hex(0xFF8F82), 0);
    lv_obj_set_style_bg_opa(hi, LV_OPA_40, 0);

    /* Transparent canvas overlay for the rotating surface dots. */
    w->canvas = lv_canvas_create(w->cont);
    lv_canvas_set_buffer(w->canvas, canvas_buf, BALL_SZ, BALL_SZ, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_center(w->canvas);
    lv_canvas_fill_bg(w->canvas, lv_color_black(), LV_OPA_TRANSP);

    redraw(w);
    w->timer = lv_timer_create(tick, 33, w);
}
```

- [ ] **Step 4: Show the ball on the status screen**

In `src/layouts/asurada/status_screen.c`: add the include, a static widget
instance, and its init call (place it after the existing widget inits so it
layers on top — Phase 2 moves it to its own page):

```c
#include "ball.h"
/* ... */
static struct zmk_widget_asurada_ball ball_widget;
/* inside zmk_display_status_screen(), before `return screen;`: */
    zmk_widget_asurada_ball_init(&ball_widget, screen);
```

- [ ] **Step 5: Verify — CI build green**

Rebuild via the totem-cfg CI pointed at this branch. Expected: **green**. If
`lv_color_format` / `LV_GRAD_DIR_VER` / `lv_canvas_set_px` signatures mismatch
the pinned LVGL 9, adjust to the header spellings used elsewhere in the module
(`radii/layer_indicator.c` for canvas, `battery_circles.c` for gradients) and
rebuild.

- [ ] **Step 6: Verify — the ball rolls (hardware)**

Flash the dongle, pair the trackball. Expected: a red sphere centred on the
screen; rolling the trackball makes the light surface dots travel in the drag
direction (a rolling ball), with a short coast-down when you stop. If X/Y feel
swapped or inverted, flip the `dx`/`dy` assignment or signs in `tick()`. If
frames stutter, raise `LAT_STEP`/`LON_STEP` (fewer points).

- [ ] **Step 7: Commit**

```bash
git add boards/shields/asurada_adapter/src/layouts/asurada/ball.h \
        boards/shields/asurada_adapter/src/layouts/asurada/ball.c \
        boards/shields/asurada_adapter/src/layouts/asurada/status_screen.c \
        boards/shields/asurada_adapter/src/layouts/asurada/Kconfig.defconfig
git commit -m "feat(display): red 3D rolling trackball ball widget"
```

---

## Phase 1 done

Flashing the dongle now shows a red ball that rolls with the trackball — the
input path and the procedural sphere are proven on hardware. **Phases 2–5 get
their own plans** (carousel + keyboard page + swipe rebind; connections + context
battery + pointing mode; standby-eyes rewrite; `AUTO_PAGE_FOLLOW`), authored once
this phase is validated so their code builds on a confirmed foundation.

## Self-Review

- **Spec coverage (Phase 1 slice):** trackball input tap (§8) → Task 1; red
  procedural rolling ball (§5) → Task 2. Pointing-mode text, battery, carousel,
  tach, eyes, auto-follow are later phases by design (§14).
- **Placeholders:** none — every code step is complete. The two "adjust to the
  pinned LVGL 9 spellings" notes are real CI-iteration guidance against named
  precedent files, not hand-waves.
- **Type consistency:** `asurada_trackball_fetch(int32_t*,int32_t*)` defined in
  Task 1 header and consumed with matching types in Task 2; widget
  init/obj/struct names consistent between `ball.h` and `ball.c` and the
  `status_screen.c` call.
- **Known external dependency:** Task 1 Step 6 explicitly gates on REL actually
  reaching the dongle before the ball work begins.
