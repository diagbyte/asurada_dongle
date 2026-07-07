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
