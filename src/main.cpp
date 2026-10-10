// =====================================================
//  CYD-GOLD - Main sketch
//  ESP32-S3 Internet Radio / MP3 Player / Alarm Clock
//
//  Entry points:
//    setup() - hardware init, WiFi, LVGL, home screen
//    loop()  - audio, LVGL, WiFi watchdog, sleep
//
//  All UI screens are in ui_*.cpp files.
//  Config persisted in LittleFS /config.bin.
//  Stations loaded from SD /radios.json.
// =====================================================

#include <Arduino.h>
#include "LittleFS.h"
#include "FS.h"
#include "Audio.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <sys/time.h>
#include <lvgl.h>
#include <WebServer.h>
#include <Update.h>
#include "RTClib.h"

#include "config.h"
#include "codec_es8311.h"
#include "display_gui.h"
#include "touch_helper.h"
#include "ui_home.h"
#include "ui_radio.h"
#include "ui_mp3.h"
#include "structures.h"
#include "fonctions.h"
#include "web_server.h"
#include "ui_lang.h"
#include "thai_fonts.h"

Audio audio(true);
RTC_DS3231 rtc;
String current_title = "No playback";

extern lv_obj_t * now_playing_label;
extern lv_obj_t * ui_visualizer_bars[5];
extern bool is_mp3_mode; 
extern bool is_playing;
extern bool manual_pause;
extern void play_next_mp3();
extern void check_alarme(int rtc_h, int rtc_m);
extern void update_radio_stream_title(const char* title);

int currentStation = 0;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf = nullptr; // alloue en PSRAM dans setup()
extern bool buzzer_actif;

// True while WiFi screen is open  suspends auto-reconnect
bool wifi_ui_active = false;

