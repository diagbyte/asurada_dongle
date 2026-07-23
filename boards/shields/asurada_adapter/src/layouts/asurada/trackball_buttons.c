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

static lv_obj_t *make_label(lv_obj_t *parent, const char *txt) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(BTN_IDLE), LV_PART_MAIN);
    /* dark chip so the label stays legible over the rolling ball */
    lv_obj_set_style_bg_color(l, lv_color_hex(0x0A0F12), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(l, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_pad_left(l, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_right(l, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_top(l, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(l, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(l, 3, LV_PART_MAIN);
    return l;
}

static lv_obj_t *make_row(lv_obj_t *parent) {
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, 200, 18);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    return r;
}

void zmk_widget_asurada_tb_buttons_init(struct zmk_widget_asurada_tb_buttons *w, lv_obj_t *parent) {
    w->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(w->obj);
    lv_obj_set_size(w->obj, 200, 40);
    lv_obj_set_flex_flow(w->obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(w->obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(w->obj, 4, LV_PART_MAIN);
    lv_obj_clear_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *top = make_row(w->obj);
    w->lbl[0] = make_label(top, btn_text[0]);  /* Back  (MB4)  */
    w->lbl[1] = make_label(top, btn_text[1]);  /* Fwd   (MB5)  */
    w->lbl[2] = make_label(top, btn_text[2]);  /* Wheel (MCLK) */
    w->lbl[3] = make_label(top, btn_text[3]);  /* Right (RCLK) */

    lv_obj_t *bot = make_row(w->obj);
    w->lbl[4] = make_label(bot, btn_text[4]);  /* Left   (LCLK)  */
    w->lbl[5] = make_label(bot, btn_text[5]);  /* Sniper (SNIPE) */

    sys_slist_append(&widgets, &w->node);
    wgt_tb_buttons_init();
    render();
}

lv_obj_t *zmk_widget_asurada_tb_buttons_obj(struct zmk_widget_asurada_tb_buttons *w) {
    return w->obj;
}
