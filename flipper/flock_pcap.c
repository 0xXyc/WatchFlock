#include "flock_pcap.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <storage/storage.h>
#include <stdio.h>

#define PCAP_DIR "/ext/apps_data/swiz_flock_hunter"

static Storage* g_storage = NULL;
static File*    g_file    = NULL;
static bool     g_ready   = false;

// Per-session state for duplicate-global-header suppression. Marauder emits
// the 24-byte pcap global header on every pcapOpen(), but the Flipper FAP
// writes to one file across multiple sniffflockwifi invocations, so the dup
// headers stack inside the same file and break it. We keep the first header
// (block starts with magic d4c3b2a1) and drop the first 24 bytes of any
// later block that also starts with the magic.
static bool    g_global_header_written = false;
static size_t  g_block_offset          = 0;
static bool    g_block_is_dup_header   = false;
static uint8_t g_magic_check[4];

#define PCAP_MAGIC_0 0xd4
#define PCAP_MAGIC_1 0xc3
#define PCAP_MAGIC_2 0xb2
#define PCAP_MAGIC_3 0xa1
#define PCAP_GLOBAL_HEADER_LEN 24

void flock_pcap_init(void) {
    g_storage = furi_record_open(RECORD_STORAGE);
    if(!g_storage) return;

    storage_simply_mkdir(g_storage, PCAP_DIR);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char path[96];
    snprintf(path, sizeof(path),
        PCAP_DIR "/flockwifi-%04u%02u%02u-%02u%02u%02u.pcap",
        dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

    g_file = storage_file_alloc(g_storage);
    if(!g_file) {
        furi_record_close(RECORD_STORAGE);
        g_storage = NULL;
        return;
    }

    if(!storage_file_open(g_file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(g_file);
        g_file = NULL;
        furi_record_close(RECORD_STORAGE);
        g_storage = NULL;
        return;
    }

    g_ready = true;
}

void flock_pcap_block_begin(void) {
    g_block_offset        = 0;
    g_block_is_dup_header = false;
}

void flock_pcap_write(const uint8_t* data, size_t len) {
    if(!g_ready || !data || len == 0) return;

    for(size_t i = 0; i < len; i++) {
        uint8_t b = data[i];

        if(g_block_offset < 4) {
            // Buffer the first 4 bytes of each block so we can sniff for the
            // pcap magic before committing them. Once we know whether the
            // block is a duplicate global header, we either flush these 4
            // bytes to disk or drop them.
            g_magic_check[g_block_offset++] = b;
            if(g_block_offset == 4) {
                bool is_magic =
                    g_magic_check[0] == PCAP_MAGIC_0 &&
                    g_magic_check[1] == PCAP_MAGIC_1 &&
                    g_magic_check[2] == PCAP_MAGIC_2 &&
                    g_magic_check[3] == PCAP_MAGIC_3;
                if(is_magic && g_global_header_written) {
                    g_block_is_dup_header = true; // drop bytes 0..23 this block
                } else {
                    storage_file_write(g_file, g_magic_check, 4);
                    if(is_magic) g_global_header_written = true;
                }
            }
            continue;
        }

        // Past the magic-check prefix.
        if(g_block_is_dup_header && g_block_offset < PCAP_GLOBAL_HEADER_LEN) {
            g_block_offset++;
            continue; // still inside the duplicate global header — drop
        }
        storage_file_write(g_file, &b, 1);
        g_block_offset++;
    }
}

void flock_pcap_deinit(void) {
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
