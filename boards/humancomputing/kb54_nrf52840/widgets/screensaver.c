#include "screensaver.h"
#include "util.h"

#include <lvgl.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/sensor_event.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

#if IS_ENABLED(CONFIG_ZMK_SLEEP)

// Show the screensaver this long before ZMK's activity tracker puts the half into deep sleep
// (CONFIG_ZMK_IDLE_SLEEP_TIMEOUT, 15 min by default -> screensaver at 14:45).
#define SCREENSAVER_LEAD_MS 15000
#define SCREENSAVER_DELAY_MS                                                                       \
    MAX(CONFIG_ZMK_IDLE_SLEEP_TIMEOUT - SCREENSAVER_LEAD_MS, CONFIG_ZMK_IDLE_SLEEP_TIMEOUT / 2)

static atomic_t active;

static bool usb_powered(void) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    return zmk_usb_is_powered();
#else
    return false;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
}

// Both work items run on the display work queue, so the redraws never race the other widgets.
static void show_work_cb(struct k_work *work) {
    // ZMK never sleeps while on USB power (see app/src/activity.c), so neither do we.
    if (usb_powered()) {
        return;
    }

    if (!atomic_set(&active, true)) {
        zmk_widget_screensaver_changed();
    }
}

static void hide_work_cb(struct k_work *work) {
    if (atomic_set(&active, false)) {
        zmk_widget_screensaver_changed();
    }
}

static K_WORK_DELAYABLE_DEFINE(show_work, show_work_cb);
static K_WORK_DEFINE(hide_work, hide_work_cb);

// Mirrors the events ZMK's activity tracker treats as activity, plus USB being plugged in.
static int screensaver_listener(const zmk_event_t *eh) {
    k_work_reschedule_for_queue(zmk_display_work_q(), &show_work, K_MSEC(SCREENSAVER_DELAY_MS));

    if (atomic_get(&active)) {
        k_work_submit_to_queue(zmk_display_work_q(), &hide_work);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_screensaver, screensaver_listener);
ZMK_SUBSCRIPTION(widget_screensaver, zmk_position_state_changed);
ZMK_SUBSCRIPTION(widget_screensaver, zmk_sensor_event);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_screensaver, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

void zmk_widget_screensaver_init(void) {
    k_work_reschedule_for_queue(zmk_display_work_q(), &show_work, K_MSEC(SCREENSAVER_DELAY_MS));
}

bool zmk_widget_screensaver_active(void) { return atomic_get(&active); }

#else

void zmk_widget_screensaver_init(void) {}

bool zmk_widget_screensaver_active(void) { return false; }

#endif /* IS_ENABLED(CONFIG_ZMK_SLEEP) */

void zmk_widget_screensaver_draw(lv_obj_t *canvas) {
    static lv_draw_label_dsc_t z_large, z_medium, z_small;
    static bool initialized;

    if (!initialized) {
        lv_draw_label_dsc_init(&z_large);
        z_large.color = lv_color_black();
        z_large.font = &lv_font_montserrat_22;

        lv_draw_label_dsc_init(&z_medium);
        z_medium.color = lv_color_black();
        z_medium.font = &lv_font_montserrat_18;

        lv_draw_label_dsc_init(&z_small);
        z_small.color = lv_color_black();
        z_small.font = &lv_font_montserrat_14;

        initialized = true;
    }

    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);

    // A rising, shrinking "Z z z".
    canvas_draw_text(canvas, 34, 72, 30, &z_large, "Z");
    canvas_draw_text(canvas, 58, 54, 30, &z_medium, "z");
    canvas_draw_text(canvas, 78, 40, 30, &z_small, "z");
}
