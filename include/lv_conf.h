/**
 * Minimal LVGL v8 configuration for the Crown Green Bowls Scoring App.
 *
 * Any setting not defined here falls back to the defaults built into
 * LVGL's lv_conf_internal.h, so only the options this app actually needs
 * to override are listed below.
 */

#if !defined(LV_CONF_H) || defined(LV_CONF_INCLUDE_SIMPLE)
#define LV_CONF_H

#include <stdint.h>

/* Color depth: 16 (RGB565) matches the AMOLED panel framebuffer format. */
#define LV_COLOR_DEPTH 16

/* Use the standard C library malloc/free/memset/etc. */
#define LV_MEM_CUSTOM 1

/* Use Arduino's millis() as the LVGL tick source. */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* Widgets used by the app's screens. */
#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 1
#define LV_USE_LIST 1
#define LV_USE_FLEX 1

/* Built-in default font used for labels/buttons. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Enable logging over Serial to help diagnose display/touch issues. */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#endif /* LV_CONF_H */
