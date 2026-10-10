// lv_conf.h — minimal LVGL v8 config for netradio (ES3C40P, 480x320 ST7796S)
// Picked up via -DLV_CONF_INCLUDE_SIMPLE. Everything not defined here
// falls back to LVGL defaults in lv_conf_internal.h.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_HOR_RES_MAX 480
#define LV_VER_RES_MAX 320

// Heap for widgets/styles (draw buffer itself is PSRAM-allocated in main.cpp)
#define LV_MEM_SIZE (64U * 1024U)

#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM 0
#define LV_USE_LOG 0

// Fonts used across ui_*.cpp (Montserrat 10/12/14/18/20/24/32)
#define LV_FONT_MONTSERRAT_8 0
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_34 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_38 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_42 1
#define LV_FONT_MONTSERRAT_44 1
#define LV_FONT_MONTSERRAT_46 1
#define LV_FONT_MONTSERRAT_48 1

#endif // LV_CONF_H
