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

## Companion app

[**WatchFlock-Hunter**](https://github.com/0xXyc/SwizFlockHunter) is a Flipper Zero FAP that reads the SWIZ-protocol tagged-text records over UART (Flipper GPIO pins 13/14 ↔ kokollc Marauder C5 Adapter). Live dashboard, per-MAC unique counter, haptic + audio alerts on first detection.

## Build and run

See [BUILD-C5.md](./BUILD-C5.md). TL;DR: Arduino ESP32 core 3.3.0 (NOT 3.3.8 — PSRAM regression), `sketch_flags=-DMARAUDER_C5 -DSWIZ_FLIPPER_PROTOCOL`, partition scheme `default_8MB`, CDCOnBoot disabled, then flash via `c5_flasher.py`.

Inspired by the [Watch_Dogs](https://en.wikipedia.org/wiki/Watch_Dogs) games — turning the city's sensors back on the people who installed them.

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
