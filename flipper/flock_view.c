#include "flock_view.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <string.h>

// Alert intensity scales with confidence tier so the user can recognize
// what they walked past without looking at the screen. Tuned for "firm but
// not bomb-going-off", earlier audiovisual_alert version was startling.
//   HIGH   → success chirp + vibro (pleasant ascending tone, definitely
//            noticeable but not alarming)
//   MEDIUM → double vibro (two haptic taps, no sound, subtle in public)
//   LOW    → single short vibro (barely noticeable, just confirms a log)
// Fires once per uniq++, never on repeats, no buzz spam.
static void fire_alert(SwizApp* app, SwizConf conf) {
    if (!app || !app->notif) return;
    const NotificationSequence* seq = NULL;
    switch (conf) {
        case SwizConfHigh:   seq = &sequence_success;       break;
        case SwizConfMedium: seq = &sequence_double_vibro;  break;
        case SwizConfLow:    seq = &sequence_single_vibro;  break;
        default: return;
    }
    notification_message(app->notif, seq);
}

// GPS-lock acquired notification. Must be audibly + tactilely distinct from
// the three Flock-detection sequences so the user knows what just happened
// without looking. Three rapid ascending notes (G5 / C6 / E6, perfect
// fourth + major third) with a green LED hold and a single brief vibro at
// the start. Distinct from sequence_success (C-major triad over a longer
// hold) and from the vibro-only MEDIUM/LOW sequences.
static const NotificationSequence sequence_gps_lock = {
    &message_green_255,
    &message_vibro_on,
    &message_note_g5,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_note_e6,
    &message_delay_100,
    &message_sound_off,
    &message_delay_250,
    &message_green_0,
    NULL,
};

static void fire_gps_lock(SwizApp* app) {
    if (!app || !app->notif) return;
    notification_message(app->notif, &sequence_gps_lock);
}

// 128x64 layout
//
//   [0..12]   header bar      "Swiz Flock Hunter"
//   [13..34]  counters         vis/hid/ch on row 1, flag/hits/fr on row 2
//   [35]      separator line
//   [36..63]  hit pane         empty when no HIGH hit, or 3 lines of detail

static void draw_header(Canvas* c, SwizModel* m) {
    canvas_set_font(c, FontPrimary);
    // Header tracks the active band so the user can tell which radio is in
    // play even before the first SWIZ ack lands (no hits yet, no badge).
    const char* title = (m->active_band == SwizBandBLE) ?
                        "WatchFlock BLE" :
                        "WatchFlock WiFi";
    canvas_draw_str(c, 2, 10, title);
    canvas_draw_line(c, 0, 12, 128, 12);

    // GPS indicator on counter row 1: filled box if fix, hollow if not.
    canvas_set_font(c, FontSecondary);
    if (m->gps_ok) {
        canvas_draw_box(c, 104, 14, 22, 9);
        canvas_set_color(c, ColorWhite);
        canvas_draw_str(c, 107, 22, "GPS");
        canvas_set_color(c, ColorBlack);
    } else {
        canvas_draw_frame(c, 104, 14, 22, 9);
        canvas_draw_str(c, 107, 22, "GPS");
    }
}

static void draw_counters(Canvas* c, SwizModel* m) {
    char buf[40];
    canvas_set_font(c, FontSecondary);

    snprintf(buf, sizeof(buf), "vis %lu  hid %lu  ch %u",
             (unsigned long)m->visible,
             (unsigned long)m->hidden,
             (unsigned)m->ch);
    canvas_draw_str(c, 2, 22, buf);

    // Row 2: uniq + hits + peak RSSI. Replaced the firmware's `flagged`
    // (which was a duplicate of hidden) with FAP-side `uniq` (distinct MACs
    // hit), and the `fr` (frames) with `peak` RSSI, the strongest signal
    // observed this session, which gives a quick "how close am I" read
    // during a field walk. Frame totals can still be recovered from the
    // pcap if needed.
    if (m->peak_rssi_set) {
        snprintf(buf, sizeof(buf), "uniq %lu hits %lu peak %d",
                 (unsigned long)m->uniq_total,
                 (unsigned long)m->hits,
                 (int)m->peak_rssi);
    } else {
        snprintf(buf, sizeof(buf), "uniq %lu hits %lu peak -",
                 (unsigned long)m->uniq_total,
                 (unsigned long)m->hits);
    }
    canvas_draw_str(c, 2, 32, buf);

    canvas_draw_line(c, 0, 35, 128, 35);
}

