#include "flock_log.h"

#include <storage/storage.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <stdio.h>
#include <string.h>

#define LOG_DIR  "/ext/apps_data/swiz_flock_hunter"
#define LOG_PATH LOG_DIR "/SwizWiFiFlockHunter-hits.csv"

static const char* CSV_HEADER =
    "Swiz WiFi Flock Hunter - Detection Log,,,,,,,,,,,,,,,\n"
    "fired_at_ms,mac,oui,vendor,rule,ssid,ssid_hidden,rssi,ch,conf,gps_ok,lat,lon,alt,fix_time,flipper_time\n";

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

// Build an ISO-8601-ish stamp from the Flipper's RTC. Format chosen so it
// sorts lexicographically and is unambiguous in spreadsheets. If the user
// hasn't set their RTC, year will be the Flipper's epoch default — still
// useful for relative ordering within a session.
static void format_now_iso(char* out, size_t out_sz) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    // %04u for uint16 year could theoretically produce 5 chars (e.g. 65535)
    // — GCC's -Wformat-truncation flags 24-byte buffers as too tight. 32 is
    // plenty.
    snprintf(out, out_sz, "%04u-%02u-%02uT%02u:%02u:%02u",
        dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
}

void flock_log_init(void) {
    g_storage = furi_record_open(RECORD_STORAGE);
    if(!g_storage) return;

    storage_simply_mkdir(g_storage, LOG_DIR);

    bool needs_fresh_file = true;

    // Schema-aware migration: if the existing CSV has flipper_time in its
    // header, append to it; otherwise archive it under a timestamped name
    // and start a clean file on the new schema. One-time on schema bump.
    if(storage_file_exists(g_storage, LOG_PATH)) {
        File* probe = storage_file_alloc(g_storage);
        bool has_new_schema = false;
        if(probe && storage_file_open(probe, LOG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            char head[256];
            uint16_t n = storage_file_read(probe, head, sizeof(head) - 1);
            head[n < sizeof(head) - 1 ? n : sizeof(head) - 1] = '\0';
            if(strstr(head, "flipper_time") != NULL) has_new_schema = true;
            storage_file_close(probe);
        }
        if(probe) storage_file_free(probe);

        if(has_new_schema) {
            needs_fresh_file = false;
        } else {
            char ts[32];
            format_now_iso(ts, sizeof(ts));
            char archive_path[160];
            snprintf(archive_path, sizeof(archive_path),
                LOG_DIR "/SwizWiFiFlockHunter-hits-archive-%s.csv", ts);
            // best-effort: if rename fails, fall through and we'll append-with-
            // mixed-schema, which is uglier than a clean archive but not fatal.
            storage_common_rename(g_storage, LOG_PATH, archive_path);
        }
    }

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

    if(needs_fresh_file) {
        storage_file_write(g_file, CSV_HEADER, strlen(CSV_HEADER));
    }

    g_ready = true;
}

void flock_log_hit(const SwizHit* h) {
    if(!g_ready || !h) return;

    char ts[32];
    format_now_iso(ts, sizeof(ts));

    char row[400];
    int n = snprintf(row, sizeof(row),
        "%lu,%s,%s,\"%s\",%s,\"%s\",%d,%d,%d,%s,%d,%s,%s,%s,%s,%s\n",
        (unsigned long)h->fired_at_tick,
        h->mac, h->oui, h->vendor, h->rule,
        h->ssid_hidden ? "" : h->ssid,
        h->ssid_hidden ? 1 : 0,
        h->rssi, h->ch, conf_str(h->conf),
        h->gps_ok ? 1 : 0,
        h->lat, h->lon, h->alt, h->time,
        ts);
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
