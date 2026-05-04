#include "flock_parser.h"
#include "flock_oui.h"

#include <stdlib.h>
#include <string.h>

static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

// Find key= as a word and copy the value into out.
// Value goes to next whitespace, or to closing quote if value starts with ".
static bool extract_kv(const char* line, const char* key, char* out, size_t out_sz) {
    if (out_sz == 0) return false;
    out[0] = '\0';

    size_t klen = strlen(key);
    const char* p = line;
    while ((p = strstr(p, key)) != NULL) {
        // Word boundary check: previous char must be space, tab, or start
        if (p > line) {
            char prev = p[-1];
            if (prev != ' ' && prev != '\t') {
                p++;
                continue;
            }
        }
        if (p[klen] != '=') {
            p++;
            continue;
        }
        const char* v = p + klen + 1;
        if (*v == '"') {
            v++;
            const char* end = strchr(v, '"');
            if (!end) return false;
            size_t n = (size_t)(end - v);
            if (n >= out_sz) n = out_sz - 1;
            memcpy(out, v, n);
            out[n] = '\0';
            return true;
        }
        const char* end = v;
        while (*end && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') end++;
        size_t n = (size_t)(end - v);
        if (n >= out_sz) n = out_sz - 1;
        memcpy(out, v, n);
        out[n] = '\0';
        return true;
    }
    return false;
}

static int extract_int(const char* line, const char* key, int dflt) {
    char buf[16];
    if (!extract_kv(line, key, buf, sizeof(buf))) return dflt;
    return atoi(buf);
}

static SwizConf parse_conf(const char* s) {
    if (strcmp(s, "HIGH") == 0)   return SwizConfHigh;
    if (strcmp(s, "MEDIUM") == 0) return SwizConfMedium;
    if (strcmp(s, "LOW") == 0)    return SwizConfLow;
    return SwizConfNone;
}

bool flock_parse_line(const char* line, SwizMsg* out) {
    line = skip_ws(line);
    memset(out, 0, sizeof(*out));

    if (strncmp(line, "SWIZ ready", 10) == 0) {
        out->type = SwizMsgReady;
        out->body.ready.version = (uint8_t)extract_int(line, "proto", 1);
        return true;
    }
    if (strncmp(line, "STAT ", 5) == 0) {
        out->type = SwizMsgStat;
        out->body.stat.frames  = (uint32_t)extract_int(line, "frames",  0);
        out->body.stat.mgmt    = (uint32_t)extract_int(line, "mgmt",    0);
        out->body.stat.visible = (uint32_t)extract_int(line, "visible", 0);
        out->body.stat.hidden  = (uint32_t)extract_int(line, "hidden",  0);
        out->body.stat.flagged = (uint32_t)extract_int(line, "flagged", 0);
        out->body.stat.hits    = (uint32_t)extract_int(line, "hits",    0);
        out->body.stat.ch      = (uint8_t)extract_int(line, "ch", 0);
        char gps_state[8] = "";
        extract_kv(line, "gps", gps_state, sizeof(gps_state));
        out->body.stat.gps_ok  = (strcmp(gps_state, "ok") == 0);
        return true;
    }
    if (strncmp(line, "HIDE ", 5) == 0) {
        out->type = SwizMsgHide;
        extract_kv(line, "mac", out->body.hide.mac, sizeof(out->body.hide.mac));
        extract_kv(line, "oui", out->body.hide.oui, sizeof(out->body.hide.oui));
        out->body.hide.rssi = extract_int(line, "rssi", 0);
        out->body.hide.ch   = extract_int(line, "ch", 0);
        return true;
    }
    if (strncmp(line, "HIT ", 4) == 0) {
        out->type = SwizMsgHit;
        SwizHit* h = &out->body.hit;
        extract_kv(line, "mac",  h->mac,  sizeof(h->mac));
        extract_kv(line, "oui",  h->oui,  sizeof(h->oui));
        extract_kv(line, "rule", h->rule, sizeof(h->rule));

        char ssid[SWIZ_SSID_BUF_SIZE];
        if (extract_kv(line, "ssid", ssid, sizeof(ssid))) {
            if (strcmp(ssid, "hidden") == 0) {
                h->ssid_hidden = true;
                h->ssid[0] = '\0';
            } else {
                h->ssid_hidden = false;
                strncpy(h->ssid, ssid, sizeof(h->ssid) - 1);
                h->ssid[sizeof(h->ssid) - 1] = '\0';
            }
        }
        h->rssi = extract_int(line, "rssi", 0);
        h->ch   = extract_int(line, "ch", 0);

        char conf_buf[8];
        if (extract_kv(line, "conf", conf_buf, sizeof(conf_buf))) {
            h->conf = parse_conf(conf_buf);
        }

        char gps_state[8] = "";
        extract_kv(line, "gps", gps_state, sizeof(gps_state));
        h->gps_ok = (strcmp(gps_state, "ok") == 0);
        extract_kv(line, "lat",  h->lat,  sizeof(h->lat));
        extract_kv(line, "lon",  h->lon,  sizeof(h->lon));
        extract_kv(line, "alt",  h->alt,  sizeof(h->alt));
        extract_kv(line, "time", h->time, sizeof(h->time));

        flock_oui_lookup(h->oui, h->vendor, sizeof(h->vendor));
        return true;
    }
    return false;
}