static void draw_hit_pane(Canvas* c, SwizModel* m) {
    if (m->latest_valid &&
        (m->latest.conf == SwizConfHigh || m->latest.conf == SwizConfMedium)) {
        // Inverted badge with conf level + band. ch=0 is the BLE sentinel
        // (Penguin detector emits ch=0 since BLE has no WiFi channel concept);
        // ch 1-14 is 2.4 GHz; everything else is 5 GHz.
        const char* conf_lbl = (m->latest.conf == SwizConfHigh) ? "HIGH" : "MED";
        const char* band_lbl = (m->latest.ch == 0) ? "BLE" :
                               (m->latest.ch >= 1 && m->latest.ch <= 14) ? "2G" : "5G";
        char badge[12];
        snprintf(badge, sizeof(badge), "%s %s", conf_lbl, band_lbl);
        canvas_draw_box(c, 0, 37, 60, 11);
        canvas_set_color(c, ColorWhite);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 2, 46, badge);
        canvas_set_color(c, ColorBlack);

        canvas_set_font(c, FontSecondary);

        // MAC on its own line so the address is fully readable
        canvas_draw_str(c, 2, 56, m->latest.mac);

        // Vendor + RSSI/ch (band is in the badge so we save space here).
        // "dBm" suffix is dropped, the negative RSSI implies units, and on
        // FontSecondary at 5px/char, "dBm" rendered as visual mush ("...dl").
        // ch is only shown for WiFi hits (ch>0); BLE uses ch=0 sentinel which
        // is already conveyed by the BLE badge so the channel digit is noise.
        char buf[40];
        const char* vendor = m->latest.vendor[0] ? m->latest.vendor : "unknown";
        if (m->latest.ch > 0) {
            snprintf(buf, sizeof(buf), "%s %d ch%d",
                     vendor, m->latest.rssi, m->latest.ch);
        } else {
            snprintf(buf, sizeof(buf), "%s %d", vendor, m->latest.rssi);
        }
        canvas_draw_str(c, 64, 46, buf);

        // SSID line below, relabeled to "serial:" for BLE hits since the
        // protocol stuffs the BLE Penguin's TN-prefix serial into the ssid
        // field (no real WiFi SSID exists in BLE adverts). For ch>0 hits
        // it's a real WiFi SSID and stays labeled as such.
        const char* ssid_disp = m->latest.ssid_hidden ? "<hidden>" :
                                (m->latest.ssid[0] ? m->latest.ssid : "<empty>");
        const char* ssid_label = (m->latest.ch == 0) ? "serial:" : "ssid:";
        char ssid_buf[48];
        snprintf(ssid_buf, sizeof(ssid_buf), "%s %s", ssid_label, ssid_disp);
        canvas_draw_str(c, 2, 63, ssid_buf);
        return;
    }

    canvas_set_font(c, FontSecondary);
    bool is_ble = (m->active_band == SwizBandBLE);
    if (!m->proto_ready) {
        canvas_draw_str(c, 2, 48,
                        is_ble ? "Waiting for BLE FW" : "Waiting for SWIZ");
        canvas_draw_str(c, 2, 60,
                        is_ble ? "sniffflockble cmd"  : "firmware @ 115200 baud");
    } else if (m->uniq_total > 0) {
        // Latest hit expired the sticky timer but we've seen flocks this
        // session, show running summary instead of an empty "no flocks"
        // message that contradicts the visible hits/uniq counters.
        char buf[40];
        snprintf(buf, sizeof(buf), "%lu unique flocks",
                 (unsigned long)m->uniq_total);
        canvas_draw_str(c, 2, 48, buf);
        uint32_t now = furi_get_tick();
        uint32_t freq = furi_kernel_get_tick_frequency();
        uint32_t since_ms = (now > m->last_hit_tick && freq)
            ? (uint32_t)(((uint64_t)(now - m->last_hit_tick) * 1000ULL) / freq)
            : 0;
        if (since_ms < 60UL * 1000UL) {
            snprintf(buf, sizeof(buf), "last hit %lus ago",
                     (unsigned long)(since_ms / 1000UL));
        } else {
            snprintf(buf, sizeof(buf), "last hit %lum ago",
                     (unsigned long)(since_ms / 60000UL));
        }
        canvas_draw_str(c, 2, 60, buf);
    } else {
        canvas_draw_str(c, 2, 48,
                        is_ble ? "Scanning BLE..."    : "Scanning...");
        canvas_draw_str(c, 2, 60, "No flocks hit yet");
    }
}

