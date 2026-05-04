// OUI -> vendor name lookup for the Flock-family prefixes the firmware
// flags. Mirrors the OUI lists in
// WatchFlock/esp32_marauder/WiFiScan.cpp.

#pragma once

#include <stddef.h>

// Writes the vendor name (or "unknown") into out. Always NUL-terminates.
void flock_oui_lookup(const char* oui, char* out, size_t out_sz);
