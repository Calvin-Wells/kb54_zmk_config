#include "status.h"
#include "util.h"

#include <lvgl.h>

#include <zephyr/kernel.h>

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>

// Note: struct zmk_peripheral_battery_state_changed and its ZMK_EVENT_DECLARE are declared
// alongside zmk_battery_state_changed in <zmk/events/battery_state_changed.h> (already included
// above) -- there is no separate peripheral_battery_state_changed.h header in ZMK.

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static lv_draw_label_dsc_t layer_label;
static char layer_text[20] = {};

static lv_draw_label_dsc_t profile_label_left;
static char profile_text_left[10] = {};
static lv_draw_label_dsc_t profile_label_right;
static char profile_text_right[10] = {};

static lv_draw_label_dsc_t battery_label_left;
// Large enough for two LV_SYMBOL_* (up to ~4 bytes each) plus "L 100%  R 100%" style text.
static char battery_text_left[40] = {};

static void draw(struct zmk_widget_status *widget) {
    lv_obj_t *canvas = lv_obj_get_child(zmk_widget_status_obj(widget), 0);
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);

    /////// LAYER
    if (widget->state.layer_label == NULL || strlen(widget->state.layer_label) > 12) {
        snprintf(layer_text, sizeof(layer_text), "LAYER: %i", widget->state.layer_index);
    } else {
        snprintf(layer_text, sizeof(layer_text), "LAYER: %s", widget->state.layer_label);
    }
    lv_canvas_draw_text(canvas, 0, 15, 128, &layer_label, layer_text);

    /////// Battery
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
    char left_part[16] = {};
    char right_part[16] = {};

    if (widget->state.charging) {
        snprintf(left_part, sizeof(left_part), "%s %i%%", LV_SYMBOL_CHARGE,
                 widget->state.battery);
    } else {
        snprintf(left_part, sizeof(left_part), "L %i%%", widget->state.battery);
    }

    if (widget->state.peripheral_battery_valid) {
        snprintf(right_part, sizeof(right_part), "R %i%%", widget->state.peripheral_battery);
    } else {
        snprintf(right_part, sizeof(right_part), "R --");
    }

    snprintf(battery_text_left, sizeof(battery_text_left), "%s  %s", left_part, right_part);
#else
    const char *battery_symbol =
        zmk_widget_battery_symbol(widget->state.battery, widget->state.charging);
    snprintf(battery_text_left, sizeof(battery_text_left), "%s %i%%", battery_symbol,
             widget->state.battery);
#endif /* IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING) */
    lv_canvas_draw_text(canvas, 0, 46, 128, &battery_label_left, battery_text_left);

    /////// PROFILE
    profile_label_left.align = LV_TEXT_ALIGN_LEFT;
    uint8_t profile_text_padding = 10;
    switch (widget->state.selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        profile_text_padding = 0;
        profile_label_left.align = LV_TEXT_ALIGN_CENTER;
        snprintf(profile_text_left, sizeof(profile_text_left), "%s USB", LV_SYMBOL_USB);
        snprintf(profile_text_right, sizeof(profile_text_right), "%s", "");
        break;
    case ZMK_TRANSPORT_BLE:
        snprintf(profile_text_left, sizeof(profile_text_left), "%s %i", LV_SYMBOL_BLUETOOTH,
                 widget->state.active_profile_index);
        if (widget->state.active_profile_bonded) {
            if (widget->state.active_profile_connected) {
                snprintf(profile_text_right, sizeof(profile_text_right), "%s", LV_SYMBOL_OK);
            } else {
                snprintf(profile_text_right, sizeof(profile_text_right), "%s", LV_SYMBOL_CLOSE);
            }
        } else {
            snprintf(profile_text_right, sizeof(profile_text_right), "%s", LV_SYMBOL_SETTINGS);
        }
        break;
    }
    lv_canvas_draw_text(canvas, profile_text_padding, CANVAS_SIZE - 32, 128, &profile_label_left,
                        profile_text_left);
    lv_canvas_draw_text(canvas, -profile_text_padding, CANVAS_SIZE - 32, 128, &profile_label_right,
                        profile_text_right);

    rotate_canvas(canvas, widget->cbuf);
}

