// Swiz WiFi Flock Hunter
// Flipper Zero companion app for WatchFlock.
//
// On launch, presents a band-picker submenu (2.4 GHz / 5 GHz / Dual / BLE).
// Selection drives a runtime CLI argument to the C5 firmware so band changes
// don't require reflashing. Selected band routes into flock_uart_start(),
// which sends `sniffflockwifi -b 2g|5g` (default dual) or `sniffflockble`
// (WiFi-off BLE Penguin detector) to the C5.

#include "swiz_flock_hunter.h"
#include "flock_log.h"
#include "flock_parser.h"
#include "flock_uart.h"
#include "flock_view.h"

#include <gui/modules/submenu.h>
#include <gui/modules/loading.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define VIEW_ID_MENU       0
#define VIEW_ID_DASHBOARD  1
#define VIEW_ID_INIT       2
#define EVENT_QUEUE_SIZE   16
#define TICK_INTERVAL_MS   250
// Time to wait after switching to the init view before kicking off the
// blocking flock_uart_start. Just enough for the dispatcher to render the
// "Initializing..." spinner so the user sees feedback instead of a frozen
// menu while the C5 boots and accepts the band command.
#define INIT_RENDER_DELAY_MS 80

typedef struct {
    SwizApp*    app;
    Submenu*    submenu;
    Loading*    loading;
    FuriTimer*  timer;
    FuriTimer*  init_timer;
    SwizBand    pending_band;
    bool        scan_started;
} AppCtx;

static void on_timer(void* ctx) {
    AppCtx*  c = ctx;
    if (!c->scan_started) return;
    uint32_t now = furi_get_tick();

    SwizMsg msg;
    while (furi_message_queue_get(c->app->event_queue, &msg, 0) == FuriStatusOk) {
        flock_view_apply_msg(c->app->view, c->app, &msg, now);
    }
    flock_view_tick(c->app->view, now);
}

// Two-phase init flow:
//   on_menu_select   → switch to "Initializing..." loading view, arm init_timer
//   on_init_timer    → run the blocking flock_uart_start, then switch to dashboard
// Phase split is needed because flock_uart_start blocks ~5s (C5 boot wait +
// stopscan + band command, dual-shot) and the dispatcher won't render the
// loading view until the menu callback returns. The 80ms init_timer delay
// gives the dispatcher one frame to paint the spinner.
static void start_scan(AppCtx* c, SwizBand band) {
    c->app->band = band;
    flock_view_set_band(c->app->view, band);
    if (!flock_uart_start(c->app)) {
        // UART setup failed — bounce back to the band picker so the user
        // can retry rather than getting stuck on the loading screen.
        view_dispatcher_switch_to_view(c->app->vd, VIEW_ID_MENU);
        return;
    }

    // Two-stage flush of any pre-mode-switch state:
    //   1. Reset the rx_stream byte buffer — wipes UNPARSED bytes from when
    //      the C5 was in the previous scan mode. Without this step the worker
    //      thread continues parsing those bytes after we drain, leaking
    //      stale HITs into the new dashboard (e.g. "HIGH 5G Liteon" badges
    //      appearing in BLE Flock mode where ch>14 is impossible).
    //   2. Brief settle to let the worker thread finish its current iteration.
    //   3. Drain the event_queue — wipes already-parsed messages.
    if (c->app->rx_stream) {
        furi_stream_buffer_reset(c->app->rx_stream);
    }
    furi_delay_ms(50);
    SwizMsg drained;
    while (furi_message_queue_get(c->app->event_queue, &drained, 0) == FuriStatusOk) {
        // discard
    }

    view_dispatcher_switch_to_view(c->app->vd, VIEW_ID_DASHBOARD);
    c->scan_started = true;
}

static void on_init_timer(void* ctx) {
    AppCtx* c = ctx;
    start_scan(c, c->pending_band);
}

// BACK on the dashboard returns to the band picker (instead of exiting the
// app). Stops the active scan, sends stopscan to the C5, resets the FAP's
// view model so the next band-pick starts with a clean dashboard. The
// dispatcher's per-view previous_callback gets the SwizApp* passed via
// view_set_context, from which we recover AppCtx.
static uint32_t on_dashboard_back(void* ctx) {
    SwizApp* app = ctx;
    AppCtx*  c   = (AppCtx*)app->host_ctx;
    if (c && c->scan_started) {
        flock_uart_stop(app);
        c->scan_started = false;
    }
    flock_view_reset(app->view);
    return VIEW_ID_MENU;
}

