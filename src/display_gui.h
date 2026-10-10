#ifndef DISPLAY_GUI_H
#define DISPLAY_GUI_H

#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <LittleFS.h>
#include "structures.h"
#include "config.h"
#include "fonctions.h"
#include "logo_def.h"
#include "buzzer_data.h"

TFT_eSPI tft = TFT_eSPI();
// Dedicated SPI bus for the SD slot (HSPI = SPI3); TFT_eSPI uses FSPI.
SPIClass SD_SPI(HSPI);


void setup_display() {
    tft.init();
    tft.setRotation(1); // Landscape: 480x320 on ST7796S 320x480
    tft.fillScreen(TFT_BLACK);
    // ES3C28P/ILI9341 used invertDisplay(true). ST7796S may want
    // false — if colours look inverted, flip this flag.
    tft.invertDisplay(true);
    
    // Enable backlight
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, map(userConfig.brightness, 0, 100, 0, 255));
}

void setup_sd_card() {
    // --- Internal flash first: radio + buzzer work with no SD card ---
    if (!LittleFS.exists("/radios.json")) {
        Serial.println("[FS] Creating default radios.json...");
        File file = LittleFS.open("/radios.json", FILE_WRITE);
        if (file) {
            file.print(DEFAULT_RADIOS_JSON);
            file.close();
            Serial.println("[FS] radios.json created with default stations.");
        }
    }
    if (!LittleFS.exists("/buzzer.mp3")) {
        Serial.println("[FS] Restoring buzzer.mp3 from PROGMEM...");
        File f_buzz = LittleFS.open("/buzzer.mp3", FILE_WRITE);
        if (f_buzz) {
            uint8_t buf[64];
            unsigned int count = 0;
            while (count < default_buzzer_len) {
                unsigned int chunk = default_buzzer_len - count;
                if (chunk > 64) chunk = 64;
                memcpy_P(buf, default_buzzer_mp3 + count, chunk);
                f_buzz.write(buf, chunk);
                count += chunk;
            }
            f_buzz.flush();
            f_buzz.close();
            delay(200);
            Serial.println("[FS] buzzer.mp3 installed.");
        } else {
            Serial.println("[FS] Failed to create buzzer.mp3");
        }
    }

    // --- SD card (optional): MP3 files + custom logos ---
    // SPI mode on a dedicated bus (HSPI/SPI3): TFT keeps FSPI pins 11/12/13.
    SD_SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    // Mount SD (mount point: /sd)
    if(!SD.begin(SD_CS, SD_SPI, 20000000)){
        Serial.println("[SD] No card — MP3/logos disabled (radio still works)");
        return;
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[SD] Card ready: %llu MB (SPI mode)\n", cardSize);

    Serial.println("[SD] Checking system folders...");

    // Ensure required folders exist
    const char* folders[] = {"/mp3", "/logos"};

    for(const char* folder : folders) {
        if (!SD.exists(folder)) {
            Serial.printf("[SD] Creating folder: %s\n", folder);
            SD.mkdir(folder);
        }
    }

    // Restore default logo from PROGMEM if missing
    if (!SD.exists("/logos/def.bin")) {
        Serial.println("[SD] Restoring def.bin from PROGMEM...");
        File f_log = SD.open("/logos/def.bin", FILE_WRITE);
        if (f_log) {
            uint8_t buf[64];
            unsigned int count = 0;

            while (count < def_bin_len) {
                unsigned int chunk = def_bin_len - count;
                if (chunk > 64) chunk = 64;

                // Safe copy from PROGMEM to RAM
                memcpy_P(buf, def_bin + count, chunk);
                // Write chunk to SD
                f_log.write(buf, chunk);

                count += chunk;
            }
            f_log.flush();
            f_log.close();
            delay(200);
            Serial.println("[SD] def.bin installed.");
        } else {
            Serial.println("[SD] Failed to create def.bin");
        }
    }

    Serial.println("[SD] Ready.");
}

// LVGL file system callbacks for SD card (driver letter S:)
static void * sd_fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    File * f = new File();
    *f = SD.open(path, mode == LV_FS_MODE_WR ? FILE_WRITE : FILE_READ);
    if(!*f || f->isDirectory()) {
        delete f;
        return NULL;
    }
    return (void *)f;
}

static lv_fs_res_t sd_fs_close(lv_fs_drv_t * drv, void * file_p) {
    File * f = (File *)file_p;
    f->close();
    delete f;
    return LV_FS_RES_OK;
}

static lv_fs_res_t sd_fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
    File * f = (File *)file_p;
    *br = f->read((uint8_t *)buf, btr);
    return LV_FS_RES_OK;
}

static lv_fs_res_t sd_fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
    File * f = (File *)file_p;
    f->seek(pos, whence == LV_FS_SEEK_SET ? SeekSet : (whence == LV_FS_SEEK_CUR ? SeekCur : SeekEnd));
    return LV_FS_RES_OK;
}

static lv_fs_res_t sd_fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
    File * f = (File *)file_p;
    *pos_p = f->position();
    return LV_FS_RES_OK;
}

// Register the SD LVGL file system driver
void lv_fs_sd_init() {
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);
    
    drv.letter = 'S'; // Use S:/ prefix for SD paths
    drv.ready_cb = NULL;
    drv.open_cb = sd_fs_open;
    drv.close_cb = sd_fs_close;
    drv.read_cb = sd_fs_read;
    drv.seek_cb = sd_fs_seek;
    drv.tell_cb = sd_fs_tell;
    
    lv_fs_drv_register(&drv);
}

#endif