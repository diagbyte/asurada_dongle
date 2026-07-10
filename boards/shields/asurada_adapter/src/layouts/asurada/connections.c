#include "connections.h"

#include <zmk/display.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <zmk/event_manager.h>

#include <fonts.h>
#include "display_colors.h"
#include "asurada_battery.h"

/*
 * Connections page: a title plus one row per split peripheral (up to
 * ASURADA_CONN_ROWS), each labeled from CONFIG_ASURADA_CONN_LABEL_n (defaults
 * Left/Right/Trackball; slot order 0/1/2, confirm on hardware) -- each row a
 * small connection-status dot and a battery-percent label.
 *
 * Mirrors battery_circles.c's dual ZMK listener wiring: a per-source battery
 * level event and a per-source connection-state event, each fanned out to
 * every registered widget instance. Only the per-row rendering is simpler
 * (a colored dot + "NN%"/"--" label instead of arcs/bars).
 */

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

#ifndef PERIPHERAL_COUNT
#define PERIPHERAL_COUNT ZMK_SPLIT_BLE_PERIPHERAL_COUNT
#endif

#define CONN_DOT_CONNECTED    0x35C46B
#define CONN_DOT_DISCONNECTED 0xF0564D

/* Rendered rows = the smaller of the actual peripheral count and our label
 * capacity; the fixed dot[]/pct[]/row_battery[]/row_connected[] arrays stay
 * sized ASURADA_CONN_ROWS so out-of-range indices are always memory-safe. */
#define CONN_N_ROWS MIN(PERIPHERAL_COUNT, ASURADA_CONN_ROWS)

static const char *const row_names[ASURADA_CONN_ROWS] = {
    CONFIG_ASURADA_CONN_LABEL_0, CONFIG_ASURADA_CONN_LABEL_1,
    CONFIG_ASURADA_CONN_LABEL_2, CONFIG_ASURADA_CONN_LABEL_3,
};

static uint8_t row_battery[ASURADA_CONN_ROWS] = {0};
static bool row_connected[ASURADA_CONN_ROWS] = {false};

struct battery_update_state {
    uint8_t source;
    uint8_t level;
};

struct connection_update_state {
    uint8_t source;
    bool connected;
};

static void update_row_display(uint8_t row) {
    if (row >= ASURADA_CONN_ROWS) {
        return;
    }

    bool connected = row_connected[row];
    uint8_t level = row_battery[row];

    struct zmk_widget_asurada_connections *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        lv_obj_t *dot = widget->dot[row];
        lv_obj_t *pct = widget->pct[row];

        if (dot) {
            lv_obj_set_style_bg_color(
                dot, lv_color_hex(connected ? CONN_DOT_CONNECTED : CONN_DOT_DISCONNECTED),
                LV_PART_MAIN);
        }

        if (pct) {
            char text[5];
            if (connected) {
                snprintf(text, sizeof(text), "%d%%", level);
            } else {
                snprintf(text, sizeof(text), "--");
            }
            lv_label_set_text(pct, text);
        }
    }
}

static void set_battery_level(uint8_t source, uint8_t level) {
    if (source >= PERIPHERAL_COUNT || source >= CONN_N_ROWS) {
        return;
    }
    row_battery[source] = asurada_battery_display_pct(level);
    update_row_display(source);
}

static void set_connection_status(uint8_t source, bool connected) {
    if (source >= PERIPHERAL_COUNT || source >= CONN_N_ROWS) {
        return;
    }
    row_connected[source] = connected;
    update_row_display(source);
}

void connections_battery_update_cb(struct battery_update_state state) {
    set_battery_level(state.source, state.level);
}

static struct battery_update_state connections_get_battery_state(const zmk_event_t *eh) {
    if (eh == NULL) {
        return (struct battery_update_state){.source = 0, .level = 0};
    }

    const struct zmk_peripheral_battery_state_changed *bat_ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (bat_ev == NULL) {
        return (struct battery_update_state){.source = 0, .level = 0};
    }

    return (struct battery_update_state){
        .source = bat_ev->source,
        .level = bat_ev->state_of_charge,
    };
}

void connections_connection_update_cb(struct connection_update_state state) {
    set_connection_status(state.source, state.connected);
}

static struct connection_update_state connections_get_connection_state(const zmk_event_t *eh) {
    if (eh == NULL) {
        return (struct connection_update_state){.source = 0, .connected = false};
    }

    const struct zmk_split_central_status_changed *conn_ev =
        as_zmk_split_central_status_changed(eh);
    if (conn_ev == NULL) {
        return (struct connection_update_state){.source = 0, .connected = false};
    }

    return (struct connection_update_state){
        .source = conn_ev->slot,
        .connected = conn_ev->connected,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_connections_battery, struct battery_update_state,
                            connections_battery_update_cb, connections_get_battery_state);
ZMK_SUBSCRIPTION(widget_asurada_connections_battery, zmk_peripheral_battery_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_asurada_connections_connection, struct connection_update_state,
                            connections_connection_update_cb, connections_get_connection_state);
ZMK_SUBSCRIPTION(widget_asurada_connections_connection, zmk_split_central_status_changed);

void zmk_widget_asurada_connections_init(struct zmk_widget_asurada_connections *w, lv_obj_t *parent) {
    w->obj = lv_obj_create(parent);
    lv_obj_set_size(w->obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(w->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(w->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(w->obj, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(w->obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(w->obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(w->obj, 12, LV_PART_MAIN);
    lv_obj_clear_flag(w->obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(w->obj);
    lv_label_set_text(title, "Connections");
    lv_obj_set_style_text_font(title, &FG_Medium_21, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(DISPLAY_COLOR_LAYER_TEXT), LV_PART_MAIN);

    for (int i = 0; i < CONN_N_ROWS; i++) {
        lv_obj_t *row = lv_obj_create(w->obj);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 176, 28);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *dot = lv_obj_create(row);
        w->dot[i] = dot;
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 14, 14);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, lv_color_hex(CONN_DOT_DISCONNECTED), LV_PART_MAIN);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, row_names[i]);
        /* FG_Medium_21 covers 0x20-0x7F (incl. lowercase); FG_Medium_20 stops at
         * 0x60, so mixed-case labels like "Left"/"Trackball" rendered as tofu. */
        lv_obj_set_style_text_font(name, &FG_Medium_21, LV_PART_MAIN);
        lv_obj_set_style_text_color(name, lv_color_hex(DISPLAY_COLOR_LAYER_TEXT), LV_PART_MAIN);
        lv_obj_set_flex_grow(name, 1);

        lv_obj_t *pct = lv_label_create(row);
        w->pct[i] = pct;
        lv_label_set_text(pct, "--");
        lv_obj_set_style_text_font(pct, &FG_Medium_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(pct, lv_color_hex(DISPLAY_COLOR_LAYER_TEXT), LV_PART_MAIN);
    }

    widget_asurada_connections_battery_init();
    widget_asurada_connections_connection_init();

    sys_slist_append(&widgets, &w->node);
}

lv_obj_t *zmk_widget_asurada_connections_obj(struct zmk_widget_asurada_connections *w) {
    return w->obj;
}
