# Quick Start Guide

Get the current release running quickly on a master board with web UI and MQTT.

## Prerequisites

- ESP32 DOIT DevKit v1 or compatible
- USB cable
- MQTT broker reachable from the ESP32
- This repository with the local PlatformIO environment available

## Step 1: Flash The Master

```bash
cd myenergi_CT-Clamp-Simulator
.venv/bin/pio run -e esp32-master -t upload
```

## Step 2: Open The Serial Monitor

```bash
.venv/bin/pio run -e esp32-master -t monitor
```

Expected boot flow:

```text
=== CT Clamp Simulator ===
MASTER (Phase A + MQTT)
Initializing...
✓ Signal generator initialized
✓ MQTT handler initialized
✓ Setup complete!
```

If no station WiFi is configured or the connection fails, the device starts the fallback AP `CTSimulator`.

## Step 3: Open The Web UI

- If the board joins your WiFi, open its DHCP address in a browser.
- If the board starts the fallback AP, connect to `CTSimulator` and open `http://192.168.4.1/`.

In the UI you can:

- Set WiFi and MQTT values
- Watch WiFi, MQTT and slave status
- See `Last Change` and `Online since`
- Upload master and slave firmware

## Step 4: Publish Test Values

```bash
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseA_Amp" -m "25"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseB_Amp" -m "23"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseC_Amp" -m "24"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/SumPower_kW" -m "8.7"
```

Then verify in the browser:

- WiFi and MQTT pills show `online`
- Power text and graph update together
- `Last Change` increases after the most recent MQTT update

## Step 5: Optional Slave Bring-Up

If you use a second ESP32:

```bash
.venv/bin/pio run -e esp32-slave -t upload
```

Then continue with [MASTER_SLAVE_QUICKSTART.md](MASTER_SLAVE_QUICKSTART.md).

## Troubleshooting

### No web page reachable

- Check whether the device started as station or fallback AP
- Verify the USB serial monitor for the current IP address
- Reconnect power after changing WiFi credentials

### MQTT stays offline

- Verify broker IP, port and credentials in the web UI
- Confirm your publish topics match the configured path
- Use `mosquitto_sub -t "/esp32CTSimulator/#"` to inspect traffic

### OLED does not match the web UI

- The OLED is a compact summary, not a full web mirror
- Use the display test environments if the panel itself looks suspect

## Next Documents

- [MQTT.md](MQTT.md)
- [MASTER_SLAVE_QUICKSTART.md](MASTER_SLAVE_QUICKSTART.md)
- [TESTING.md](TESTING.md)