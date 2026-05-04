// Swiz Flock Hunter shared types.
// Pairs with the WatchFlock firmware built with -DSWIZ_FLIPPER_PROTOCOL.
// Records arrive over UART (USART1, GPIO pins 13/14) at 115200 baud.

#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdint.h>
#include <stdbool.h>

#define SWIZ_RX_STREAM_SIZE  1024
#define SWIZ_LINE_BUF_SIZE   256
#define SWIZ_SSID_BUF_SIZE   33
#define SWIZ_VENDOR_BUF_SIZE 24
#define SWIZ_HIT_STICKY_MS   30000
#define SWIZ_SEEN_MAX        32

typedef enum {
    SwizConfNone = 0,
    SwizConfLow,
    SwizConfMedium,
    SwizConfHigh,
} SwizConf;

#define SWIZ_GPS_FIELD_SIZE  16
#define SWIZ_TIME_FIELD_SIZE 24

typedef struct {
    char     mac[18];
    char     oui[9];
    char     vendor[SWIZ_VENDOR_BUF_SIZE];
    char     rule[24];
    char     ssid[SWIZ_SSID_BUF_SIZE];
    bool     ssid_hidden;
    int      rssi;
    int      ch;
    SwizConf conf;
    bool     gps_ok;
    char     lat[SWIZ_GPS_FIELD_SIZE];
    char     lon[SWIZ_GPS_FIELD_SIZE];
    char     alt[SWIZ_GPS_FIELD_SIZE];
    char     time[SWIZ_TIME_FIELD_SIZE];
    uint32_t fired_at_tick;
} SwizHit;

// Per-unique-MAC stats accumulated during a scan session. Lets the dashboard
// show "you've encountered N distinct flocks" rather than just the last raw
// hit count from the firmware (which counts every probe-req frame match).
typedef struct {
    char     mac[18];
    int8_t   rssi_best;       // strongest signal we've seen for this MAC
    int8_t   rssi_last;       // most recent (for proximity inference)
    uint32_t hit_count;       // raw hits attributed to this MAC
    uint32_t last_seen_tick;  // for "last hit Ns ago" rendering
} SwizSeenMac;

typedef struct {
    // All "this session" counters. The firmware emits cumulative-since-scan
    // counters in STAT, but with a loud emitter nearby (or any extended
    // running scan), the first STAT after FAP open already shows a large
    // number. To give the user "0 at app open, ticks up from there", we
    // snapshot the first STAT we receive into baseline_* below and display
    // current STAT minus baseline. If the firmware ever resets its own
    // counters mid-session (current < baseline), we re-snapshot.
    uint32_t frames;
    uint32_t mgmt;
    uint32_t visible;
    uint32_t hidden;
    uint32_t flagged;
    uint32_t hits;
    uint8_t  ch;
    bool     gps_ok;
    uint32_t last_stat_tick;
    // Baseline only the fast-tick counters that can ramp into the hundreds
    // before the FAP even renders. vis/hid grow slowly (one per new BSSID)
    // and the firmware already resets them on sniffflock* — leaving them
    // raw matches user intuition ("how many flocks/hidden APs am I near
    // right now"). hits and frames need baseline because the WROVER spammer
    // (or a real busy RF environment) racks up dozens before first STAT.
    bool     baseline_set;
    uint32_t baseline_frames;
    uint32_t baseline_hits;
    // Strongest (closest to 0) RSSI seen across all session HITs. Updated
    // on every HIT in flock_view_apply_msg. Surfaces proximity awareness
    // in the counter row — good for "am I close to a flock right now"
    // field-walk reading without needing the badge to be active.
    int8_t   peak_rssi;
    bool     peak_rssi_set;

    SwizHit  latest;
    bool     latest_valid;

    bool     proto_ready;
    uint8_t  proto_version;

    // Active scan band (set on launch, before any UART traffic). Drives the
    // header label and waiting-pane copy so the user can tell BLE-mode from
    // WiFi-mode at a glance even before the first SWIZ ack.
    uint8_t  active_band;

    // Unique-MAC tracking. seen[] is a fixed-size bag — once full, repeats
    // for known MACs still update stats but new MACs only bump uniq_total
    // (their per-MAC stats just aren't tracked). For a typical field walk
    // 32 distinct flocks is far beyond what one session encounters.
    SwizSeenMac seen[SWIZ_SEEN_MAX];
    uint16_t    seen_count;     // entries currently populated in seen[]
    uint32_t    uniq_total;     // distinct MACs encountered (may exceed seen_count)
    uint32_t    last_hit_tick;  // any hit, new or repeat — refreshes whenever we get a HIT
} SwizModel;

typedef struct SwizApp SwizApp;

typedef enum {
    SwizBandAll = 0,
    SwizBand2G  = 1,
    SwizBand5G  = 2,
    SwizBandBLE = 3,  // Pure-BLE Flock Penguin detector; firmware shuts WiFi off
} SwizBand;

struct SwizApp {
    Gui*                  gui;
    ViewDispatcher*       vd;
    View*                 view;
    NotificationApp*      notif;  // haptic + audio alerts on first detection per MAC
    void*                 host_ctx; // opaque AppCtx pointer; cast inside swiz_flock_hunter.c

    FuriHalSerialHandle*  serial;
    FuriStreamBuffer*     rx_stream;
    FuriThread*           rx_worker;
    FuriMessageQueue*     event_queue;
    bool                  worker_running;

    SwizBand              band;
};
