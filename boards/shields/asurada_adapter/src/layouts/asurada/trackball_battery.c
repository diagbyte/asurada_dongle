#include "trackball_battery.h"

#include <zmk/display.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <zmk/event_manager.h>

#include <fonts.h>
#include "display_colors.h"

/* Battery of the trackball peripheral (CONFIG_ASURADA_TRACKBALL_SLOT), shown on
 * the trackball page as a small battery glyph + percentage. Mirrors
 * connections.c's dual ZMK listeners; only this one slot is displayed. */

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

#ifndef PERIPHERAL_COUNT
#define PERIPHERAL_COUNT ZMK_SPLIT_BLE_PERIPHERAL_COUNT
#endif

#define TB_SLOT      CONFIG_ASURADA_TRACKBALL_SLOT
#define BATT_HIGH    0x35C46B   /* >50% green  */
#define BATT_MID     0xF5A623   /* 20–50% amber */
#define BATT_LOW     0xF0564D   /* <20% red    */
#define BATT_OFF     0x505050   /* disconnected */
#define FILL_MAX_W   20         /* px, inner fill at 100% */

static uint8_t tb_level = 0;
static bool tb_connected = false;

struct battery_update_state { uint8_t source; uint8_t level; };
struct connection_update_state { uint8_t source; bool connected; };

static uint32_t level_color(uint8_t level) {
    if (level > 50) return BATT_HIGH;
    if (level > 20) return BATT_MID;
    return BATT_LOW;
}

static void tb_battery_render(void) {
    struct zmk_widget_asurada_tb_battery *w;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, w, node) {
        if (tb_connected) {
            int fw = (int)tb_level * FILL_MAX_W / 100;
            if (fw < 1) fw = 1;
            lv_obj_set_width(w->fill, fw);
            lv_obj_set_style_bg_color(w->fill, lv_color_hex(level_color(tb_level)), LV_PART_MAIN);
            lv_obj_set_style_bg_color(w->pct, lv_color_hex(level_color(tb_level)), LV_PART_MAIN); /* no-op if pct has no bg */
            lv_obj_set_style_text_color(w->pct, lv_color_hex(level_color(tb_level)), LV_PART_MAIN);
            char t[5];
            snprintf(t, sizeof(t), "%d%%", tb_level);
            lv_label_set_text(w->pct, t);
        } else {
            lv_obj_set_width(w->fill, 1);
            lv_obj_set_style_bg_color(w->fill, lv_color_hex(BATT_OFF), LV_PART_MAIN);
            lv_obj_set_style_text_color(w->pct, lv_color_hex(BATT_OFF), LV_PART_MAIN);
            lv_label_set_text(w->pct, "--");
        }
    }
}

static void set_battery_level(uint8_t source, uint8_t level) {
    if (source != TB_SLOT) return;
    tb_level = level;
    tb_battery_render();
}

static void set_connection_status(uint8_t source, bool connected) {
    if (source != TB_SLOT) return;
    tb_connected = connected;
    tb_battery_render();
}

void tb_battery_battery_update_cb(struct battery_update_state state) {
    set_battery_level(state.source, state.level);
}

static struct battery_update_state tb_battery_get_battery_state(const zmk_event_t *eh) {
    if (eh == NULL) return (struct battery_update_state){0, 0};
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    if (ev == NULL) return (struct battery_update_state){0, 0};
    return (struct battery_update_state){.source = ev->source, .level = ev->state_of_charge};
}

void tb_battery_connection_update_cb(struct connection_update_state state) {
    set_connection_status(state.source, state.connected);
}

static struct connection_update_state tb_battery_get_connection_state(const zmk_event_t *eh) {
    if (eh == NULL) return (struct connection_update_state){0, false};
    const struct zmk_split_central_status_changed *ev = as_zmk_split_central_status_changed(eh);
    if (ev == NULL) return (struct connection_update_state){0, false};
    return (struct connection_update_state){.source = ev->slot, .connected = ev->connected};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_tb_battery_battery, struct battery_update_state,
                            tb_battery_battery_update_cb, tb_battery_get_battery_state);
ZMK_SUBSCRIPTION(widget_asurada_tb_battery_battery, zmk_peripheral_battery_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_tb_battery_connection, struct connection_update_state,
                            tb_battery_connection_update_cb, tb_battery_get_connection_state);
ZMK_SUBSCRIPTION(widget_asurada_tb_battery_connection, zmk_split_central_status_changed);

void zmk_widget_asurada_tb_battery_init(struct zmk_widget_asurada_tb_battery *w, lv_obj_t *parent) {
    /* container: [ battery body (+nub) ] [ NN% ] laid out in a row */
    w->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(w->obj);
    lv_obj_set_size(w->obj, 74, 20);
    lv_obj_set_flex_flow(w->obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(w->obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(w->obj, 6, LV_PART_MAIN);
    lv_obj_clear_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

    /* battery body: bordered rounded rect */
    lv_obj_t *body = lv_obj_create(w->obj);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, 26, 14);
    lv_obj_set_style_radius(body, 3, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(body, lv_color_hex(0x9AB0B8), LV_PART_MAIN);
    lv_obj_set_style_border_opa(body, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 1, LV_PART_MAIN);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    /* inner fill bar, left-aligned, width ∝ level */
    w->fill = lv_obj_create(body);
    lv_obj_remove_style_all(w->fill);
    lv_obj_set_size(w->fill, 1, 8);
    lv_obj_align(w->fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(w->fill, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(w->fill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(w->fill, lv_color_hex(BATT_OFF), LV_PART_MAIN);

    /* positive nub on the right of the body */
    lv_obj_t *nub = lv_obj_create(w->obj);
    lv_obj_remove_style_all(nub);
    lv_obj_set_size(nub, 3, 7);
    lv_obj_set_style_radius(nub, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(nub, lv_color_hex(0x9AB0B8), LV_PART_MAIN);

    /* percentage label */
    w->pct = lv_label_create(w->obj);
    lv_label_set_text(w->pct, "--");
    lv_obj_set_style_text_font(w->pct, &FG_Medium_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(w->pct, lv_color_hex(BATT_OFF), LV_PART_MAIN);

    sys_slist_append(&widgets, &w->node);
    widget_asurada_tb_battery_battery_init();
    widget_asurada_tb_battery_connection_init();
    tb_battery_render();
}

lv_obj_t *zmk_widget_asurada_tb_battery_obj(struct zmk_widget_asurada_tb_battery *w) {
    return w->obj;
}
