#ifndef FONCTIONS_H
#define FONCTIONS_H

#include <vector>
#include "structures.h"

// Global station list — populated by loadStationsFromSD()
extern std::vector<Station> STATIONS;

// Bundled default stations (JSON array) — shared with display_gui.h
extern const char* DEFAULT_RADIOS_JSON;

// Load /radios.json from LittleFS into STATIONS.
// Missing bundled defaults are merged in (by URL) and saved back.
// Falls back to a hardcoded coolism station if flash is unreadable.
bool loadStationsFromSD();

// Read battery level as a percentage (0..100).
// Averages 10 ADC samples to reduce ESP32 ADC noise.
int get_battery_level();

#endif
