#ifndef THAI_FONTS_H
#define THAI_FONTS_H

// UI fonts: Noto Sans Thai + ASCII + all 31 FontAwesome symbols used
// by the app. Generated with lv_font_conv (bpp 4); symbols merged in.
//
// NOTE: ui_font() always returns the Thai-capable font, even for
// Latin languages. Reason: LVGL's built-in Montserrat only ships
// 3 of the 31 icons this UI uses (wifi/battery/audio/etc. would be
// blank). The Noto-based font is a superset, so every language
// renders text + icons correctly with identical layout metrics.

#include <lvgl.h>

LV_FONT_DECLARE(thai_10);
LV_FONT_DECLARE(thai_12);
LV_FONT_DECLARE(thai_14);
LV_FONT_DECLARE(thai_16);
LV_FONT_DECLARE(thai_18);
LV_FONT_DECLARE(thai_20);
LV_FONT_DECLARE(thai_24);
LV_FONT_DECLARE(thai_26);
LV_FONT_DECLARE(thai_32);

static inline const lv_font_t* ui_font(int size) {
    switch (size) {
        case 10: return &thai_10;
        case 12: return &thai_12;
        case 14: return &thai_14;
        case 16: return &thai_16;
        case 18: return &thai_18;
        case 20: return &thai_20;
        case 24: return &thai_24;
        case 26: return &thai_26;
        case 32: return &thai_32;
        default: return &thai_14;
    }
}

#endif
