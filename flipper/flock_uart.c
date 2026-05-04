#include "flock_uart.h"
#include "flock_log.h"
#include "flock_parser.h"
#include "flock_view.h"

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <string.h>

#define FLOCK_BAUD       115200u
#define FLOCK_RX_TIMEOUT 100u
#define FLOCK_RX_CHUNK   64u

// ISR context: just push raw bytes into the stream buffer for the worker.
static void on_rx_irq(FuriHalSerialHandle* h, FuriHalSerialRxEvent ev, void* ctx) {
    SwizApp* app = ctx;
    if (ev & FuriHalSerialRxEventData) {
        uint8_t b = furi_hal_serial_async_rx(h);
        furi_stream_buffer_send(app->rx_stream, &b, 1, 0);
    }
}

static int32_t rx_worker_run(void* ctx) {
    SwizApp* app = ctx;
    char     line[SWIZ_LINE_BUF_SIZE];
    size_t   line_len = 0;

    while (app->worker_running) {
        uint8_t buf[FLOCK_RX_CHUNK];
        size_t  got = furi_stream_buffer_receive(
            app->rx_stream, buf, sizeof(buf), FLOCK_RX_TIMEOUT);
        for (size_t i = 0; i < got; i++) {
            uint8_t c = buf[i];
            if (c == '\n' || c == '\r') {
                if (line_len == 0) continue;
                size_t end = line_len < sizeof(line) - 1 ? line_len : sizeof(line) - 1;
                line[end] = '\0';
                SwizMsg msg;
                if (flock_parse_line(line, &msg)) {
                    if (msg.type == SwizMsgHit) {
                        flock_log_hit(&msg.body.hit);
                    }
                    furi_message_queue_put(app->event_queue, &msg, 0);
                }
                line_len = 0;
            } else if (line_len < sizeof(line) - 1) {
                line[line_len++] = (char)c;
            } else {
                // Overflow: drop the partial line, resync on next newline
                line_len = 0;
            }
        }
    }
    return 0;
}

bool flock_uart_start(SwizApp* app) {
    app->rx_stream = furi_stream_buffer_alloc(SWIZ_RX_STREAM_SIZE, 1);
    if (!app->rx_stream) return false;

    app->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if (!app->serial) {
        furi_stream_buffer_free(app->rx_stream);
        app->rx_stream = NULL;
        return false;
    }
    furi_hal_serial_init(app->serial, FLOCK_BAUD);
    furi_hal_serial_async_rx_start(app->serial, on_rx_irq, app, false);

    app->worker_running = true;
    app->rx_worker      = furi_thread_alloc_ex("FlockRxWorker", 4096, rx_worker_run, app);
    furi_thread_start(app->rx_worker);

    const char* start_cmd = "sniffflockwifi\r\n";
    if (app->band == SwizBand2G) start_cmd = "sniffflockwifi -b 2g\r\n";
    else if (app->band == SwizBand5G) start_cmd = "sniffflockwifi -b 5g\r\n";
    else if (app->band == SwizBandBLE) start_cmd = "sniffflockble\r\n";
    static const char kStopCmd[] = "stopscan\r\n";

    // Send the stop+start sequence TWICE with different timings to handle
    // both fast and slow C5 boot scenarios:
    //
    //   Shot 1 @ ~2.0s — catches the C5 if it was already booted and idle
    //                    (e.g., app restart without OTG power cycle).
    //   Shot 2 @ ~5.0s — catches the C5 on a cold boot, where Marauder's
    //                    SD/GPS/screen init can take 3-5s before the CLI
    //                    parser is alive. The first shot's commands get
    //                    dropped during the bootloader phase, the second
    //                    one lands after the `> ` prompt is up.
    //
    // The dual-shot is idempotent — if both lands, the second `stopscan`
    // briefly halts the scan started by shot 1 and the second start
    // command immediately restarts it. No user-visible side effects beyond
    // a ~50ms gap in HIT emissions.
    furi_delay_ms(2000);
    furi_hal_serial_tx(app->serial, (const uint8_t*)kStopCmd, sizeof(kStopCmd) - 1);
    furi_delay_ms(200);
    furi_hal_serial_tx(app->serial, (const uint8_t*)start_cmd, strlen(start_cmd));

    furi_delay_ms(2800);
    furi_hal_serial_tx(app->serial, (const uint8_t*)kStopCmd, sizeof(kStopCmd) - 1);
    furi_delay_ms(200);
    furi_hal_serial_tx(app->serial, (const uint8_t*)start_cmd, strlen(start_cmd));

    return true;
}

void flock_uart_stop(SwizApp* app) {
    // Stop RX FIRST so no new bytes flood the worker while we shut down.
    // Otherwise the worker can be perpetually busy processing TEST_MODE HIT
    // floods and never check worker_running for cleanup.
    if (app->serial) {
        furi_hal_serial_async_rx_stop(app->serial);
    }

    if (app->worker_running) {
        app->worker_running = false;
    }
    if (app->rx_worker) {
        furi_thread_join(app->rx_worker);
        furi_thread_free(app->rx_worker);
        app->rx_worker = NULL;
    }

    if (app->serial) {
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
        app->serial = NULL;
    }
    if (app->rx_stream) {
        furi_stream_buffer_free(app->rx_stream);
        app->rx_stream = NULL;
    }
}