// =====================================================
//  VEILLE PROFONDE (Deep Sleep)
// =====================================================
void enter_deep_sleep() {
    Serial.println("[SLEEP] Entering deep sleep");
    saveConfig();
    audio.stopSong();
    digitalWrite(AMP_EN, HIGH);      // coupe l'ampli
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    digitalWrite(TFT_BL, !TFT_BL_ON); // ecran eteint
    // No need to stop lvgl_tick_timer  deep sleep kills everything
    delay(200);
   // Configure pull-up (ESP-IDF API, compatible with ESP32 Arduino Core v3.x)
    // Keeps GPIO HIGH during sleep when switch is OFF
    gpio_set_direction((gpio_num_t)SLEEP_BTN_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en((gpio_num_t)SLEEP_BTN_PIN);
    gpio_pulldown_dis((gpio_num_t)SLEEP_BTN_PIN);

    // Hold GPIO state during deep sleep so pull-up stays active
    gpio_hold_en((gpio_num_t)SLEEP_BTN_PIN);

    // EXT1 wake-up: LOW level = switch flipped back to ON
    uint64_t pin_mask = 1ULL << SLEEP_BTN_PIN;
    esp_sleep_enable_ext1_wakeup(pin_mask, ESP_EXT1_WAKEUP_ALL_LOW);
    Serial.flush();
    esp_deep_sleep_start();
}

// LVGL tick  hardware timer at 1ms, independent of loop load
static esp_timer_handle_t lvgl_tick_timer = nullptr;
static void IRAM_ATTR lvgl_tick_cb(void*) { lv_tick_inc(1); }

lv_obj_t * status_label;
lv_obj_t * radio_img;
lv_obj_t * time_label;
lv_obj_t * date_label;
lv_obj_t * wifi_icon_label;
lv_obj_t * batt_icon_label;
lv_obj_t * ip_label;

// =====================================================
//  CONFIG PERSISTENCE (LittleFS /config.bin)
// =====================================================

Config userConfig; // Defaults from member initializers (see structures.h)

void saveConfig() {
    File f = LittleFS.open("/config.bin", "w");
    if(f) {
        f.write((const uint8_t*)&userConfig, sizeof(userConfig));
        f.close();
        Serial.println("[CONFIG] Saved");
    }
}

bool loadConfig() {
    if (LittleFS.exists("/config.bin")) {
        File f = LittleFS.open("/config.bin", "r");
        if (f) {
            size_t n = f.read((uint8_t*)&userConfig, sizeof(userConfig));
            f.close();
            if (n == sizeof(userConfig) && userConfig.magic == NETRADIO_CONFIG_MAGIC) {
                userConfig.volsound = constrain(userConfig.volsound, 0, 21);
                Serial.printf("[CONFIG] Loaded  brightness: %d%% vol: %d\n",
                              userConfig.brightness, userConfig.volsound);
                return true;
            }
            Serial.println("[CONFIG] Version/size mismatch — resetting to defaults");
        } else {
            // Fichier existe mais ne s'ouvre pas (FS corrompu ?)
            Serial.println("[CONFIG] File exists but cannot open (FS error?)");
        }
    } else {
        // Fichier absent  premier demarrage normal
        Serial.println("[CONFIG] Not found  using defaults");
    }
    setDefaultConfig();
    saveConfig();
    return false;
}

void setDefaultConfig() {
    userConfig.magic = NETRADIO_CONFIG_MAGIC;
    userConfig.brightness = 70;
    userConfig.volsound = 13; // ~60% (lib scale 0..21)
    userConfig.alarm_h = 7;
    userConfig.alarm_m = 0;
    userConfig.alarm_on = false;
    userConfig.alarm_mode = 0;
    userConfig.alarm_station = 0;
    userConfig.networks_count = 0;
    userConfig.eq_bass = 3;
    userConfig.eq_mid = 1;
    userConfig.eq_treble = 2;
    userConfig.selected_theme = 0;
    userConfig.language = 1; // LANG_EN (English) — default
    // ===== INITIALISATION DU FUSEAU HORAIRE PAR DEFAUT =====
    // On nettoie d'abord la memoire du tableau pour etre super propre
    memset(userConfig.timezone, 0, sizeof(userConfig.timezone));
    // Default timezone: Asia/Bangkok (ICT-7, no DST).
    // Changeable later in Settings > Pays. Upstream default was France.
    strncpy(userConfig.timezone, "ICT-7", sizeof(userConfig.timezone) - 1);
    // Default WiFi network (also seeded at compile time via WIFI_SEED_*).
    memset(userConfig.wifi_ssid, 0, sizeof(userConfig.wifi_ssid));
    memset(userConfig.wifi_pass, 0, sizeof(userConfig.wifi_pass));
    strncpy(userConfig.wifi_ssid, "DRBOM", sizeof(userConfig.wifi_ssid) - 1);
    strncpy(userConfig.wifi_pass, "55555555", sizeof(userConfig.wifi_pass) - 1);
    // Initialiser le tableau de reseaux connus
    for(int i = 0; i < 10; i++) {
        memset(userConfig.known_networks[i].ssid, 0, 32);
        memset(userConfig.known_networks[i].pass, 0, 64);
    }
    strncpy(userConfig.known_networks[0].ssid, "DRBOM", 31);
    strncpy(userConfig.known_networks[0].pass, "55555555", 63);
    userConfig.networks_count = 1;
    Serial.println("[CONFIG] Defaults applied");
}

// =====================================================
//  WIFI HELPERS
// =====================================================
void add_or_update_network(const char* ssid, const char* pass) {
    int index = -1;
    
    // Check if this SSID is already in the list
    for(int i = 0; i < userConfig.networks_count; i++) {
        if(strcmp(userConfig.known_networks[i].ssid, ssid) == 0) {
            index = i; // Trouve !
            break;
        }
    }

    // Not found and list has room  add it
    if(index == -1 && userConfig.networks_count < 5) {
        index = userConfig.networks_count;
        userConfig.networks_count++;
        Serial.println("[WIFI] New network saved");
    }

    // Write credentials to the slot
    if(index != -1) {
        strncpy(userConfig.known_networks[index].ssid, ssid, 31);
        userConfig.known_networks[index].ssid[31] = '\0';
        
        strncpy(userConfig.known_networks[index].pass, pass, 63);
        userConfig.known_networks[index].pass[63] = '\0';
        
        saveConfig(); 
        Serial.printf("[WIFI] Saved at slot %d\n", index);
    } else {
        Serial.println("[WIFI] Network list full (max 10)");
    }
}

void sync_rtc_from_ntp(); // forward declaration
void start_wifi_connect() {
    // Show connecting status on screen
    lv_obj_clean(ui_wifi_list);
    lv_list_add_text(ui_wifi_list, "Connecting...");
    lv_list_add_text(ui_wifi_list, userConfig.wifi_ssid);
    lv_timer_handler(); 

    Serial.printf("[WIFI] Trying: %s\n", userConfig.wifi_ssid);

    // Attempt connection
    WiFi.disconnect();
    WiFi.begin(userConfig.wifi_ssid, userConfig.wifi_pass);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        lv_timer_handler();
        delay(500);
        Serial.print(".");
        retry++;
    }

    // Handle result
    lv_obj_clean(ui_wifi_list);
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WIFI] Connected");
        lv_list_add_text(ui_wifi_list, "Connected!");

        sync_rtc_from_ntp(); // sync NTP  DS3231 (meme fonction que le boot)

        add_or_update_network(userConfig.wifi_ssid, userConfig.wifi_pass);

        lv_obj_t * btn = lv_list_add_btn(ui_wifi_list, LV_SYMBOL_OK, userConfig.wifi_ssid);
        lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    } 

    else {
        Serial.println("\n[WIFI] Failed");
        lv_list_add_text(ui_wifi_list, "Connection failed");
        lv_list_add_text(ui_wifi_list, "Check your password");
    }
}

