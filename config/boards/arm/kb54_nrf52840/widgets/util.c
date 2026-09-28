#include "util.h"

#include <lvgl.h>

#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#if DT_NODE_EXISTS(DT_ALIAS(charging_pin))
#include <zephyr/drivers/gpio.h>
#endif

void rotate_canvas(lv_obj_t *canvas, lv_color_t *cbuf) {
    static lv_color_t cbuf_tmp[CANVAS_SIZE * CANVAS_SIZE];
    memcpy(cbuf_tmp, cbuf, sizeof(cbuf_tmp));
    lv_img_dsc_t img;
    img.data = (void *)cbuf_tmp;
    img.header.cf = LV_IMG_CF_TRUE_COLOR;
    img.header.w = CANVAS_SIZE;
    img.header.h = CANVAS_SIZE;
    lv_canvas_transform(canvas, &img, 1800, LV_IMG_ZOOM_NONE, 0, 0, CANVAS_SIZE / 2,
                        CANVAS_SIZE / 2, false);
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
