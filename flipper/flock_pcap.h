// Per-session pcap writer for SwizFlockHunter.
//
// Captures the [BUF/BEGIN]..[BUF/CLOSE] framed binary stream emitted by
// WatchFlock's Buffer::saveSerial() over the C5<->Flipper UART, and writes
// the inner bytes (a valid libpcap stream the firmware constructs in
// Buffer::open(is_pcap=true)) to the Flipper SD card.
//
// The C5 onboard SD slot is dead on the current kokollc adapter revision,
// so this is how we recover raw 802.11 frame capture for Wireshark.

#pragma once

#include <stddef.h>
#include <stdint.h>

void flock_pcap_init(void);

// Called once at the start of each [BUF/BEGIN]..[BUF/CLOSE] block so the
// writer can decide whether the block's first 24 bytes are a duplicate pcap
// global header (emitted by Marauder every time RunFlockWifiScan kicks
// pcapOpen, happens on the dual-shot start in flock_uart_start). The first
// header per session is kept; subsequent ones are silently dropped so the
// resulting file stays Wireshark-valid.
void flock_pcap_block_begin(void);

void flock_pcap_write(const uint8_t* data, size_t len);
void flock_pcap_deinit(void);
