#pragma once

#include "util.h"
#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_status {
    sys_snode_t node;
    lv_obj_t *obj;
    // LVGL 9 canvas buffers are raw bytes sized/aligned via LV_CANVAS_BUF_SIZE, not lv_color_t
    // arrays -- see CANVAS_BUF_SIZE / CANVAS_COLOR_FORMAT in util.h.
    uint8_t cbuf[CANVAS_BUF_SIZE];
    struct status_state state;
};

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget);