#pragma once

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include <zmk/endpoints.h>

#define CANVAS_SIZE 128

// LVGL 9 canvas buffers are addressed by color format rather than lv_color_t, and must be sized
// (and stride-aligned) via LV_CANVAS_BUF_SIZE. L8 is the smallest format lv_draw_sw_rotate
// supports; the actual 1-bit Sharp memory LCD framebuffer conversion happens separately (see
// LV_COLOR_DEPTH_1 / LV_Z_BITS_PER_PIXEL in Kconfig.defconfig).
#define CANVAS_COLOR_FORMAT LV_COLOR_FORMAT_L8
#define CANVAS_BUF_SIZE                                                                            \
    LV_CANVAS_BUF_SIZE(CANVAS_SIZE, CANVAS_SIZE, LV_COLOR_FORMAT_GET_BPP(CANVAS_COLOR_FORMAT),     \
                       LV_DRAW_BUF_STRIDE_ALIGN)

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

// Rotates the canvas' own draw buffer 180 degrees in place (the display is mounted upside down).
void rotate_canvas(lv_obj_t *canvas);

// Draws text into the canvas via a temporary draw layer, mirroring the nice!view LVGL 9 helper of
// the same name (app/boards/shields/nice_view/widgets/util.c on ZMK main).
void canvas_draw_text(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t max_w,
                      lv_draw_label_dsc_t *draw_dsc, const char *txt);

// Returns the LV_SYMBOL_* string to display for the given battery level / charging state.
// Shared by both the central and peripheral status widgets so the thresholds stay in sync.
const char *zmk_widget_battery_symbol(uint8_t level, bool charging);

// Returns whether the local half is currently charging.
//
// If the board defines a `charging-pin` alias (see kb54_nrf52840.dtsi), the actual charger status
// GPIO is read. Otherwise this falls back to `usb_present`, matching the previous behavior of
// treating "USB present" as "charging".
bool zmk_widget_is_charging(bool usb_present);
