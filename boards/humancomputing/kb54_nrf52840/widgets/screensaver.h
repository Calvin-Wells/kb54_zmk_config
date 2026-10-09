#pragma once

#include <lvgl.h>
#include <stdbool.h>

// "Z z z" screensaver shown shortly before the half goes into deep sleep. Any key press (or USB
// being plugged in) hides it again.

// Starts the idle countdown. Call from the status widget's init, once the display is running.
void zmk_widget_screensaver_init(void);

bool zmk_widget_screensaver_active(void);

// Clears the canvas and draws the screensaver. The caller still rotates the canvas.
void zmk_widget_screensaver_draw(lv_obj_t *canvas);

// Implemented by the status widget: called on the display work queue whenever the screensaver
// turns on or off, so it can redraw.
void zmk_widget_screensaver_changed(void);
