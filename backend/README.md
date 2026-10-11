# Bundled oFono backend

This directory contains the oFono companion plugin bundled with **Felis BT Battery Audio**.

The plugin runs inside the stock `ofonod` process and adds battery reporting for Bluetooth audio devices that send either:

- HFP 1.7 `AT+BIEV` battery indicator reports, or
- Apple-style `AT+XAPL` / `AT+IPHONEACCEV` accessory battery reports.

The reported value is published to BlueZ through `org.bluez.BatteryProvider1`, so the GUI can consume it as ordinary `org.bluez.Battery1` data.

The stock `/usr/sbin/ofonod` binary is not replaced or patched.

The plugin intentionally does not contain vendor-specific headset protocols and does not estimate battery state.
