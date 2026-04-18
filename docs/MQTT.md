# MQTT Configuration Guide

## Overview

The current release uses a path-based MQTT layout centered around the configured `mqtt_path`. The master firmware subscribes to phase and power topics, republishes retained datapoints, and exposes the received values in the web UI.

## Topic Layout

The firmware shortens any configured path to its last segment. With the default path `esp32CTSimulator`, the active topics are:

```text
/esp32CTSimulator/PhaseA_Amp
/esp32CTSimulator/PhaseB_Amp
/esp32CTSimulator/PhaseC_Amp
/esp32CTSimulator/SumPower_kW
/esp32CTSimulator/Status
```

Compatibility topics without a leading slash are also accepted.

## Inputs

- `PhaseA_Amp`: Phase A current in amperes
- `PhaseB_Amp`: Phase B current in amperes
- `PhaseC_Amp`: Phase C current in amperes
- `SumPower_kW`: total power value used by the web graph and power label

Payloads are numeric strings, for example:

```text
Topic: /esp32CTSimulator/PhaseA_Amp
Payload: 23.5
```

## Outputs

The firmware publishes retained datapoints so broker-side entities exist immediately after reconnect:

- `PhaseA_Amp`
- `PhaseB_Amp`
- `PhaseC_Amp`
- `SumPower_kW`
- `Status`

`Status` is published as JSON, for example:

```json
{
  "status": "online",
  "timestamp": 1700000000
}
```

## Publish Examples

### Mosquitto

```bash
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseA_Amp" -m "16.3"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseB_Amp" -m "14.9"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/PhaseC_Amp" -m "15.6"
mosquitto_pub -h 192.168.1.100 -t "/esp32CTSimulator/SumPower_kW" -m "10.8"
```

### Python helper in this repo

```bash
python3 mqtt_test_generator.py --broker 192.168.1.100 --path esp32CTSimulator --test ramp
```

### Bash helper in this repo

```bash
TOPIC_PATH=esp32CTSimulator ./test_mqtt.sh
```

## Subscribe Examples

```bash
mosquitto_sub -h 192.168.1.100 -t "/esp32CTSimulator/#"
```

## Web Dashboard Interaction

- `SumPower_kW` drives the live graph and power label.
- `Last Change` shows the relative age of the last received MQTT message.
- Graph and text values refresh together in one polling cycle.
- WiFi and MQTT state are shown as `online` or `offline` in the header.

## Path Notes

- A configured path such as `/my/site/device/esp32CTSimulator` is normalized to `esp32CTSimulator`.
- This means broker publishers should target the short segment actually used by the firmware.

## Troubleshooting

### Values do not appear in the web UI

- Verify you publish to the normalized path
- Confirm the payload is numeric
- Confirm MQTT is `online` in the web UI

### Status topic exists but current values do not move

- Make sure you publish `PhaseA_Amp`, `PhaseB_Amp` and `PhaseC_Amp`
- If the graph is flat, also publish `SumPower_kW`

### Broker UI shows topics without a leading slash

- That is acceptable, the firmware subscribes to both variants