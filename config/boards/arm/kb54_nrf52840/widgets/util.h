#pragma once

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include <zmk/endpoints.h>

#define CANVAS_SIZE 128

struct status_state {
    uint8_t battery;
    bool charging;
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    uint8_t layer_index;
    const char *layer_label;
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
    uint8_t peripheral_battery;
    bool peripheral_battery_valid;
#endif /* IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING) */
#else
    bool connected;
#endif
};

struct battery_status_state {
    uint8_t level;
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    bool usb_present;
#endif
};

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
};

struct layer_status_state {
    uint8_t index;
    const char *label;
};

struct peripheral_status_state {
    bool connected;
};

void rotate_canvas(lv_obj_t *canvas, lv_color_t *cbuf);

// Returns the LV_SYMBOL_* string to display for the given battery level / charging state.
// Shared by both the central and peripheral status widgets so the thresholds stay in sync.
const char *zmk_widget_battery_symbol(uint8_t level, bool charging);

// Returns whether the local half is currently charging.
//
// If the board defines a `charging-pin` alias (see kb54_nrf52840.dtsi), the actual charger status
// GPIO is read. Otherwise this falls back to `usb_present`, matching the previous behavior of
// treating "USB present" as "charging".
bool zmk_widget_is_charging(bool usb_present);
