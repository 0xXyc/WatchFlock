#include "flock_log.h"

#include <storage/storage.h>

#define LOG_DIR  "/ext/apps_data/swiz_flock_hunter"
#define LOG_PATH LOG_DIR "/SwizWiFiFlockHunter-hits.csv"

static const char* CSV_HEADER =
    "Swiz WiFi Flock Hunter - Detection Log,,,,,,,,,,,,,,\n"
    "fired_at_ms,mac,oui,vendor,rule,ssid,ssid_hidden,rssi,ch,conf,gps_ok,lat,lon,alt,fix_time\n";

static Storage* g_storage = NULL;
static File*    g_file    = NULL;
static bool     g_ready   = false;

static const char* conf_str(SwizConf c) {
    switch (c) {
    case SwizConfHigh:   return "HIGH";
    case SwizConfMedium: return "MEDIUM";
    case SwizConfLow:    return "LOW";
    default:             return "NONE";
    }
}

void flock_log_init(void) {
    g_storage = furi_record_open(RECORD_STORAGE);
    if(!g_storage) return;

    storage_simply_mkdir(g_storage, LOG_DIR);

    bool needs_header = !storage_file_exists(g_storage, LOG_PATH);

    g_file = storage_file_alloc(g_storage);
    if(!g_file) {
        furi_record_close(RECORD_STORAGE);
        g_storage = NULL;
        return;
    }

    // Persistent file handle for the whole app session — open once, write per
    // HIT, close on exit. Avoids open/close churn that previously saturated SD.
    if(!storage_file_open(g_file, LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_free(g_file);
        g_file = NULL;
        furi_record_close(RECORD_STORAGE);
        g_storage = NULL;
        return;
    }

    if(needs_header) {
        storage_file_write(g_file, CSV_HEADER, strlen(CSV_HEADER));
    }

    g_ready = true;
}

void flock_log_hit(const SwizHit* h) {
    if(!g_ready || !h) return;

    char row[320];
    int n = snprintf(row, sizeof(row),
        "%lu,%s,%s,\"%s\",%s,\"%s\",%d,%d,%d,%s,%d,%s,%s,%s,%s\n",
        (unsigned long)h->fired_at_tick,
        h->mac, h->oui, h->vendor, h->rule,
        h->ssid_hidden ? "" : h->ssid,
        h->ssid_hidden ? 1 : 0,
        h->rssi, h->ch, conf_str(h->conf),
        h->gps_ok ? 1 : 0,
        h->lat, h->lon, h->alt, h->time);
    if(n < 0) return;
    if((size_t)n >= sizeof(row)) n = sizeof(row) - 1;

    storage_file_write(g_file, row, (size_t)n);
}

void flock_log_deinit(void) {
    g_ready = false;
    if(g_file) {
        storage_file_close(g_file);
        storage_file_free(g_file);
        g_file = NULL;
    }
    if(g_storage) {
        furi_record_close(RECORD_STORAGE);
        g_storage = NULL;
    }
}