static void draw_callback(Canvas* c, void* model_v) {
    SwizModel* m = model_v;
    canvas_clear(c);
    draw_header(c, m);
    draw_counters(c, m);
    draw_hit_pane(c, m);
}

static bool input_callback(InputEvent* e, void* ctx) {
    UNUSED(e);
    UNUSED(ctx);
    // BACK is intentionally not consumed here, the dispatcher routes it to
    // the view's previous_callback (set in swiz_flock_hunter.c) which cleans
    // up the active scan and returns to the band-picker. Returning false
    // lets all input events bubble up.
    return false;
}

View* flock_view_alloc(SwizApp* app) {
    View* v = view_alloc();
    view_allocate_model(v, ViewModelTypeLocking, sizeof(SwizModel));
    view_set_context(v, app);
    view_set_draw_callback(v, draw_callback);
    view_set_input_callback(v, input_callback);
    return v;
}

void flock_view_free(View* v) {
    view_free(v);
}

void flock_view_apply_msg(View* v, SwizApp* app, const SwizMsg* msg, uint32_t now_tick) {
    bool refresh = true;
    with_view_model(
        v,
        SwizModel * m,
        {
            switch (msg->type) {
            case SwizMsgReady:
                m->proto_ready    = true;
                m->proto_version  = msg->body.ready.version;
                break;
            case SwizMsgStat: {
                // Baseline only the fast-tick counters (frames, hits). vis/hid
                // grow slowly and are firmware-reset per scan-start, so they
                // pass through raw. mgmt is just a copy of frames in the
                // firmware emit, no separate baseline needed.
                const uint32_t f  = msg->body.stat.frames;
                const uint32_t hi = msg->body.stat.hits;
                if (!m->baseline_set ||
                    f  < m->baseline_frames || hi < m->baseline_hits) {
                    m->baseline_frames = f;
                    m->baseline_hits   = hi;
                    m->baseline_set    = true;
                }
                m->frames  = f  - m->baseline_frames;
                m->hits    = hi - m->baseline_hits;
                m->mgmt    = msg->body.stat.mgmt;
                m->visible = msg->body.stat.visible;
                m->hidden  = msg->body.stat.hidden;
                m->flagged = msg->body.stat.flagged;
                m->ch             = msg->body.stat.ch;
                // Edge-trigger the GPS-lock notification on false→true. The
                // STAT field gps=ok requires both module-detected AND fix
                // acquired, so the transition reliably indicates a real
                // fix. Refires if the fix is lost and reacquired (rare,
                // but useful signal if it happens mid-field).
                bool was_gps_ok = m->gps_ok;
                m->gps_ok         = msg->body.stat.gps_ok;
                if (!was_gps_ok && m->gps_ok) {
                    fire_gps_lock(app);
                }
                m->last_stat_tick = now_tick;
                // STAT is SWIZ-protocol-only; if we get one we know the
                // firmware is talking even if the SWIZ ready ack was missed.
                m->proto_ready    = true;
                break;
            }
            case SwizMsgHide:
                // Counter folded into next STAT; no separate UI surface.
                // Still flips proto_ready: any tagged record proves the C5 is
                // alive and using the SWIZ protocol.
                m->proto_ready = true;
                break;
            case SwizMsgHit: {
                m->last_hit_tick = now_tick;
                m->proto_ready   = true;

                // Track peak (strongest) RSSI for the counter row.
                int8_t r = (int8_t)msg->body.hit.rssi;
                if (!m->peak_rssi_set || r > m->peak_rssi) {
                    m->peak_rssi     = r;
                    m->peak_rssi_set = true;
                }

                // Find existing seen[] entry for this MAC (or note that it's
                // a first-time encounter). This drives uniq counting AND
                // badge eligibility, only the *first* encounter of a MAC
                // earns the 30s badge slot. Re-hits of an already-seen MAC
                // never re-show the badge, even if the prior badge timer
                // has already expired. Keeps the dashboard from getting
                // stuck on a single device that keeps spamming.
                bool is_new_mac = true;
                for (uint16_t i = 0; i < m->seen_count; i++) {
                    if (strncmp(m->seen[i].mac, msg->body.hit.mac, 17) == 0) {
                        m->seen[i].rssi_last      = (int8_t)msg->body.hit.rssi;
                        if ((int8_t)msg->body.hit.rssi > m->seen[i].rssi_best)
                            m->seen[i].rssi_best  = (int8_t)msg->body.hit.rssi;
                        m->seen[i].hit_count++;
                        m->seen[i].last_seen_tick = now_tick;
                        is_new_mac = false;
                        break;
                    }
                }

                if (is_new_mac) {
                    // First encounter of this MAC this session, show badge,
                    // start the sticky-30s timer, fire haptic + audio.
                    m->latest               = msg->body.hit;
                    m->latest.fired_at_tick = now_tick;
                    m->latest_valid         = true;

                    m->uniq_total++;
                    if (m->seen_count < SWIZ_SEEN_MAX) {
                        SwizSeenMac* s = &m->seen[m->seen_count++];
                        strncpy(s->mac, msg->body.hit.mac, sizeof(s->mac) - 1);
                        s->mac[sizeof(s->mac) - 1] = '\0';
                        s->rssi_best      = (int8_t)msg->body.hit.rssi;
                        s->rssi_last      = (int8_t)msg->body.hit.rssi;
                        s->hit_count      = 1;
                        s->last_seen_tick = now_tick;
                    }
                    fire_alert(app, msg->body.hit.conf);
                } else {
                    // Known MAC. If the badge is currently showing THIS MAC,
                    // refresh the displayed RSSI/SSID with the latest data
                    //, but DO NOT extend the sticky timer. The badge still
                    // expires 30s after first detection. If badge is already
                    // expired or showing a different MAC, leave it alone:
                    // no re-show for already-acknowledged devices.
                    bool showing_this = m->latest_valid &&
                        strncmp(m->latest.mac, msg->body.hit.mac, 17) == 0;
                    if (showing_this) {
                        uint32_t saved_fired = m->latest.fired_at_tick;
                        m->latest               = msg->body.hit;
                        m->latest.fired_at_tick = saved_fired;
                    }
                }
                break;
            }
            default:
                refresh = false;
                break;
            }
        },
        refresh);
}

void flock_view_reset(View* v) {
    with_view_model(
        v,
        SwizModel * m,
        {
            // Wipe everything but preserve active_band, start_scan will
            // overwrite it on the next pick anyway, but keeping it stable
            // here avoids a brief flash of the wrong band label between
            // the back-press and the next selection.
            uint8_t band = m->active_band;
            memset(m, 0, sizeof(*m));
            m->active_band = band;
        },
        true);
}

void flock_view_set_band(View* v, SwizBand band) {
    with_view_model(
        v,
        SwizModel * m,
        { m->active_band = (uint8_t)band; },
        true);
}

void flock_view_tick(View* v, uint32_t now_tick) {
    bool refresh = false;
    with_view_model(
        v,
        SwizModel * m,
        {
            if (m->latest_valid &&
                (now_tick - m->latest.fired_at_tick) > furi_ms_to_ticks(SWIZ_HIT_STICKY_MS)) {
                m->latest_valid = false;
                refresh         = true;
            }
            // While the summary pane is up, force a redraw so the "last hit
            // Ns ago" counter ticks visibly. Cheap, only fires when no
            // active hit badge is showing.
            if (!m->latest_valid && m->uniq_total > 0) {
                refresh = true;
            }
        },
        refresh);
}
