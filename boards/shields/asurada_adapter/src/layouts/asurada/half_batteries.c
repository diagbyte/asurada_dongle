#include "half_batteries.h"

#include <zmk/display.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <zmk/event_manager.h>

#include <fonts.h>
#include "display_colors.h"

/*
 * Keyboard-page battery: one slim cell per keyboard half (split slots 0 and 1),
 * in the same battery-glyph style as the trackball page, each prefixed with the
 * half's letter (L / R, from CONFIG_ASURADA_CONN_LABEL_0/1). The trackball is
 * intentionally NOT shown here -- it lives on its own page. Mirrors
 * trackball_battery.c's dual ZMK listeners, indexed per slot.
 */

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

#define BATT_HIGH  0x35C46B   /* >50% green  */
#define BATT_MID   0xF5A623   /* 20-50% amber */
#define BATT_LOW   0xF0564D   /* <20% red    */
#define BATT_OFF   0x505050   /* disconnected */
#define FILL_MAX_W 18         /* px, inner fill at 100% */

static const char *const half_label[ASURADA_HALF_COUNT] = {
    CONFIG_ASURADA_CONN_LABEL_0, CONFIG_ASURADA_CONN_LABEL_1,
};
static uint8_t half_level[ASURADA_HALF_COUNT];
static bool half_conn[ASURADA_HALF_COUNT];

struct battery_update_state { uint8_t source; uint8_t level; };
struct connection_update_state { uint8_t source; bool connected; };

static uint32_t level_color(uint8_t level) {
    if (level > 50) return BATT_HIGH;
    if (level > 20) return BATT_MID;
    return BATT_LOW;
}

static void render(void) {
    struct zmk_widget_asurada_half_batteries *w;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, w, node) {
        for (int i = 0; i < ASURADA_HALF_COUNT; i++) {
            if (half_conn[i]) {
                int fw = (int)half_level[i] * FILL_MAX_W / 100;
                if (fw < 1) fw = 1;
                lv_obj_set_width(w->fill[i], fw);
                lv_obj_set_style_bg_color(w->fill[i], lv_color_hex(level_color(half_level[i])), LV_PART_MAIN);
                lv_obj_set_style_text_color(w->pct[i], lv_color_hex(level_color(half_level[i])), LV_PART_MAIN);
                char t[5];
                snprintf(t, sizeof(t), "%d%%", half_level[i]);
                lv_label_set_text(w->pct[i], t);
            } else {
                lv_obj_set_width(w->fill[i], 1);
                lv_obj_set_style_bg_color(w->fill[i], lv_color_hex(BATT_OFF), LV_PART_MAIN);
                lv_obj_set_style_text_color(w->pct[i], lv_color_hex(BATT_OFF), LV_PART_MAIN);
                lv_label_set_text(w->pct[i], "--");
            }
        }
    }
}

static void set_level(uint8_t source, uint8_t level) {
    if (source >= ASURADA_HALF_COUNT) return;
    half_level[source] = level;
    render();
}

static void set_conn(uint8_t source, bool connected) {
    if (source >= ASURADA_HALF_COUNT) return;
    half_conn[source] = connected;
    render();
}

void half_batteries_battery_update_cb(struct battery_update_state s) { set_level(s.source, s.level); }

static struct battery_update_state half_batteries_get_battery_state(const zmk_event_t *eh) {
    if (eh == NULL) return (struct battery_update_state){0, 0};
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    if (ev == NULL) return (struct battery_update_state){0, 0};
    return (struct battery_update_state){.source = ev->source, .level = ev->state_of_charge};
}

void half_batteries_connection_update_cb(struct connection_update_state s) { set_conn(s.source, s.connected); }

static struct connection_update_state half_batteries_get_connection_state(const zmk_event_t *eh) {
    if (eh == NULL) return (struct connection_update_state){0, false};
    const struct zmk_split_central_status_changed *ev = as_zmk_split_central_status_changed(eh);
    if (ev == NULL) return (struct connection_update_state){0, false};
    return (struct connection_update_state){.source = ev->slot, .connected = ev->connected};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_half_batteries_battery, struct battery_update_state,
                            half_batteries_battery_update_cb, half_batteries_get_battery_state);
ZMK_SUBSCRIPTION(widget_asurada_half_batteries_battery, zmk_peripheral_battery_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_half_batteries_connection, struct connection_update_state,
                            half_batteries_connection_update_cb, half_batteries_get_connection_state);
ZMK_SUBSCRIPTION(widget_asurada_half_batteries_connection, zmk_split_central_status_changed);

static void make_cell(struct zmk_widget_asurada_half_batteries *w, int i) {
    lv_obj_t *cell = lv_obj_create(w->obj);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, LV_SIZE_CONTENT, 20);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cell, 4, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    /* half letter (L / R) */
    lv_obj_t *lbl = lv_label_create(cell);
    char letter[2] = { half_label[i][0] ? half_label[i][0] : (char)('L' + i), '\0' };
    lv_label_set_text(lbl, letter);
    lv_obj_set_style_text_font(lbl, &FG_Medium_21, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(DISPLAY_COLOR_LAYER_TEXT), LV_PART_MAIN);

    /* battery body: bordered rounded rect */
    lv_obj_t *body = lv_obj_create(cell);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, 24, 13);
    lv_obj_set_style_radius(body, 3, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(body, lv_color_hex(0x9AB0B8), LV_PART_MAIN);
    lv_obj_set_style_border_opa(body, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 1, LV_PART_MAIN);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    w->fill[i] = lv_obj_create(body);
    lv_obj_remove_style_all(w->fill[i]);
    lv_obj_set_size(w->fill[i], 1, 7);
    lv_obj_align(w->fill[i], LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(w->fill[i], 1, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(w->fill[i], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(w->fill[i], lv_color_hex(BATT_OFF), LV_PART_MAIN);

    /* positive nub */
    lv_obj_t *nub = lv_obj_create(cell);
    lv_obj_remove_style_all(nub);
    lv_obj_set_size(nub, 3, 6);
    lv_obj_set_style_radius(nub, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(nub, lv_color_hex(0x9AB0B8), LV_PART_MAIN);

    /* percentage */
    w->pct[i] = lv_label_create(cell);
    lv_label_set_text(w->pct[i], "--");
    lv_obj_set_style_text_font(w->pct[i], &FG_Medium_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(w->pct[i], lv_color_hex(BATT_OFF), LV_PART_MAIN);
}

void zmk_widget_asurada_half_batteries_init(struct zmk_widget_asurada_half_batteries *w, lv_obj_t *parent) {
    w->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(w->obj);
    lv_obj_set_size(w->obj, LV_SIZE_CONTENT, 24);
    lv_obj_set_flex_flow(w->obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(w->obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(w->obj, 16, LV_PART_MAIN);
    lv_obj_clear_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < ASURADA_HALF_COUNT; i++) {
        make_cell(w, i);
    }

    sys_slist_append(&widgets, &w->node);
    widget_asurada_half_batteries_battery_init();
    widget_asurada_half_batteries_connection_init();
    render();
}

lv_obj_t *zmk_widget_asurada_half_batteries_obj(struct zmk_widget_asurada_half_batteries *w) {
    return w->obj;
}
