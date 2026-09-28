#include "status.h"
#include "util.h"

#include <lvgl.h>

#include <zephyr/kernel.h>

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/usb.h>

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static lv_draw_label_dsc_t connection_label;
static char connection_text[10] = {};

static lv_draw_label_dsc_t battery_label_left;
static char battery_text_left[10] = {};

static void draw(struct zmk_widget_status *widget) {
    lv_obj_t *canvas = lv_obj_get_child(zmk_widget_status_obj(widget), 0);
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);

    /////// Battery
    const char *battery_symbol =
        zmk_widget_battery_symbol(widget->state.battery, widget->state.charging);
    snprintf(battery_text_left, sizeof(battery_text_left), "%s %i%%", battery_symbol,
             widget->state.battery);
    canvas_draw_text(canvas, 0, 46, 128, &battery_label_left, battery_text_left);

    /////// PROFILE
    snprintf(connection_text, sizeof(connection_text), "%s",
             widget->state.connected ? (LV_SYMBOL_BLUETOOTH " " LV_SYMBOL_OK)
                                     : (LV_SYMBOL_BLUETOOTH " " LV_SYMBOL_CLOSE));
    canvas_draw_text(canvas, 0, CANVAS_SIZE - 32, 128, &connection_label, connection_text);

    rotate_canvas(canvas);
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

//////////////////////////////// Peripheral status ////////////////////////////////////

static struct peripheral_status_state get_state(const zmk_event_t *_eh) {
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected()};
}

static void set_connection_status(struct zmk_widget_status *widget,
                                  struct peripheral_status_state state) {
    widget->state.connected = state.connected;

    draw(widget);
}

static void output_status_update_cb(struct peripheral_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_connection_status(widget, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);

//////////////////////////////// Initialization ////////////////////////////////////

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_SIZE, CANVAS_SIZE);

    // Canvas.
    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    // Peripheral.
    lv_draw_label_dsc_init(&connection_label);
    connection_label.color = lv_color_black();
    connection_label.align = LV_TEXT_ALIGN_CENTER;
    connection_label.font = &lv_font_montserrat_22;

    // Battery.
    lv_draw_label_dsc_init(&battery_label_left);
    battery_label_left.color = lv_color_black();
    battery_label_left.align = LV_TEXT_ALIGN_CENTER;
    battery_label_left.font = &lv_font_montserrat_22;

    sys_slist_append(&widgets, &widget->node);
    widget_peripheral_status_init();
    widget_battery_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
