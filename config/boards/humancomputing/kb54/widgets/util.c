#include "util.h"

#include <lvgl.h>

#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#if DT_NODE_EXISTS(DT_ALIAS(charging_pin))
#include <zephyr/drivers/gpio.h>
#endif

void rotate_canvas(lv_obj_t *canvas) {
    // Ported from nice!view's LVGL 9 util.c (app/boards/shields/nice_view/widgets/util.c on ZMK
    // main): lv_canvas_transform() no longer exists in LVGL 9, so rotation is done directly on
    // the canvas' own draw buffer with lv_draw_sw_rotate(). 180 degrees here (vs. nice!view's
    // 270) because this display is mounted upside down rather than sideways.
    uint8_t *buf = lv_canvas_get_draw_buf(canvas)->data;
    static uint8_t buf_copy[CANVAS_BUF_SIZE];
    memcpy(buf_copy, buf, sizeof(buf_copy));

    const uint32_t stride = lv_draw_buf_width_to_stride(CANVAS_SIZE, CANVAS_COLOR_FORMAT);
    lv_draw_sw_rotate(buf_copy, buf, CANVAS_SIZE, CANVAS_SIZE, stride, stride,
                      LV_DISPLAY_ROTATION_180, CANVAS_COLOR_FORMAT);
}

void canvas_draw_text(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t max_w,
                      lv_draw_label_dsc_t *draw_dsc, const char *txt) {
    // Ported from nice!view's LVGL 9 util.c: LVGL 9 removed lv_canvas_draw_text() in favor of
    // drawing into a temporary layer with lv_draw_label().
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    draw_dsc->text = txt;
    lv_area_t coords = {x, y, x + max_w, y + CANVAS_SIZE};
    lv_draw_label(&layer, draw_dsc, &coords);

    lv_canvas_finish_layer(canvas, &layer);
}

const char *zmk_widget_battery_symbol(uint8_t level, bool charging) {
    if (charging) {
        return LV_SYMBOL_CHARGE;
    }

    if (level >= 95) {
        return LV_SYMBOL_BATTERY_FULL;
    } else if (level >= 65) {
        return LV_SYMBOL_BATTERY_3;
    } else if (level >= 35) {
        return LV_SYMBOL_BATTERY_2;
    } else if (level > 5) {
        return LV_SYMBOL_BATTERY_1;
    }

    return LV_SYMBOL_BATTERY_EMPTY;
}

//////////////////////////////// Charging pin ////////////////////////////////////

// The board exposes the PMIC's charge-status output as the `charging-pin` alias (active low,
// pulled up -- see kb54_nrf52840.dtsi). No Zephyr driver currently binds to this GPIO: the
// devicetree node it lives under is only a `gpio-keys` node for convenience/documentation, and
// the `gpio_keys` input driver is gated on `CONFIG_INPUT`, which this board does not enable (and
// nothing else on this board turns on). So it is safe for us to configure and read the pin
// directly here.
#if DT_NODE_EXISTS(DT_ALIAS(charging_pin))

static const struct gpio_dt_spec charging_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(charging_pin), gpios);
static bool charging_gpio_ready;

static int charging_gpio_init(void) {
    if (!gpio_is_ready_dt(&charging_gpio)) {
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&charging_gpio, GPIO_INPUT);
    if (ret < 0) {
        return ret;
    }

    charging_gpio_ready = true;

    return 0;
}

SYS_INIT(charging_gpio_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* DT_NODE_EXISTS(DT_ALIAS(charging_pin)) */

bool zmk_widget_is_charging(bool usb_present) {
#if DT_NODE_EXISTS(DT_ALIAS(charging_pin))
    if (charging_gpio_ready) {
        return gpio_pin_get_dt(&charging_gpio) > 0;
    }
#endif /* DT_NODE_EXISTS(DT_ALIAS(charging_pin)) */

    return usb_present;
}