// =====================================================
//  AUDIO LIBRARY CALLBACKS
// =====================================================
void audio_showstreamtitle(const char *info) {
    Serial.printf("[RADIO] Stream title : %s\n", info);
    current_title = "[RADIO] " + String(info);
    update_radio_stream_title(info);
}

void audio_showstation(const char *info) {
    Serial.printf("[RADIO] Station : %s\n", info);
}

// Stream error  called by ESP32-audioI2S on connection failure
void audio_error_mp3(const char *info) {
    Serial.printf("[RADIO] Erreur flux : %s\n", info);
    // Mark as stopped so the watchdog does not retry on a dead stream
    is_playing = false;
    current_title = String(lang->err_stream) + " : " + String(info);
    update_radio_stream_title(current_title.c_str());
}

// Stream connected  logged for debug
void audio_connected(const char *info) {
    Serial.printf("[RADIO] Connecte : %s\n", info);
}
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    ts.read();
    if (ts.isTouched) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = ts.points[0].x;
        data->point.y = ts.points[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// Indique si le RTC DS3231 a ete trouve au boot (evite d'interroger un RTC absent)
static bool rtc_ok = false;

// Applique le fuseau horaire POSIX  appele une seule fois au boot et si l'user
// change le fuseau dans les reglages. Jamais dans le loop.
void apply_timezone() {
    setenv("TZ", userConfig.timezone, 1);
    tzset();
    Serial.printf("[TZ] Applied: %s\n", userConfig.timezone);
}

// Primary time source: HTTP unixtime server. NTP is backup only.
// Appele au boot apres connexion WiFi, et periodiquement (1x/jour).
#define TIME_HTTP_URL "http://akkasua.3bbddns.com:57862/unixtime.php"

// Fetch unix time (UTC seconds) from the HTTP server and set system clock.
// Returns true on success.
static bool sync_time_from_http() {
    if (WiFi.status() != WL_CONNECTED) return false;
    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(TIME_HTTP_URL)) {
        Serial.println("[TIME] HTTP begin failed");
        return false;
    }
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[TIME] HTTP GET failed: %d\n", code);
        http.end();
        return false;
    }
    String body = http.getString();
    http.end();
    body.trim();
    if (body.length() == 0 || body.length() > 11) {
        Serial.printf("[TIME] Bad body: %s\n", body.c_str());
        return false;
    }
    for (size_t i = 0; i < body.length(); i++) {
        if (!isdigit((unsigned char)body[i])) {
            Serial.printf("[TIME] Non-numeric body: %s\n", body.c_str());
            return false;
        }
    }
    long long t = strtoll(body.c_str(), NULL, 10);
    // Sanity window: 2024-01-01 .. 2035-01-01
    if (t < 1704067200LL || t > 2051222400LL) {
        Serial.printf("[TIME] Out of range: %lld\n", t);
        return false;
    }
    struct timeval tv = { .tv_sec = (time_t)t, .tv_usec = 0 };
    if (settimeofday(&tv, NULL) != 0) {
        Serial.println("[TIME] settimeofday failed");
        return false;
    }
    Serial.printf("[TIME] System time set from HTTP: %lld\n", t);
    return true;
}

