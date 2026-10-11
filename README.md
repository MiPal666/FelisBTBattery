# Felis BT Battery Audio

Native Sailfish OS Bluetooth battery monitor and connection controller.

Felis BT Battery Audio reads paired devices directly from BlueZ, shows battery values actually reported by the Bluetooth stack, and lets the user connect or disconnect paired devices from the app. The RPM also bundles a small oFono plugin that makes common Bluetooth audio battery reports available to BlueZ.

## What it does

- Lists paired Bluetooth devices, including disconnected devices.
- Shows live `org.bluez.Battery1` values from BlueZ.
- Connects/disconnects a paired device by tapping it.
- Bundles an oFono plugin for HFP 1.7 `AT+BIEV` battery reports.
- Supports Apple-style `AT+XAPL` / `AT+IPHONEACCEV` headset battery reports.
- Publishes those HFP values back to BlueZ through `org.bluez.BatteryProvider1`.
- GUI is prepared for multiple BlueZ battery components such as left/right/case when the system actually exposes them.
- Czech and English UI.
- No polling daemon.
- No vendor-specific battery protocols and no estimated battery percentages.

## Tested environment

Primary development and testing was done on:

- Jolla Phone 2026
- Sailfish OS 5.2.x
- BlueZ 5.87
- oFono 1.29 based Sailfish build

Tested Bluetooth audio devices include Soundcore Liberty 5, Marshall Major IV, LAMAX Sounder2 Play and APW Boost ULTRA.

Normal playback, reconnects and Bluetooth phone calls were tested with the bundled backend. General Bluetooth/audio routing problems that may occur independently in the Sailfish OS stack are outside the scope of this application.

## NOTICE / known limitations

The application is primarily tested with Bluetooth audio devices.

### Multiple TWS batteries

The GUI can represent multiple `Battery1` components such as left, right and case, but this has not yet been verified end-to-end on suitable physical hardware. On the tested Jolla/BlueZ 5.87 setup, TWS devices normally expose only one HFP/accessory battery value to this application.

### Non-audio devices

Support is intentionally generic: if BlueZ exposes `org.bluez.Battery1` for a paired non-audio device, the GUI is intended to display it. Suitable physical non-audio hardware reporting a usable BlueZ battery value was not available for testing.

Reports from users with separate left/right/case BlueZ batteries or battery-reporting non-audio devices are very welcome.

## Architecture

```text
Bluetooth audio device
        |
        | HFP BIEV or XAPL/IPHONEACCEV
        v
      oFono
        |
        | bundled felisbtbattery.so
        v
BlueZ BatteryProvider1
        |
        v
BlueZ Battery1  <----- standard battery-capable Bluetooth devices
        |
        v
Felis BT Battery Audio GUI
```

The bundled plugin does **not** replace the system `ofonod` binary. It is installed as:

```text
/usr/lib64/ofono/plugins/felisbtbattery.so
```

(on architectures where `%{_libdir}` is `/usr/lib64`).

The package restarts oFono after installation so the plugin is loaded, and after final removal so it is unloaded. Upgrades do not intentionally restart it twice.

## Build

From the project root in the Sailfish SDK environment:

```bash
sfdk build
```

The RPM is `harbour-felisbtbattery` and contains both GUI and backend plugin.

The build requires SailfishApp/Qt 5 development packages plus oFono, GLib and D-Bus development metadata.

## Upgrading from development packages

The unified RPM obsoletes the old standalone `felisbtbattery-ofono-plugin` development package, so it should be replaced automatically when installing the unified package.

## Diagnostics

The backend keeps a D-Bus diagnostics interface useful for troubleshooting:

```bash
busctl --system call org.ofono /org/ofono/felisbtbattery org.feliscatus.FelisBTBattery.Diagnostics1 GetDiagnostics
```

## License

The GUI is GPL-3.0-or-later. The bundled oFono plugin is GPL-2.0-or-later; its license text is stored in `backend/LICENSE.GPL-2.0`.
