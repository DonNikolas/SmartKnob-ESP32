#ifndef LV_CONF_H
#define LV_CONF_H

/* LVGL configuration prepared with ChatGPT assistance. */

/* RGB565 colours for the display driver. */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Internal memory reserved for LVGL objects and styles. */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64U * 1024U)

/* The test program will update the LVGL tick explicitly. */
#define LV_TICK_CUSTOM 0
#define LV_DISP_DEF_REFR_PERIOD 20

/* Built-in fonts for the first display test. */
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Keep diagnostic overlays disabled. */
#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#endif