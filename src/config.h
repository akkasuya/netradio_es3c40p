#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =====================================================
//  HARDWARE PIN CONFIGURATION — LCDWiki ES3C40P
//  (ported from ES3C28P/netradio: same pins except display
//  ST7796S 320x480 + I2S speaker data on GPIO6)
//  All GPIO assignments for the ESP32-S3 board.
//  Edit here only — never hard-code pins elsewhere.
//  Ref: https://www.lcdwiki.com/4.0inch_ESP32-S3_Display
// =====================================================

// --- TFT display (ST7796S, SPI, landscape 480x320) ---
#define TFT_W        480  // Width  in landscape mode (used by LVGL)
#define TFT_H        320  // Height in landscape mode
#define TFT_MOSI     11
#define TFT_SCLK     12
#define TFT_CS       10
#define TFT_DC       46
#define TFT_RST      -1   // Tied to global reset
#define TFT_MISO     13
#define TFT_BL       45   // Backlight PWM
#define TFT_BL_ON    HIGH

// --- Capacitive touch controller (FT6336U, 320x480, addr 0x38) ---
#define TP_INT       GPIO_NUM_17
#define TP_RST       GPIO_NUM_18
#define TP_W         320  // Panel width  in portrait (native)
#define TP_H         480  // Panel height in portrait (native)

// --- I2C bus (DS3231 RTC + ES8311 codec) ---
#define I2C_SDA      GPIO_NUM_16
#define I2C_SCL      GPIO_NUM_15

// --- SD card (SPI mode, dedicated HSPI bus) ---
// Same workaround as ES3C28P: upstream CYD-GOLD used SDMMC 4-bit,
// but the slot on these LCDWiki boards does not mount that way,
// so SD is wired SPI (cf. ../es3c28p/ehradio myoptions.h: SD on SPI bus B).
// TFT stays on FSPI (11/12/13), SD gets its own bus (HSPI) so pin mappings
// do not clash.
#define SD_SCK       38
#define SD_MISO      39
#define SD_MOSI      40
#define SD_CS        47

// --- Audio I2S (ES8311 codec) ---
// ES3C40P routes speaker data to IO6 per spec, but if there is no
// sound (reported), fall back to the ES3C28P value IO8.
#define I2S_BCK      GPIO_NUM_5
#define I2S_WS       GPIO_NUM_7
#define I2S_DOUT     GPIO_NUM_8
#define I2S_DIN      GPIO_NUM_6   // Mic data (unused by netradio)
#define I2S_MCK      GPIO_NUM_4
#define AMP_EN       GPIO_NUM_1  // LOW = amplifier active

// --- Battery ADC ---
#define BAT_ADC_PIN  9
#define BAT_ADC_MIN  1900   // Raw ADC value at 0%  — calibrate for your divider
#define BAT_ADC_MAX  2400   // Raw ADC value at 100%

// --- Deep-sleep slide switch ---
// Wired between GPIO and GND with INPUT_PULLUP.
// Switch OFF → GPIO HIGH → deep sleep.
// Switch ON  → GPIO LOW  → normal operation.
// Wake-up: EXT1 on LOW level (switch flipped back to ON).
// ES3C28P adaptation: bare board has NO slide switch — GPIO2 floats HIGH
// with INPUT_PULLUP, which would deep-sleep on every boot. Keep disabled
// (0) unless you wired the CYD-GOLD enclosure switch to GPIO2.
#define ENABLE_SLEEP_SWITCH 0
#define SLEEP_BTN_PIN    GPIO_NUM_2
#define SLEEP_BTN_LEVEL  LOW

#endif
