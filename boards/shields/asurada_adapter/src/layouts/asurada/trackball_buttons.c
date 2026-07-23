#include "trackball_buttons.h"

#include <zmk/display.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/event_manager.h>

#include <fonts.h>
#include "display_colors.h"

/*
 * Trackball button legend + live state. The six trackball buttons arrive at the
 * central as split key positions 38..43 (base-layer bindings):
 *   38 MB4   39 MB5   40 MCLK   41 RCLK   42 LCLK   43 SNIPE
 * Laid out to match the physical cluster (4 across the top, 2 at the bottom
 * corners); each label lights cyan while its button is held -- a live reference
 * AND state. RCLK/SNIPE also scroll when held long; the label shows the tap name.
 */

#define TB_BTN_FIRST_POS 38
#define BTN_IDLE   0x5A6A72   /* dim slate when released */
#define BTN_ACTIVE 0x35E0FF   /* cyan when pressed       */

/* index 0..5 == position 38..43 */
static const char *const btn_text[ASURADA_TB_BTN_COUNT] = {
    "Back", "Fwd", "Wheel", "Right", "Left", "Sniper",
};
static bool btn_down[ASURADA_TB_BTN_COUNT];

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct btn_state { uint8_t idx; bool down; bool valid; };

static void render(void) {
    struct zmk_widget_asurada_tb_buttons *w;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, w, node) {
        for (int i = 0; i < ASURADA_TB_BTN_COUNT; i++) {
            lv_obj_set_style_text_color(
                w->lbl[i], lv_color_hex(btn_down[i] ? BTN_ACTIVE : BTN_IDLE), LV_PART_MAIN);
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

/* Offsets from the page centre for each button, arranged as a RING around the
 * ball that keeps the physical cluster's left/right + top/bottom sense (clock:
 * Back 10, Fwd 11, Wheel 1, Right 2 across the top; Left 8, Sniper 4 at the lower
 * corners). Radius ~88px clears the 132px ball and the round edge both -- so the
 * labels sit around the sphere, not over it. */
static const struct { int16_t x, y; } btn_pos[ASURADA_TB_BTN_COUNT] = {
    {-76, -44},  /* Back   (10 o'clock) */
    {-44, -76},  /* Fwd    (11) */
    { 44, -76},  /* Wheel  (1)  */
    { 76, -44},  /* Right  (2)  */
    {-76,  44},  /* Left   (8)  */
    { 76,  44},  /* Sniper (4)  */
};

static lv_obj_t *make_label(lv_obj_t *parent, const char *txt) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(BTN_IDLE), LV_PART_MAIN);
    return l;
}

void zmk_widget_asurada_tb_buttons_init(struct zmk_widget_asurada_tb_buttons *w, lv_obj_t *parent) {
    /* Full-page transparent layer; the six labels are placed absolutely in a ring
     * around the ball (see btn_pos). */
    w->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(w->obj);
    lv_obj_set_size(w->obj, 240, 240);
    lv_obj_center(w->obj);
    lv_obj_clear_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

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
