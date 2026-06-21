# BLE Spam ESP32 (CYD / ESP32-2432S028)

LVGL-based BLE spam firmware for the "Cheap Yellow Display" board, ported from the
`blespam-fixed` Android app with one major upgrade: **per-packet MAC randomization**,
which Android cannot do but the ESP32 BLE stack can — so popups keep re-appearing on
iOS/Android instead of stopping after the device caches your MAC.

## Features
- Apple Continuity spam: AirPods / AirPods Pro / AirPods Max / AirTag / Apple TV Setup /
  Keyboard / Action Modal / iOS17 crash burst
- Android Fast Pair, Samsung Buds/Watch Easy Setup, Xiaomi Quick Connect, Windows Swift Pair
- Group modes: All Apple / All Android / All Devices (cycles through every payload)
- Touchscreen UI: scrollable attack list, live TX counter, packet log screen, settings
  screen (TX interval slider 5–500ms, MAC-rotation toggle)
- Settings persisted to flash (NVS) across reboots

## Build (GitHub Actions — recommended, no local toolchain needed)
1. Push this repo to GitHub (same flow as the Android app).
2. Go to **Actions** tab → wait for the build → download the `ble-spam-esp32-firmware`
   artifact. It contains `bootloader.bin`, `partitions.bin`, `firmware.bin`.

## Flash (browser, no install)
1. Open `https://esp.huhn.me` in Chrome on your PC.
2. Connect → select the ESP32's COM port.
3. Add three files at these addresses:

   | File              | Address  |
   |-------------------|----------|
   | bootloader.bin    | `0x1000` |
   | partitions.bin    | `0x8000` |
   | firmware.bin      | `0x10000`|

4. Hold **BOOT** on the board, click **Flash**, release BOOT once flashing starts.
5. Press **RST** when done.

## Touch calibration
If touches land in the wrong place, adjust `TOUCH_MIN_X/MAX_X/MIN_Y/MAX_Y` in
`include/board_config.h` and rebuild. Typical CYD values are already set as defaults.

## Display upside-down / mirrored
Change `tft.setRotation(0)` in `src/display_driver.cpp` to `1`, `2`, or `3` and rebuild.

## Why MAC rotation matters
iOS and Android cache the BLE MAC address of a device after showing its pairing popup
once, and silently ignore further packets from that same MAC for a while. Phones can't
reprogram their own BLE radio's address per-packet, but the ESP32's BLE stack can — this
firmware calls `esp_ble_gap_set_rand_addr()` with a fresh random address before every
single advertisement when "Randomize MAC each TX" is enabled (default: on), so each
packet looks like it's coming from a brand new device.
