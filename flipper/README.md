# Swiz Flock Hunter

Flipper Zero companion app for [WatchFlock](https://github.com/0xXyc/WatchFlock). Live dashboard for the WiFi-side Flock ALPR detector. Reads tagged-text records over UART, renders a single screen with counters and HIGH-confidence hit detail.

## What it shows

```
┌─── Swiz Flock Hunter ────┐
│ vis 47  hid 12  ch 6     │   <- counters from the STAT record
│ flag 3  hits 1  fr 18249 │
├──────────────────────────┤
│[HIGH HIT] Liteon -70dBm  │   <- only when the firmware emits a HIT
│           ch2            │      with conf=HIGH; sticky for 30s
│ e4:aa:ea:80:a1:9b        │
│ ssid: <hidden>           │
└──────────────────────────┘
```

When no HIGH-confidence hit is on screen, the bottom pane shows scanning status. The hit panel disappears 30 seconds after the last HIT and the pane goes back to scanning state until another HIGH fires.

## How it talks to the firmware

UART, USART1 on the Flipper GPIO header (pins 13 = TX, 14 = RX), 115200 baud, 8N1. The Marauder C5 Adapter wires the ESP32-C5's UART to those pins, so the C5 plugs in and just works.

The firmware must be the [WatchFlock](https://github.com/0xXyc/WatchFlock) fork built with the `SWIZ_FLIPPER_PROTOCOL` flag. Stock kokollc Marauder will not produce records this app can parse.

Records emitted by the firmware:

```
SWIZ ready proto=1                                              boot ack
STAT frames=N mgmt=N visible=N hidden=N flagged=N hits=N ch=N   every 10s
HIDE mac=XX oui=YY rssi=-NN ch=N                                per unique hidden BSSID
HIT  mac=XX oui=YY rule=R ssid="S" rssi=-NN ch=N conf=HIGH      per match
```

`conf` is one of `HIGH`, `MEDIUM`, `LOW`. Only `HIGH` triggers the hit pane.

## Build and install

Install [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) once:

```
python3 -m pip install --upgrade ufbt
ufbt update
```

Build and deploy:

```
cd ~/repos/SwizFlockHunter
ufbt
ufbt launch                # uploads .fap to the Flipper and runs it
```

The compiled `.fap` lands in `dist/<target>/swiz_flock_hunter.fap`. Sideload via qFlipper if you want it permanent: copy the `.fap` into `apps/GPIO/` on the Flipper SD card.

## Run

1. Flash the C5 with [WatchFlock](https://github.com/0xXyc/WatchFlock) compiled with `-DSWIZ_FLIPPER_PROTOCOL`. See that repo's `BUILD-C5.md`.
2. Plug C5 into the kokollc Marauder C5 Adapter.
3. Plug the adapter into the Flipper GPIO header.
4. On the Flipper, launch `Apps > GPIO > Swiz Flock Hunter`.
5. Drive to a known Flock pole. Watch counters tick up. When a HIGH hit fires, the bottom pane shows MAC, vendor, SSID, RSSI, channel.

BACK exits.

## OUI table

The vendor lookup table mirrors the firmware's high-confidence and contract-mfr lists in `WatchFlock/esp32_marauder/WiFiScan.cpp`. If you add an OUI to the firmware, mirror it in `flock_oui.c` here so the dashboard shows the right vendor name.

By [Jake / Swiz Security](https://github.com/0xXyc).
