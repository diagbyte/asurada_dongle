#include "trackball_buttons.h"

#include <zmk/display.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/event_manager.h>

#include <fonts.h>
#include "display_colors.h"

/*
 * Trackball buttons drawn as pie/arc "buttons" ringing the ball, mirroring the
 * AdeptBLE cluster: 4 across the top, 2 at the lower corners. Each button is an
 * arc SEGMENT of a band around the ball with its label inside; pressing the
 * physical button (forwarded as split key positions 38..43) lights that segment
 * cyan and darkens its text -- a pressed-button effect.
 *   38 MB4/Back  39 MB5/Fwd  40 MCLK/Wheel  41 RCLK/Right  42 LCLK/Left  43 SNIPE/Sniper
 * (RCLK/SNIPE also scroll on long-hold; the label shows the tap action.)
 */

#define TB_BTN_FIRST_POS 38

#define SEG_IDLE   0x3A4E58   /* visible slate band = un-pressed button */
#define SEG_ACTIVE 0x35E0FF   /* cyan band = pressed (lit) */
#define TXT_IDLE   0xD5E4EA   /* light text on the slate band */
#define TXT_ACTIVE 0x05222A   /* dark text on the cyan band */

#define RING_W  40            /* arc band thickness, px (thick = button-like) */
#define ARC_SZ  212           /* arc box -> ring radius ~86, band 66..106 (5px off the ball) */

/* index 0..5 == position 38..43 */
static const char *const btn_text[ASURADA_TB_BTN_COUNT] = {
    "Back", "Fwd", "Wheel", "Right", "Left", "Sniper",
};
/* Arc segment angles (LVGL: 0=east, clockwise; 270=top). The top 4 span the upper
 * arc with small gaps; the 2 bottom buttons sit at the lower corners, leaving the
 * bottom-centre and sides open -- the AdeptBLE cluster shape. */
static const struct { uint16_t start, end; } seg_ang[ASURADA_TB_BTN_COUNT] = {
    {202, 233},  /* Back   upper-left  */
    {237, 268},  /* Fwd    upper-left  */
    {272, 303},  /* Wheel  upper-right */
    {307, 338},  /* Right  upper-right */
    {122, 158},  /* Left   lower-left  */
    { 22,  58},  /* Sniper lower-right */
};
/* label centre offset (~radius 86 = band centre, at each segment's mid-angle) */
static const struct { int16_t x, y; } btn_pos[ASURADA_TB_BTN_COUNT] = {
    {-68, -52},  /* Back   217.5 deg */
    {-26, -82},  /* Fwd    252.5 */
    { 26, -82},  /* Wheel  287.5 */
    { 68, -52},  /* Right  322.5 */
    {-66,  55},  /* Left   140   */
    { 66,  55},  /* Sniper 40    */
};
static bool btn_down[ASURADA_TB_BTN_COUNT];

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct btn_state { uint8_t idx; bool down; bool valid; };

static void render(void) {
    struct zmk_widget_asurada_tb_buttons *w;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, w, node) {
        for (int i = 0; i < ASURADA_TB_BTN_COUNT; i++) {
            bool d = btn_down[i];
            /* Pressed = the segment lights cyan, its label darkens AND pops one
             * size larger -- a tactile button-press feel. */
            lv_obj_set_style_arc_color(w->arc[i], lv_color_hex(d ? SEG_ACTIVE : SEG_IDLE),
                                       LV_PART_MAIN);
            lv_obj_set_style_text_color(w->lbl[i], lv_color_hex(d ? TXT_ACTIVE : TXT_IDLE),
                                        LV_PART_MAIN);
            lv_obj_set_style_text_font(w->lbl[i],
                                       d ? &lv_font_montserrat_14 : &lv_font_montserrat_12,
                                       LV_PART_MAIN);
        }
    }
}

void tb_buttons_update_cb(struct btn_state s) {
    if (!s.valid) {
        return;
    }
    btn_down[s.idx] = s.down;
    render();
}

static struct btn_state tb_buttons_get_state(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL) {
        return (struct btn_state){0, false, false};
    }
    if (ev->position < TB_BTN_FIRST_POS ||
        ev->position >= TB_BTN_FIRST_POS + ASURADA_TB_BTN_COUNT) {
        return (struct btn_state){0, false, false};
    }
    return (struct btn_state){
        .idx = (uint8_t)(ev->position - TB_BTN_FIRST_POS),
        .down = ev->state,
        .valid = true,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(wgt_tb_buttons, struct btn_state, tb_buttons_update_cb,
                            tb_buttons_get_state);
ZMK_SUBSCRIPTION(wgt_tb_buttons, zmk_position_state_changed);

static lv_obj_t *make_arc(lv_obj_t *parent, uint16_t start, uint16_t end) {
    lv_obj_t *a = lv_arc_create(parent);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(a, ARC_SZ, ARC_SZ);
    lv_obj_center(a);
    lv_arc_set_rotation(a, 0);
    lv_arc_set_bg_angles(a, start, end);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);          /* no rect background */
    lv_obj_set_style_arc_width(a, RING_W, LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, lv_color_hex(SEG_IDLE), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);             /* flat ends = segment */
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);    /* no value arc */
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);          /* no knob */
    lv_obj_set_style_pad_all(a, 0, LV_PART_KNOB);
    return a;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *txt) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(TXT_IDLE), LV_PART_MAIN);
    return l;
}

void zmk_widget_asurada_tb_buttons_init(struct zmk_widget_asurada_tb_buttons *w, lv_obj_t *parent) {
    /* Full-page transparent layer; the arc segments + labels are placed
     * absolutely as a ring around the ball. */
    w->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(w->obj);
    lv_obj_set_size(w->obj, 240, 240);
    lv_obj_center(w->obj);
    lv_obj_clear_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

    /* arcs first, then labels on top of them */
    for (int i = 0; i < ASURADA_TB_BTN_COUNT; i++) {
        w->arc[i] = make_arc(w->obj, seg_ang[i].start, seg_ang[i].end);
    }
    for (int i = 0; i < ASURADA_TB_BTN_COUNT; i++) {
        w->lbl[i] = make_label(w->obj, btn_text[i]);
        lv_obj_align(w->lbl[i], LV_ALIGN_CENTER, btn_pos[i].x, btn_pos[i].y);
    }

    sys_slist_append(&widgets, &w->node);
    wgt_tb_buttons_init();
    render();
}

lv_obj_t *zmk_widget_asurada_tb_buttons_obj(struct zmk_widget_asurada_tb_buttons *w) {
    return w->obj;
}
