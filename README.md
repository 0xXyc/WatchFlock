# FlockWiFiMarauder

Marauder fork that finds Flock ALPRs over WiFi. Stock detection is BLE-only and misses any pole without an external battery, which is most of them.

Fork of [justcallmekoko/ESP32Marauder](https://github.com/justcallmekoko/ESP32Marauder). All credit for the firmware goes to kokollc. This fork adds one scan mode on top.

## What's new

A new scan mode `WIFI_SCAN_FLOCK_AP`. In the menu it's at `WiFi > Sniffers > WiFi Flock Sniff`. Over serial it's `sniffflockwifi`. It runs the WiFi radio in promiscuous mode, hops channels, and watches probe requests, beacons, and probe responses for:

- 21 high-confidence Flock OUIs (direct IEEE registration plus exclusive use)
- Contract manufacturer OUIs (Liteon, USI)
- SSID patterns: `Flock-XXXXXX`, `test_flck` (CVE-2025-59409), any `*flock*` substring

Hits go to serial and to a pcap on SD. Hidden SSIDs from a matching OUI get flagged once per BSSID per session.

## Why

Stock Marauder's "Flock Sniff" looks for BLE chatter from the optional `FS Ext Battery` accessory. Most pole-mounted Falcon V2s run on internal battery plus solar and never advertise BLE. They do continuously probe on WiFi for a hidden uplink SSID. This catches them that way. Field-confirmed in St. Pete FL: Liteon OUI `e4:aa:ea` caught a Falcon V2 in 50 seconds.

## Build and run

See [BUILD-C5.md](./BUILD-C5.md) for the ESP32-C5-DevKitC-1 + Marauder C5 Adapter + Flipper Zero rig. Companion rig on ESP32-WROVER-E: [flock-you-wifi-recon](https://github.com/0xXyc/flock-you-wifi-recon).

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
