// UART worker for the SwizFlockHunter Flipper app.
// Acquires USART1 (GPIO pins 13/14) at 115200 baud, reads bytes via the
// async RX ISR into a stream buffer, and a worker thread splits on '\n'
// then parses each line into a SwizMsg posted on app->event_queue.

#pragma once

#include "swiz_flock_hunter.h"

bool flock_uart_start(SwizApp* app);
void flock_uart_stop(SwizApp* app);
