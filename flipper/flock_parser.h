// Parser for the SwizFlockHunter UART protocol.
// One record per line, key=value pairs. Quoted SSIDs allowed.

#pragma once

#include "swiz_flock_hunter.h"

typedef enum {
    SwizMsgUnknown = 0,
    SwizMsgReady,
    SwizMsgStat,
    SwizMsgHide,
    SwizMsgHit,
} SwizMsgType;

typedef struct {
    SwizMsgType type;
    union {
        struct {
            uint8_t version;
        } ready;
        struct {
            uint32_t frames, mgmt, visible, hidden, flagged, hits;
            uint8_t  ch;
            bool     gps_ok;
        } stat;
        struct {
            char mac[18];
            char oui[9];
            int  rssi;
            int  ch;
        } hide;
        SwizHit hit;
    } body;
} SwizMsg;

bool flock_parse_line(const char* line, SwizMsg* out);
