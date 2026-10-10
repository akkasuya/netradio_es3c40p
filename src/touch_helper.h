#ifndef TOUCH_HELPER_H
#define TOUCH_HELPER_H

#include <Arduino.h>
#include <FT6336.h>
#include "config.h"

// =====================================================
//  CAPACITIVE TOUCH CONTROLLER — FT6336U (ES3C40P, 320x480)
//  Same FT6336 protocol/addr (0x38) as ES3C28P FT6336G.
//  Communicates via I2C (shared bus with DS3231/ES8311).
//  Landscape rotation (1) matches the TFT orientation.
// =====================================================

FT6336 ts = FT6336((int)I2C_SDA, (int)I2C_SCL,
                   (int)TP_INT, (int)TP_RST,
                   TP_W, TP_H);

void setup_touch() {
    ts.begin();
    ts.setRotation(1); // Landscape — matches TFT rotation
}

#endif
