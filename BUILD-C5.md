# Build & Flash: Marauder Fork with WiFi Flock Sniff (ESP32-C5)

This fork adds a passive WiFi-side Flock Safety ALPR detector to ESP32Marauder. The detection logic is ported from `~/repos/flock-you-wifi-recon` (Swiz Security). It runs as a new scan mode `WIFI_SCAN_FLOCK_AP` alongside the upstream BLE-based `BT_SCAN_FLOCK`, which it replaces in practice (BLE-only Flock detection misses solar-only Falcon V2 installs).

## Why this exists

Stock Marauder ships "Flock Sniff" as a BLE-first mode that detects the optional `FS Ext Battery` accessory. Most pole-mounted Falcon V2 cameras run on internal battery + solar and never advertise BLE. They DO continuously probe on WiFi in STA mode for a hidden uplink SSID, with manufacturer OUIs that are public knowledge. This fork adds a mode that catches them via 802.11 monitor mode + OUI/SSID matching, the same way the standalone `flock-you-wifi-recon` rig does.

Field-confirmed: Liteon OUI `e4:aa:ea` caught a Falcon V2 in St. Pete FL via 12 probe-req frames over 50 seconds.

## Required hardware

| Item | Notes |
|------|-------|
| **ESP32-C5-DevKitC-1-N8R8** | 8MB flash, 8MB PSRAM. Amazon B0G4CY619B, ~$15. The C5 is required for 5 GHz band coverage. |
| **kokollc Marauder C5 Adapter for Flipper Zero** | https://justcallmekokollc.com/products/marauder-c5-adapter-flipper-zero. On-board GPS, microSD, status LEDs, protective enclosure. |
| **Flipper Zero** | Stock firmware works. Marauder companion app must be installed via the Flipper app catalog or qFlipper sideload. |

The WROVER-E will not work. Marauder has no WROVER target, and the adapter is keyed to the C5 form factor.

## Prerequisites

- Arduino IDE 2.x (or arduino-cli)
- ESP32 Arduino core 3.x (3.0.4+ for proper C5 support, ships IDF 5.3+)
- Python 3.10+ for `c5_flasher.py`
- USB-C cable for the C5 DevKit

Install ESP32 core 3.x via Arduino IDE Boards Manager:

```
Preferences → Additional Boards URLs:
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
Boards Manager → search "esp32" → install latest 3.x
```

## Build

1. Open `esp32_marauder/esp32_marauder.ino` in Arduino IDE.

2. Edit `esp32_marauder/configs.h`. Find the board-select block near the top and uncomment **only** `MARAUDER_C5`:

   ```c
   //#define MARAUDER_V8
   //#define MARAUDER_MINI_V3
   #define MARAUDER_C5         // <-- this one
   //#define MARAUDER_CARDPUTER
   ```

3. Tools menu:
   - Board: `ESP32C5 Dev Module`
   - Flash Size: `8MB`
   - PSRAM: `OPI PSRAM`
   - Partition Scheme: `Default 4MB with spiffs (or matching the bundled partitions.csv)`
   - USB CDC On Boot: `Enabled`

4. Sketch → Verify/Compile. Output goes under `~/Library/Caches/arduino/sketches/<hash>/esp32_marauder.ino.bin`.

### arduino-cli alternative

```bash
arduino-cli core install esp32:esp32@3.0.7
arduino-cli compile -b esp32:esp32:esp32c5 \
  --build-property "build.extra_flags=-DMARAUDER_C5" \
  ~/repos/flock-marauder/esp32_marauder
```

The `.bin` lands in the build directory printed at the end.

## Flash

```bash
cd ~/repos/flock-marauder/C5_Py_Flasher
python3 c5_flasher.py /path/to/your/esp32_marauder.ino.bin
```

The flasher script puts the C5 into download mode and writes bootloader + firmware. If it stalls, hold BOOT, tap RESET, release BOOT, retry.

## Run

### Menu

`WiFi → Sniffers → WiFi Flock Sniff`

The Flipper UI shows match lines as they fire. SD card gets a `flockwifi-XXXX.pcap` for each session.

### CLI (Marauder companion or USB serial at 115200)

```
sniffflockwifi
```

Stop with `stopscan`.

### Expected output

Quiet baseline:

```
[FLOCK-WIFI] [+00:00:10.123] frames_seen:842 matches:0 hidden_flagged:0 ch:6
```

Match:

```
[FLOCK-WIFI] [+00:07:24.461] MATCH(oui_flock) PROBE_REQ src:e4:aa:ea:80:a1:9b ssid:"<hidden>" rssi:-80 ch:6 hits:2
```

Hidden SSID (one per BSSID per session):

```
[FLOCK-WIFI] [+00:01:12.873] ######## HIDDEN SSID DETECTED ########
[FLOCK-WIFI]   bssid:   e4:aa:ea:80:a1:9b
[FLOCK-WIFI]   frame:   BEACON   ch:6   rssi:-72
[FLOCK-WIFI]   oui:     oui_flock
[FLOCK-WIFI]   >>> FLAGGED FOR REVIEW (#1) <<<
[FLOCK-WIFI] ######################################
```

## Detection rules

OUI lists (file: `esp32_marauder/WiFiScan.cpp`, search for `fy_flock_mac_prefixes`):

- Direct Flock + exclusive-use prefixes: 21 OUIs incl. `b4:1e:52` (Flock Safety direct), `e4:aa:ea` (Liteon, field-confirmed)
- Contract manufacturer prefixes: 6 OUIs (Liteon, USI). Lower confidence, can false positive.
- SoundThinking/ShotSpotter: `d4:11:d6`

SSID rules:

- Exact: `test_flck` (CVE-2025-59409, dev SSID baked into prod firmware)
- Pattern: `Flock-XXXXXX` where X is hex
- Substring: `*flock*` (case-insensitive)
- Substring: `*flck*` (case-insensitive, catches abbreviated variants)

Frame types matched: 802.11 management subtypes 0x4 (Probe Req), 0x5 (Probe Resp), 0x8 (Beacon).

RSSI threshold: `-100 dBm` default. Override via `-DFY_WIFI_SCAN_RSSI_MIN=<value>`.

## What this fork changes vs upstream Marauder

Five files touched, +314 / -1 line. Run `git diff master` to see everything.

| File | Change |
|------|--------|
| `WiFiScan.h` | `#define WIFI_SCAN_FLOCK_AP 84`. Two new method decls. |
| `WiFiScan.cpp` | New ~290-line section at EOF: OUI lists, helpers, callback, runner. Three small list-edits to `StartScan`, `StopScan`, `main()` channel-hop dispatch. |
| `MenuFunctions.cpp` | One new `addNodes` call under WiFi > Sniffers. |
| `CommandLine.h` | `SNIFF_FLOCK_WIFI_CMD = "sniffflockwifi"` + help string. |
| `CommandLine.cpp` | One CLI dispatch + one help line. |

The upstream BLE-based `BT_SCAN_FLOCK` mode is left intact for users who do detect external battery packs.

## Two-rig split

| Rig | Use case |
|-----|----------|
| ESP32-WROVER-E running `~/repos/flock-you-wifi-recon` | Car-based stake-outs, laptop-tethered, no SD/GPS. Already field-proven. |
| C5 + Marauder Adapter + Flipper | Handheld walkable, GPS-tagged hits to SD, 5 GHz coverage, screen UI. |

Both share the same detection logic and OUI list, so a hit on one is a hit on the other.
