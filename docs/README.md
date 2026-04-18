# Myenergi CT Clamp Simulator

Current release overview for the ESP32 master, slave and standalone firmware variants.

## Release Snapshot

- Master firmware includes a built-in web server with fallback AP mode.
- The dashboard shows WiFi, MQTT and slave link state as `online` or `offline`.
- `Online since` and `Last Change` are shown as relative durations.
- The live graph refreshes in the same cycle as the text values.
- The OLED live view now shows A, B, C, sum, power and comms state without the old Y row.
- Slave serial OTA and OTA wiring self-test are exposed in the master web UI.

## Deployment Modes

### Standalone
- One ESP32
- WiFi + MQTT on the same board
- Generates all three phases locally

### Master
- One ESP32 with WiFi, MQTT and web UI
- Generates Phase A locally
- Drives slave updates over UART

### Slave
- One ESP32 dedicated to Phase B and Phase C generation
- Receives commands from master over UART
- Supports serial pass-through OTA via the master board

## Hardware Summary

- ESP32 DOIT DevKit v1 or compatible
- Optional 1.3 inch OLED on I2C using the SH1106 controller
- GPIO25: Phase A output on standalone/master
- GPIO26: Phase B output on standalone/slave
- GPIO32: Phase C output on standalone, slave EN control on master
- GPIO33: Slave BOOT control on master
- GPIO17 and GPIO16: UART link between master and slave

See [HARDWARE.md](HARDWARE.md) and [MASTER_SLAVE.md](MASTER_SLAVE.md) for wiring details.

## MQTT Contract

The current firmware uses a configurable MQTT path, shortened to the last path segment. The default path is `esp32CTSimulator`.

### Inputs

Canonical topics:

```text
/esp32CTSimulator/PhaseA_Amp
/esp32CTSimulator/PhaseB_Amp
/esp32CTSimulator/PhaseC_Amp
/esp32CTSimulator/SumPower_kW
```

Compatibility topics without a leading slash are also subscribed.

### Outputs

Published or retained datapoints:

```text
/esp32CTSimulator/PhaseA_Amp
/esp32CTSimulator/PhaseB_Amp
/esp32CTSimulator/PhaseC_Amp
/esp32CTSimulator/SumPower_kW
/esp32CTSimulator/Status
```

`Status` is published as JSON. The power graph uses `SumPower_kW` values from MQTT input.

See [MQTT.md](MQTT.md) for examples.

## Web UI

The master dashboard is served from `/`.

Main runtime features:

- WiFi + MQTT configuration forms
- Web access credentials
- Master OTA upload
- Slave serial OTA upload
- Slave EN and BOOT self-test
- Health and values JSON endpoints
- Live power graph with a maximum of five evenly spaced Y-axis labels

Important JSON endpoints:

- `/health`
- `/values`
- `/graph`
- `/mqtt-status`
- `/wifi-scan`
- `/ota-progress`
- `/slave-ota-progress`

If station WiFi is unavailable, the device starts an AP named `CTSimulator` and serves the same configuration portal there.

## Build And Upload

This repository includes a local PlatformIO CLI in `.venv/bin/pio`.

```bash
cd myenergi_CT-Clamp-Simulator

# Build
.venv/bin/pio run -e esp32
.venv/bin/pio run -e esp32-master
.venv/bin/pio run -e esp32-slave

# Upload
.venv/bin/pio run -e esp32 -t upload
.venv/bin/pio run -e esp32-master -t upload
.venv/bin/pio run -e esp32-slave -t upload
```

## Quick Verification

1. Flash the master.
2. Open the serial monitor.
3. Confirm WiFi and MQTT come up, or that the fallback AP starts.
4. Publish three phase values and optional `SumPower_kW`.
5. Open the web UI and confirm live values, graph and status pills update.

Example publish commands:

```bash
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseA_Amp" -m "16.3"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseB_Amp" -m "14.9"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseC_Amp" -m "15.6"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/SumPower_kW" -m "10.8"
```

## Documentation Map

- [INDEX.md](INDEX.md) for navigation
- [QUICKSTART.md](QUICKSTART.md) for first bring-up
- [MASTER_SLAVE_QUICKSTART.md](MASTER_SLAVE_QUICKSTART.md) for the two-board setup
- [MQTT.md](MQTT.md) for topic and payload details
- [TESTING.md](TESTING.md) for verification workflows

## Notes

- The UI version string is generated from build date and time.
- Relative timestamps in the web UI are based on local device uptime because the firmware does not require RTC or NTP.
- The current release favors the master web workflow over static preview HTML files.