#include "fonctions.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"

// =====================================================
//  GLOBAL STATION LIST
// =====================================================

std::vector<Station> STATIONS;

// =====================================================
//  DEFAULT STATIONS (bundled, Thai)
//  Written to SD /radios.json on first boot; merged by URL
//  into existing cards so newly added defaults appear
//  without wiping user edits.
// =====================================================
const char* DEFAULT_RADIOS_JSON = R"([{"name":"coolism","url":"https://coolism-web3rd.cdn.byteark.com/;stream/1","logo":"S:/logos/def.bin"},{"name":"looktook_mahanakon","url":"https://play-fm95.mcot.net/fm95/fm95.m3u8","logo":"S:/logos/def.bin"},{"name":"greenwave","url":"https://atimeonline3.smartclick.co.th/green","logo":"S:/logos/def.bin"},{"name":"efm","url":"https://atimeonline3.smartclick.co.th/efm","logo":"S:/logos/def.bin"},{"name":"ingdoi","url":"https://radio1.ohmi-design.com/8170/;","logo":"S:/logos/def.bin"},{"name":"looktook_network","url":"https://cdn-th2.livestreaming.in.th/shoutcast/8200","logo":"S:/logos/def.bin"},{"name":"great93fm","url":"https://radio.vpsthai.net/proxy/qndrbfic/stream2","logo":"S:/logos/def.bin"},{"name":"request","url":"https://ozone2.javmemory.com/Music.mp3","logo":"S:/logos/def.bin"},{"name":"munforward","url":"https://cdn-th2.livestreaming.in.th/shoutcast/8795","logo":"S:/logos/def.bin"},{"name":"yesterdayradio","url":"http://radio4.thzhost.com:4122/;","logo":"S:/logos/def.bin"},{"name":"xfm","url":"https://radio12.plathong.net/7360/;stream.mp3","logo":"S:/logos/def.bin"},{"name":"thaimusic","url":"https://listen.thai-radio.net/trlq","logo":"S:/logos/def.bin"},{"name":"surfradio","url":"https://stream.surf.radio/surf1025","logo":"S:/logos/def.bin"},{"name":"sabuyradio","url":"https://radio9.plathong.net/7264/;stream.mp3","logo":"S:/logos/def.bin"},{"name":"hotwave","url":"https://atimeonline3.smartclick.co.th/hotwave","logo":"S:/logos/def.bin"},{"name":"radradio","url":"https://radradio895.com/stream.php","logo":"S:/logos/def.bin"},{"name":"passion","url":"https://media.onair.one:8170/DAB2","logo":"S:/logos/def.bin"},{"name":"dontreeseesan","url":"https://hp2.hostpleng.cloud/8203/stream","logo":"S:/logos/def.bin"},{"name":"nakonchiangmai","url":"https://radio11.plathong.net/7240/;stream.mp3","logo":"S:/logos/def.bin"},{"name":"musicstation","url":"https://hp2.hostpleng.cloud/8122/live","logo":"S:/logos/def.bin"},{"name":"goodmusic","url":"https://a.hostpleng.cloud/9114/live","logo":"S:/logos/def.bin"},{"name":"lovefm_chiangmia","url":"https://radio11.plathong.net/7142/;stream.mp3","logo":"S:/logos/def.bin"},{"name":"city_pattaya","url":"https://media.onair.one:8030/CTFLAC","logo":"S:/logos/def.bin"}])";

// Write current STATIONS to LittleFS /radios.json (internal flash — no SD needed)
static bool saveStationsToSD() {
    File file = LittleFS.open("/radios.json", FILE_WRITE);
    if (!file) {
        Serial.println("[FS] Cannot write radios.json");
        return false;
    }
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (auto& s : STATIONS) {
        JsonObject o = arr.add<JsonObject>();
        o["name"] = s.name;
        o["url"]  = s.url;
        o["logo"] = s.logo_path;
    }
    serializeJson(doc, file);
    file.close();
    return true;
}

// =====================================================
//  STATION LOADER
//
//  Reads /radios.json from the SD card and fills STATIONS.
//  Missing bundled defaults are appended (matched by URL)
//  so firmware updates add new stations to existing cards.
//  Falls back to coolism in RAM if the SD is unreadable.
// =====================================================
bool loadStationsFromSD() {
    STATIONS.clear();

    File file = LittleFS.open("/radios.json", FILE_READ);
    if (file) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        if (error) {
            Serial.printf("[FS] JSON parse error: %s\n", error.c_str());
        } else {
            for (JsonObject obj : doc.as<JsonArray>()) {
                Station s;
                s.name      = obj["name"].as<String>();
                s.url       = obj["url"].as<String>();
                s.logo_path = obj["logo"].as<String>();
                if (s.url.length()) STATIONS.push_back(s);
            }
        }
    } else {
        Serial.println("[FS] radios.json missing — will create from defaults");
    }

    // Merge bundled defaults (by URL)
    JsonDocument ddoc;
    int added = 0;
    if (!deserializeJson(ddoc, DEFAULT_RADIOS_JSON)) {
        for (JsonObject obj : ddoc.as<JsonArray>()) {
            String url = obj["url"].as<String>();
            bool found = false;
            for (auto& s : STATIONS) {
                if (s.url == url) { found = true; break; }
            }
            if (!found) {
                Station s;
                s.name      = obj["name"].as<String>();
                s.url       = url;
                s.logo_path = obj["logo"].as<String>();
                STATIONS.push_back(s);
                added++;
            }
        }
    }

    if (STATIONS.empty()) {
        // Flash unreadable — RAM-only fallback so radio UI still works
        Serial.println("[FS] No stations available — loading fallback station");
        Station s;
        s.name      = "coolism (fallback)";
        s.url       = "https://coolism-web3rd.cdn.byteark.com/;stream/1";
        s.logo_path = "S:/logos/def.bin";
        STATIONS.push_back(s);
        return false;
    }

    if (added > 0) {
        Serial.printf("[FS] %d new default station(s) merged\n", added);
        saveStationsToSD();
    }
    Serial.printf("[FS] %d station(s) loaded\n", STATIONS.size());
    return true;
}

// =====================================================
//  BATTERY LEVEL
//
//  Averages 10 ADC readings to reduce ESP32 ADC noise.
//  Raw range 1900..2400 mapped to 0..100%.
//  Adjust BAT_ADC_MIN / BAT_ADC_MAX in config.h to
//  calibrate for your specific battery/divider.
// =====================================================
int get_battery_level() {
    int32_t sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += analogRead(BAT_ADC_PIN);
        delay(2); // short delay to let the ADC capacitor recharge
    }
    int raw = (int)(sum / 10);
    return constrain(map(raw, 1900, 2400, 0, 100), 0, 100);
}