// NTP fallback (backup only).
static bool sync_time_from_ntp() {
    configTzTime(userConfig.timezone, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 10) {
        delay(500);
        retry++;
    }
    return (retry < 10);
}

void sync_rtc_from_ntp() {
    if (WiFi.status() != WL_CONNECTED) return;

    bool ok = sync_time_from_http();
    if (!ok) {
        Serial.println("[TIME] HTTP failed, trying NTP backup...");
        ok = sync_time_from_ntp();
    }
    if (ok) {
        if (rtc_ok) {
            time_t now_utc;
            time(&now_utc);
            struct tm utc_tm;
            gmtime_r(&now_utc, &utc_tm);
            rtc.adjust(DateTime(utc_tm.tm_year + 1900, utc_tm.tm_mon + 1, utc_tm.tm_mday,
                                utc_tm.tm_hour, utc_tm.tm_min, utc_tm.tm_sec));
            Serial.println("[TIME] DS3231 synced (UTC)");
        } else {
            Serial.println("[TIME] System time synced (no DS3231)");
        }
    } else {
        Serial.println("[TIME] HTTP+NTP failed  clock keeps its time");
    }
}

// --- fonction mise a jour donnees ---
void update_time(bool force) {
    static uint32_t last_check    = 0;
    static uint32_t last_ntp_sync = 0; 

    if (!force && (millis() - last_check < 4000)) return;
    last_check = millis();

    // ===== BUZZER DE SECOURS =====
    if (buzzer_actif && !audio.isRunning()) {
        Serial.println("[Securite] Relance du buzzer...");
        audio.connecttoFS(LittleFS, "/buzzer.mp3");
    }

    // ===== WATCHDOG FLUX RADIO =====
    // Si on est en mode radio, que is_playing=true mais que l'audio
    // ne tourne plus (WiFi coupe, flux mort, timeout)  tentative de
    // reconnexion automatique toutes les 10s maximum.
    // Evite le crash silencieux quand la connexion 5G du telephone flanche.
    static uint32_t last_radio_retry = 0;
    if (!is_mp3_mode && is_playing && !buzzer_actif && !audio.isRunning()) {
        if (millis() - last_radio_retry > 10000) {
            last_radio_retry = millis();
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[RADIO] Flux perdu, reconnexion station %d...\n", currentStation);
                current_title = "Reconnexion...";
                update_radio_stream_title("Reconnexion...");
                audio.connecttohost(STATIONS[currentStation].url.c_str());
            } else {
                // No WiFi  stop cleanly without infinite retry
                Serial.println("[RADIO] Flux perdu + pas de WiFi, arret.");
                is_playing = false;
                current_title = String(lang->wifi_lost);
                update_radio_stream_title(lang->wifi_lost);
            }
        }
    }

    // ===== RESYNC NTP QUOTIDIENNE (toutes les 24h si WiFi dispo) =====
    if (WiFi.status() == WL_CONNECTED &&
        (last_ntp_sync == 0 || millis() - last_ntp_sync > 86400000UL)) {
        sync_rtc_from_ntp();
        last_ntp_sync = millis();
    }

    // ===== LECTURE HEURE =====
    // DS3231 si présent, sinon heure système (NTP via configTzTime).
    // Sans RTC ni WiFi, l'heure système n'est pas valable → "--:--".
    struct tm local_tm;
    memset(&local_tm, 0, sizeof(local_tm));
    bool have_time = false;

    if (rtc_ok) {
        DateTime now = rtc.now();
        // Sanite basique : l'annee doit etre plausible (DS3231 non initialise = 2000)
        if (now.year() >= 2020) {
            // Conversion UTC -> heure locale (fuseau deja applique une fois au boot)
            time_t utc_epoch = now.unixtime();
            localtime_r(&utc_epoch, &local_tm);
            have_time = true;
        } else {
            Serial.println("[RTC] Not initialized (year < 2020)  waiting for NTP sync");
        }
    } else {
        // Pas de DS3231 : utilise l'heure système (synchronisée par NTP si WiFi).
        // getLocalTime applique le fuseau TZ déjà configuré.
        if (getLocalTime(&local_tm, 50) && (local_tm.tm_year + 1900) >= 2020) {
            have_time = true;
        }
    }

    if (!have_time) {
        if (time_label != NULL && lv_obj_is_valid(time_label))
            lv_label_set_text(time_label, "--:--");
        if (ip_label != NULL && lv_obj_is_valid(ip_label)) {
            if (WiFi.status() == WL_CONNECTED)
                lv_label_set_text(ip_label, WiFi.localIP().toString().c_str());
            else
                lv_label_set_text(ip_label, "--");
        }
        return;
    }

    int heures       = local_tm.tm_hour;
    int minutes      = local_tm.tm_min;
    int jour_semaine = local_tm.tm_wday;
    int jour_mois    = local_tm.tm_mday;
    int num_mois     = local_tm.tm_mon + 1;

    // ===== AFFICHAGE HEURE =====
    if (time_label != NULL && lv_obj_is_valid(time_label))
        lv_label_set_text_fmt(time_label, "%02d:%02d", heures, minutes);

    check_alarme(heures, minutes);

    // ===== AFFICHAGE DATE =====
    if (date_label != NULL && lv_obj_is_valid(date_label)) {
        int im = constrain(num_mois - 1, 0, 11);
        int ij = constrain(jour_semaine, 0, 6);
        lv_label_set_text_fmt(date_label, "%s %d %s",
            lang->days[ij], jour_mois, lang->months[im]);
    }

    // ===== ICNE WIFI =====
    if (wifi_icon_label != NULL && lv_obj_is_valid(wifi_icon_label)) {
        if (WiFi.status() == WL_CONNECTED) {
            lv_label_set_text(wifi_icon_label, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(wifi_icon_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else {
            lv_label_set_text(wifi_icon_label, LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_color(wifi_icon_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }

    // ===== ICNE BATTERIE =====
    if (batt_icon_label != NULL && lv_obj_is_valid(batt_icon_label)) {
        int bat = get_battery_level();
        if      (bat > 80) lv_label_set_text(batt_icon_label, LV_SYMBOL_BATTERY_FULL);
        else if (bat > 50) lv_label_set_text(batt_icon_label, LV_SYMBOL_BATTERY_3);
        else if (bat > 20) lv_label_set_text(batt_icon_label, LV_SYMBOL_BATTERY_1);
        else               lv_label_set_text(batt_icon_label, LV_SYMBOL_BATTERY_EMPTY);
        lv_obj_set_style_text_color(batt_icon_label,
            bat < 20 ? lv_palette_main(LV_PALETTE_RED) : currentTheme->text_muted, 0);
    }

    // ===== IP ADDRESS (right after battery) =====
    if (ip_label != NULL && lv_obj_is_valid(ip_label)) {
        if (WiFi.status() == WL_CONNECTED) {
            lv_label_set_text(ip_label, WiFi.localIP().toString().c_str());
        } else {
            lv_label_set_text(ip_label, "--");
        }
    }
}

void audio_eof_mp3(const char *info) {
    Serial.printf("[MP3] Track ended: %s\n", info);
    if (buzzer_actif && strcmp(info, "/buzzer.mp3") == 0) {
        Serial.println("[ALARM] Buzzer looping");
        audio.connecttoFS(LittleFS, "/buzzer.mp3"); 
    }
}

void setup() {
#if ENABLE_SLEEP_SWITCH
    gpio_hold_dis((gpio_num_t)SLEEP_BTN_PIN);
#endif
    Serial.begin(115200);
    pinMode(BAT_ADC_PIN, INPUT);

    setDefaultConfig();

    // --- Hardware and file system init ---
    setup_display();
    show_boot_msg("Init...");
    if (setup_i2c_and_codec() == ESP_OK) {
        Serial.println("[CODEC] OK");
        codec_volume(90); // analog stage ~-13dB: pairs with digital default 13
    } else {
        Serial.println("[CODEC] ERROR");
    }

    pinMode(AMP_EN, OUTPUT);
    digitalWrite(AMP_EN, HIGH);

    // Deep-sleep switch (only if wired, see ENABLE_SLEEP_SWITCH in config.h)
#if ENABLE_SLEEP_SWITCH
    pinMode(SLEEP_BTN_PIN, INPUT_PULLUP);
#endif
    setup_touch();

    if(!LittleFS.begin(true)){ 
        Serial.println("[FS] LittleFS mount failed");
    } else {
        // First boot or formatted FS  write defaults and restart
            if (!LittleFS.exists("/config.bin")) {
                Serial.println("[FS] First boot  writing defaults");
                setDefaultConfig();
                saveConfig();
                ESP.restart(); 
                }
    loadConfig();
    }
    

    // Apply saved brightness immediately
    analogWrite(TFT_BL, map(userConfig.brightness, 0, 100, 0, 255));
    apply_language(userConfig.language);

    show_boot_msg("SD...");
    setup_sd_card();
    loadStationsFromSD();

    // --- RTC DS3231 ---
    rtc_ok = rtc.begin();
    if (!rtc_ok) {
        Serial.println("[RTC] DS3231 not found  clock disabled");
    } else {
        Serial.println("[RTC] DS3231 found");
    }

    // Fuseau horaire applique
    apply_timezone();

    // Compile-time WiFi seed (WIFI_SEED_SSID/PASS from platformio.ini).
    // Applied once when no network is stored yet.
#ifdef WIFI_SEED_SSID
    if (userConfig.networks_count == 0 && strlen(userConfig.wifi_ssid) == 0) {
        strncpy(userConfig.wifi_ssid, WIFI_SEED_SSID, sizeof(userConfig.wifi_ssid) - 1);
        userConfig.wifi_ssid[sizeof(userConfig.wifi_ssid) - 1] = '\0';
#ifdef WIFI_SEED_PASS
        strncpy(userConfig.wifi_pass, WIFI_SEED_PASS, sizeof(userConfig.wifi_pass) - 1);
        userConfig.wifi_pass[sizeof(userConfig.wifi_pass) - 1] = '\0';
#endif
        add_or_update_network(userConfig.wifi_ssid, userConfig.wifi_pass);
        Serial.printf("[WIFI] Seeded compile-time network: %s\n", userConfig.wifi_ssid);
    }
#endif

    // --- WiFi AUTOMATIQUE ---
    show_boot_msg("WiFi...");
    if (strlen(userConfig.wifi_ssid) > 0) {
        Serial.printf("[WIFI] Auto-connect: %s\n", userConfig.wifi_ssid);
        WiFi.disconnect();
        WiFi.begin(userConfig.wifi_ssid, userConfig.wifi_pass);

        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 20) {
            delay(500);
            Serial.print(".");
            retry++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[WIFI] Connected");
            Serial.println(WiFi.localIP());
            sync_rtc_from_ntp(); // sync NTP  DS3231 (fonction commune)
        } else {
            Serial.println("\n[WIFI] Failed  RTC keeps its time");
        }
    } else {
        Serial.println("[WIFI] No saved network  offline mode");
    }

    if (WiFi.status() == WL_CONNECTED) {
        setup_web_server();
    }

    show_boot_msg("UI...");
    lv_init();

    // Hardware timer  1ms LVGL tick (independent of loop)
    const esp_timer_create_args_t tick_args = {
        .callback        = lvgl_tick_cb,
        .arg             = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "lvgl_tick"
    };
    esp_timer_create(&tick_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 1000); // 1000 s = 1 ms
    Serial.println("[LVGL] 1ms hardware tick started");
    
    // --- Configuration Audio ---
    audio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCK);
    audio.setVolume(userConfig.volsound);
    audio.setTone(userConfig.eq_bass, userConfig.eq_mid, userConfig.eq_treble);

    // --- LVGL display and touch drivers ---
    // Landscape framebuffer: TFT_W (480) pixels per row, 40 rows in PSRAM.
    buf = (lv_color_t*) heap_caps_malloc(TFT_W * 40 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!buf) {
        Serial.println("[LVGL] PSRAM unavailable  fallback to SRAM");
        buf = (lv_color_t*) heap_caps_malloc(TFT_W * 10 * sizeof(lv_color_t), MALLOC_CAP_DEFAULT);
    } else {
        Serial.println("[LVGL] Buffer allocated in PSRAM (40 rows)");
    }
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, TFT_W * 40);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_W;
    disp_drv.ver_res = TFT_H;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);
    lv_fs_sd_init();

    // --- Apply theme and launch home screen ---
    if (userConfig.selected_theme < 0 || userConfig.selected_theme >= NOMBRE_THEMES) {
        userConfig.selected_theme = 0; 
    }
    currentTheme = listeThemes[userConfig.selected_theme];
    Serial.print("[THEME] Applied: ");
    Serial.println(userConfig.selected_theme);
    digitalWrite(AMP_EN, LOW);
    setup_home_screen();
    lv_timer_handler();
}

void loop() {
    audio.loop();
    lv_timer_handler();
    
    // ===== RECONNEXION WIFI AUTOMATIQUE =====
    static uint32_t last_wifi_check = 0;
    static bool     was_connected   = false;
    if (millis() - last_wifi_check > 30000) {
        last_wifi_check = millis();
        if (WiFi.status() != WL_CONNECTED) {
            if (was_connected) {
                // On vient de perdre la connexion  arret propre du flux radio
                Serial.println("[WIFI] Lost  stopping radio stream");
                if (!is_mp3_mode && is_playing) {
                    audio.stopSong();
                    current_title = String(lang->wifi_lost);
                }
                was_connected = false;
            }
            // Try all known networks in order
            bool reconnected = false;
            for (int i = 0; i < userConfig.networks_count && !reconnected; i++) {
                if (strlen(userConfig.known_networks[i].ssid) == 0) continue;
                Serial.printf("[WIFI] Retrying: %s\n", userConfig.known_networks[i].ssid);
                WiFi.disconnect();
                WiFi.begin(userConfig.known_networks[i].ssid,
                           userConfig.known_networks[i].pass);
                int retry = 0;
                while (WiFi.status() != WL_CONNECTED && retry < 10) {
                    delay(500);
                    retry++;
                }
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.printf("[WIFI] Reconnected: %s\n", userConfig.known_networks[i].ssid);
                    // Update last-used network
                    strncpy(userConfig.wifi_ssid, userConfig.known_networks[i].ssid, 31);
                    strncpy(userConfig.wifi_pass, userConfig.known_networks[i].pass, 63);
                    sync_rtc_from_ntp();
                    reconnected  = true;
                    was_connected = true;
                }
            }
            if (!reconnected) {
                Serial.println("[WIFI] No known network available");
            }
        } else {
            was_connected = true;
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();
    }

    // ===== INTERRUPTEUR VEILLE PROFONDE (glissiere, optionnel) =====
#if ENABLE_SLEEP_SWITCH
    {
        static uint32_t last_sleep_check = 0;
        if (millis() - last_sleep_check > 200) {
            last_sleep_check = millis();
            if (digitalRead(SLEEP_BTN_PIN) == HIGH) {
                enter_deep_sleep(); // ne revient jamais ici
            }
        }
    }
#endif

    // Update now-playing label on home screen
    static uint32_t last_ui_update = 0;
    static String last_title = "";
    if (millis() - last_ui_update > 2000) {
        last_ui_update = millis();
        if (now_playing_label != NULL && current_title != last_title) {
            lv_label_set_text(now_playing_label, current_title.c_str());
            last_title = current_title;
        }
        update_time(false);
    }

    // Auto-advance to next track when MP3 ends
    if (is_playing && !manual_pause) {
        if (!audio.isRunning()) {
            // MP3 mode  advance to next track
            if (is_mp3_mode) {
                Serial.println("[MP3] Track ended  next");
                play_next_mp3();
            } else {
                // Radio stopped  handled by watchdog in update_time()
            }
        }
    }
  
     // Audio visualizer bars animation
    static uint32_t last_anim = 0;
    static bool bars_reset = false;
    if (is_playing && !manual_pause) {
        bars_reset = false;
        if (millis() - last_anim > 120) { 
            last_anim = millis();
            for (int i = 0; i < 5; i++) {
                if (ui_visualizer_bars != NULL && ui_visualizer_bars[i] != NULL) {
                    int h = random(5, 35);
                    lv_obj_set_height(ui_visualizer_bars[i], h);
                    lv_obj_set_y(ui_visualizer_bars[i], 40 - h); 
                }
            }
        }
    } else {

    // Reset bars to minimum when stopped (once)
        if (!bars_reset) {
            for (int i = 0; i < 5; i++) {
                if (ui_visualizer_bars != NULL && ui_visualizer_bars[i] != NULL) {
                    lv_obj_set_height(ui_visualizer_bars[i], 5);
                    lv_obj_set_y(ui_visualizer_bars[i], 35);
                }
            }
            bars_reset = true;
        }
    }
}