//////////////////////////////// Battery ////////////////////////////////////

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
// ZMK only raises battery events when the percentage changes, so the charger finishing (e.g.
// sitting at 100%) wouldn't redraw the charge icon. Re-check periodically while USB power is present.
#define CHARGING_POLL_INTERVAL K_SECONDS(10)

static void charging_poll_work_cb(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(charging_poll_work, charging_poll_work_cb);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

static void set_battery_status(struct zmk_widget_status *widget,
                               struct battery_status_state state) {
    bool usb_present = false;
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    usb_present = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    widget->state.charging = zmk_widget_is_charging(usb_present);

    widget->state.battery = state.level;

    draw(widget);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    if (usb_present) {
        k_work_reschedule(&charging_poll_work, CHARGING_POLL_INTERVAL);
    }
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
static void charging_poll_work_cb(struct k_work *work) {
    widget_battery_status_refresh_state(NULL);
    k_work_submit_to_queue(zmk_display_work_q(), &widget_battery_status_work);
}
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

//////////////////////////////// Peripheral battery ////////////////////////////////////

#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)

struct peripheral_battery_status_state {
    uint8_t level;
    bool valid;
};

static void set_peripheral_battery_status(struct zmk_widget_status *widget,
                                          struct peripheral_battery_status_state state) {
    // The listener's init pass calls get_state(NULL) before any report has arrived; keep
    // showing "R --" until a real one does.
    if (!state.valid) {
        return;
    }

    widget->state.peripheral_battery = state.level;
    widget->state.peripheral_battery_valid = true;

    draw(widget);
}

static void peripheral_battery_status_update_cb(struct peripheral_battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_peripheral_battery_status(widget, state);
    }
}

static struct peripheral_battery_status_state
peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);

    return (struct peripheral_battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : 0,
        .valid = (ev != NULL),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_battery_status,
                            struct peripheral_battery_status_state,
                            peripheral_battery_status_update_cb,
                            peripheral_battery_status_get_state)
ZMK_SUBSCRIPTION(widget_peripheral_battery_status, zmk_peripheral_battery_state_changed);

#endif /* IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING) */

//////////////////////////////// Profiles ////////////////////////////////////

static void set_output_status(struct zmk_widget_status *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index + 1;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;

    draw(widget);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_output_status(widget, &state); }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

//////////////////////////////// Layers ////////////////////////////////////

static void set_layer_status(struct zmk_widget_status *widget, struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw(widget);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){.index = index, .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

//////////////////////////////// Initialization ////////////////////////////////////

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_SIZE, CANVAS_SIZE);

    // Canvas.
    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);

    // Layer.
    lv_draw_label_dsc_init(&layer_label);
    layer_label.color = lv_color_black();
    layer_label.align = LV_TEXT_ALIGN_CENTER;
    layer_label.font = &lv_font_unscii_8;

    // Battery.
    lv_draw_label_dsc_init(&battery_label_left);
    battery_label_left.color = lv_color_black();
    battery_label_left.align = LV_TEXT_ALIGN_CENTER;
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
    // The combined "L xx%  R xx%" text is wider than a single battery readout, so use a smaller
    // font to keep it within the 128px canvas width.
    battery_label_left.font = &lv_font_montserrat_14;
#else
    battery_label_left.font = &lv_font_montserrat_22;
#endif /* IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING) */

    // Profile.
    lv_draw_label_dsc_init(&profile_label_left);
    profile_label_left.color = lv_color_black();
    profile_label_left.align = LV_TEXT_ALIGN_LEFT;
    profile_label_left.font = &lv_font_montserrat_22;
    lv_draw_label_dsc_init(&profile_label_right);
    profile_label_right.color = lv_color_black();
    profile_label_right.align = LV_TEXT_ALIGN_RIGHT;
    profile_label_right.font = &lv_font_montserrat_22;

    sys_slist_append(&widgets, &widget->node);
    widget_layer_status_init();
    widget_battery_status_init();
    widget_output_status_init();
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
    widget_peripheral_battery_status_init();
#endif /* IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING) */

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
