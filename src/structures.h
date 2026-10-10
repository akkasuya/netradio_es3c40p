#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <Arduino.h>
#include <lvgl.h>
#include <vector>

// =====================================================
//  DATA STRUCTURES — CYD-GOLD
// =====================================================

// One saved WiFi network (stored in Config.known_networks)
struct WifiNetwork {
    char ssid[32];
    char pass[65];
};

// Config format marker. Bump when Config layout OR fresh-install defaults
// change; loadConfig() resets stale blobs instead of misreading them.
// NR03: default language English + default WiFi DRBOM (was NR02 French/empty).
#define NETRADIO_CONFIG_MAGIC 0x4E523033  // "NR03"

// Persistent configuration — saved to LittleFS as /config.bin
struct Config {
    uint32_t magic        = NETRADIO_CONFIG_MAGIC;
    int     brightness    = 80;    // Backlight 0..100%
    char    wifi_ssid[32];         // Last connected SSID
    char    wifi_pass[64];         // Last connected password
    WifiNetwork known_networks[10];// Up to 10 remembered networks
    int     networks_count  = 0;
    int     alarm_h         = 7;   // Alarm hour   (0..23)
    int     alarm_m         = 0;   // Alarm minute (0..59)
    bool    alarm_on        = false;
    int     alarm_mode      = 0;   // 0=Radio  1=MP3  2=Buzzer
    int     alarm_station   = 0;   // Station index for Radio alarm
    int     volsound        = 13;  // Audio volume, lib units 0..21 (~60%)
    int     eq_bass         = 0;   // Equalizer bass  (-10..+6 dB)
    int     eq_mid          = 0;   // Equalizer mid   (-10..+6 dB)
    int     eq_treble       = 0;   // Equalizer treble(-10..+6 dB)
    int     selected_theme  = 0;   // Index into listeThemes[]
    char    timezone[64];          // POSIX TZ string (e.g. "ICT-7")
    uint8_t language        = 0;   // LangID (see ui_lang.h)
};

// One radio station entry (loaded from /radios.json)
struct Station {
    String name;
    String url;
    String logo_path; // "S:/logos/xxx.bin"
};

// LVGL theme color palette — one instance per visual theme
struct RadioTheme {
    lv_color_t bg_color;  // Screen background
    lv_color_t primary;   // Accent color (buttons, highlights)
    lv_color_t text_main; // Primary text
    lv_color_t text_muted;// Secondary / disabled text
    lv_color_t btn_core;  // Button / list item background
    lv_color_t border;    // Borders and separators
};

// =====================================================
//  GLOBAL DECLARATIONS
// =====================================================

extern Config       userConfig;
extern RadioTheme*  listeThemes[];
extern const int    NOMBRE_THEMES;
extern RadioTheme*  currentTheme;
extern std::vector<Station> STATIONS;
extern lv_obj_t*    ui_wifi_list;

extern void saveConfig();
extern bool loadConfig();
extern void start_wifi_scan();
extern void start_wifi_connect();
void setDefaultConfig();

#endif