static void on_menu_select(void* ctx, uint32_t index) {
    AppCtx* c = ctx;
    c->pending_band = (SwizBand)index;
    view_dispatcher_switch_to_view(c->app->vd, VIEW_ID_INIT);
    furi_timer_start(c->init_timer, furi_ms_to_ticks(INIT_RENDER_DELAY_MS));
}

static bool on_menu_back(void* ctx) {
    AppCtx* c = ctx;
    view_dispatcher_stop(c->app->vd);
    return true;
}

int32_t swiz_flock_hunter_app(void* p) {
    UNUSED(p);
    SwizApp app  = {0};
    AppCtx  ctx  = { .app = &app, .scan_started = false };
    app.host_ctx = &ctx;  // exposes ctx to per-view callbacks (BACK handler)

    bool otg_was_on = furi_hal_power_is_otg_enabled();
    if(!otg_was_on) furi_hal_power_enable_otg();

    flock_log_init();

    app.gui         = furi_record_open(RECORD_GUI);
    app.notif       = furi_record_open(RECORD_NOTIFICATION);
    app.vd          = view_dispatcher_alloc();
    app.event_queue = furi_message_queue_alloc(EVENT_QUEUE_SIZE, sizeof(SwizMsg));

    // Build the dashboard view first; the submenu starts it on selection.
    app.view = flock_view_alloc(&app);
    // BACK on dashboard returns to the band-picker (not exit the app).
    view_set_previous_callback(app.view, on_dashboard_back);
    view_dispatcher_add_view(app.vd, VIEW_ID_DASHBOARD, app.view);

    // Loading view shown between band-pick and dashboard so the ~3.3s
    // C5 boot+command-send doesn't look like the app froze.
    ctx.loading = loading_alloc();
    view_dispatcher_add_view(app.vd, VIEW_ID_INIT, loading_get_view(ctx.loading));

    // Build the band-picker submenu and set it as the launch view.
    ctx.submenu = submenu_alloc();
    submenu_set_header(ctx.submenu, "Pick band to scan");
    submenu_add_item(ctx.submenu, "Dual band (2.4 + 5 GHz)", SwizBandAll, on_menu_select, &ctx);
    submenu_add_item(ctx.submenu, "2.4 GHz only",            SwizBand2G,  on_menu_select, &ctx);
    submenu_add_item(ctx.submenu, "5 GHz only",              SwizBand5G,  on_menu_select, &ctx);
    submenu_add_item(ctx.submenu, "BLE Flock (WiFi off)",    SwizBandBLE, on_menu_select, &ctx);
    View* menu_view = submenu_get_view(ctx.submenu);
    view_set_previous_callback(menu_view, NULL);
    // BACK on the menu exits the app cleanly.
    view_dispatcher_set_event_callback_context(app.vd, &ctx);
    view_dispatcher_set_navigation_event_callback(app.vd, on_menu_back);

    view_dispatcher_add_view(app.vd, VIEW_ID_MENU, menu_view);
    view_dispatcher_attach_to_gui(app.vd, app.gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app.vd, VIEW_ID_MENU);

    ctx.timer = furi_timer_alloc(on_timer, FuriTimerTypePeriodic, &ctx);
    furi_timer_start(ctx.timer, furi_ms_to_ticks(TICK_INTERVAL_MS));

    // One-shot timer that fires after the dispatcher has rendered the init
    // view, then runs the actual UART setup.
    ctx.init_timer = furi_timer_alloc(on_init_timer, FuriTimerTypeOnce, &ctx);

    view_dispatcher_run(app.vd);

    // Disable OTG FIRST so the C5 loses power, stops emitting on UART, and
    // cleanup runs quickly with empty stream buffers.
    if(!otg_was_on) furi_hal_power_disable_otg();

    furi_timer_stop(ctx.timer);
    furi_timer_free(ctx.timer);
    furi_timer_stop(ctx.init_timer);
    furi_timer_free(ctx.init_timer);

    if (ctx.scan_started) flock_uart_stop(&app);

    view_dispatcher_remove_view(app.vd, VIEW_ID_MENU);
    view_dispatcher_remove_view(app.vd, VIEW_ID_DASHBOARD);
    view_dispatcher_remove_view(app.vd, VIEW_ID_INIT);
    submenu_free(ctx.submenu);
    loading_free(ctx.loading);
    flock_view_free(app.view);
    view_dispatcher_free(app.vd);
    furi_message_queue_free(app.event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    flock_log_deinit();

    return 0;
}
