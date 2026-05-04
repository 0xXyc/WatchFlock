# WatchFlock

ESP32-C5 firmware for spotting **Flock Safety** ALPR cameras and **SoundThinking** (formerly ShotSpotter) acoustic gunshot sensors in the wild. Privacy-research tooling — passive detection only, no jamming, no offensive payloads.

Fork of [justcallmekoko/ESP32Marauder](https://github.com/justcallmekoko/ESP32Marauder). All credit for the underlying firmware goes to kokollc. This fork adds three things: a WiFi-side ALPR detector, a BLE-side Penguin-battery detector, and a tagged-text protocol that streams hits to a Flipper Zero companion app over UART.

## What this fork adds

| Mode | CLI command | Detects |
|------|-------------|---------|
| **`WIFI_SCAN_FLOCK_AP`** | `sniffflockwifi [-b 2g\|5g\|all]` | Pole-mounted Falcon V2s probing for hidden uplink SSIDs |
| **`BT_SCAN_FLOCK_BLE`** | `sniffflockble` | External Penguin batteries advertising via BLE (XUNTONG mfg ID `0x09C8`) |
| **`SWIZ_FLIPPER_PROTOCOL`** *(compile flag)* | — | Emits tagged-text `HIT` / `STAT` / `HIDE` / `SWIZ ready` records the [WatchFlock-Hunter](https://github.com/0xXyc/SwizFlockHunter) Flipper FAP parses |

## Detection rules

**WiFi side** (probe-req, probe-resp, beacon parsing in promiscuous mode):

- 21 direct Flock OUIs incl. `b4:1e:52` (Flock Safety) and `e4:aa:ea` (Liteon, field-confirmed in St. Pete FL)
- Contract-manufacturer OUIs: Liteon, USI
- ShotSpotter / SoundThinking OUI `d4:11:d6`
- SSID patterns: `Flock-XXXXXX`, `test_flck` (CVE-2025-59409), `*flock*` substring (case-insensitive)

**BLE side** (BLE adverts via NimBLE):

- Penguin battery: XUNTONG manufacturer ID `0x09C8` + 10-digit name pattern (or legacy `Penguin-XXXXXXXXXX` / `FS Ext Battery`)
- Serial extraction: pulls TN-prefix + digits out of the manufacturer data block

Hits are streamed to UART (115200 baud) and dumped to SD as `flockwifi-XXXX.pcap` and `flock-XXXX.pcap` per session, with GPS-tagged CSV when a fix is available.

## Why

Stock Marauder's "Flock Sniff" only looks for BLE chatter from the optional Penguin battery. Most pole-mounted Falcon V2s run on internal battery + solar and never advertise BLE — but they do continuously probe WiFi for a hidden uplink SSID with predictable OUIs. This fork catches both surfaces.

WatchFlock is for understanding where surveillance hardware is installed in your community — *defensive* recon for journalists, researchers, civil-liberties groups, and curious civilians. It is not a jamming tool.

## Repo layout

Three components, one repo. They're separate codebases (different toolchains) but tightly coupled (the FAP only works with this firmware, and the emitter only matters for testing this firmware end-to-end).

```
WatchFlock/
  esp32_marauder/   firmware for the ESP32-C5 (Arduino + Marauder fork)
  C5_Py_Flasher/    Python flasher for the C5 over USB-C
  flipper/          Flipper Zero FAP companion (built with ufbt)
  emitter/          ESP32-WROVER-E test rig that fakes Flock/Penguin signals
```

### Firmware ([esp32_marauder/](./esp32_marauder/))

The C5 sniffer with WIFI_SCAN_FLOCK_AP and BT_SCAN_FLOCK_BLE modes. Build and flash via [BUILD-C5.md](./BUILD-C5.md). Pin Arduino ESP32 core to 3.3.0 (3.3.8 has a PSRAM regression on the N8R8 chip), use `CDCOnBoot=default`, partition `default_8MB`.

### Flipper companion ([flipper/](./flipper/))

Live dashboard for the SWIZ tagged-text records the firmware emits over UART. Per-MAC unique counter, peak RSSI, haptic + audio alerts on first detection per device, BACK to the band picker. Build with `ufbt` from inside the `flipper/` dir. See [flipper/README.md](./flipper/README.md).

### Emitter ([emitter/](./emitter/))

WROVER-E sketch that spoofs four Flock-OUI WiFi identities and three Penguin BLE identities on rotation. Lets you test the firmware + FAP end to end without driving to a real ALPR pole. Plus debug scripts (`c5-tail.sh`, `c5-cmd.sh`) for monitoring the C5 over USB-CDC without auto-resetting it. See [emitter/README.md](./emitter/README.md).

Inspired by the [Watch_Dogs](https://en.wikipedia.org/wiki/Watch_Dogs) games. Turning the city's sensors back on the people who installed them.

By [Jake / Swiz Security](https://github.com/0xXyc).

---

<!---[![License: MIT](https://img.shields.io/github/license/mashape/apistatus.svg)](https://github.com/justcallmekoko/ESP32Marauder/blob/master/LICENSE)--->
<!---[![Gitter](https://badges.gitter.im/justcallmekoko/ESP32Marauder.png)](https://gitter.im/justcallmekoko/ESP32Marauder)--->
<!---[![Build Status](https://travis-ci.com/justcallmekoko/ESP32Marauder.svg?branch=master)](https://travis-ci.com/justcallmekoko/ESP32Marauder)--->
<!---Shields/Badges https://shields.io/--->

# ESP32 Marauder
<p align="center"><img alt="Marauder logo" src="https://github.com/justcallmekoko/ESP32Marauder/blob/master/pictures/marauder_skull_patch_04_full_final.png?raw=true" width="300"></p>
<p align="center">
  <b>A suite of WiFi/Bluetooth offensive and defensive tools for the ESP32</b>
  <br><br>
  <a href="https://github.com/justcallmekoko/ESP32Marauder/blob/master/LICENSE"><img alt="License" src="https://img.shields.io/github/license/mashape/apistatus.svg"></a>
  <a href="https://gitter.im/justcallmekoko/ESP32Marauder"><img alt="Gitter" src="https://badges.gitter.im/justcallmekoko/ESP32Marauder.png"/></a>
  <a href="https://github.com/justcallmekoko/ESP32Marauder/releases/latest"><img src="https://img.shields.io/github/downloads/justcallmekoko/ESP32Marauder/total" alt="Downloads"/></a>
  <br>
  <a href="https://twitter.com/intent/follow?screen_name=jcmkyoutube"><img src="https://img.shields.io/twitter/follow/jcmkyoutube?style=social&logo=twitter" alt="Twitter"></a>
  <a href="https://www.instagram.com/just.call.me.koko"><img src="https://img.shields.io/badge/Follow%20Me-Instagram-orange" alt="Instagram"/></a>
  <br><br>
</p>
    
[![Build and Push](https://github.com/justcallmekoko/ESP32Marauder/actions/workflows/build_push.yml/badge.svg)](https://github.com/justcallmekoko/ESP32Marauder/actions/workflows/build_push.yml)

## Getting Started
Download the [latest release](https://github.com/justcallmekoko/ESP32Marauder/releases/latest) of the firmware.  

Check out the project [wiki](https://github.com/justcallmekoko/ESP32Marauder/wiki) for a full overview of the ESP32 Marauder

# For Sale Now
You can buy the ESP32 Marauder using [this link](https://www.justcallmekokollc.com)
