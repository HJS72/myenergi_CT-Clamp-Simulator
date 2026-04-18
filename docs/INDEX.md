# Documentation Index

Current release date: 2026-04-18

## Start Here

### First bring-up

- [QUICKSTART.md](QUICKSTART.md) for flashing a master and validating the web UI
- [MQTT.md](MQTT.md) for the active topic contract and publish examples

### Two-board setup

- [MASTER_SLAVE_QUICKSTART.md](MASTER_SLAVE_QUICKSTART.md) for the fastest master-slave path
- [MASTER_SLAVE.md](MASTER_SLAVE.md) for UART, OTA and architecture details

### Wiring and measurement

- [HARDWARE.md](HARDWARE.md) for physical connections
- [SCHEMATICS.md](SCHEMATICS.md) for drawings and measurement references
- [TESTING.md](TESTING.md) for validation workflows

### Project context

- [README.md](README.md) for the current release summary
- [BUILD_SUMMARY.md](BUILD_SUMMARY.md) for build and test status
- [FAQ.md](FAQ.md) for common issues

## Current Runtime Highlights

- Web dashboard with WiFi, MQTT and slave status
- Relative `Online since` and `Last Change` indicators
- Power graph with synchronized text refresh
- Master OTA and slave serial OTA in the web UI
- Compact OLED live view for runtime monitoring

## Recommended Reading Order

1. [README.md](README.md)
2. [QUICKSTART.md](QUICKSTART.md)
3. [MQTT.md](MQTT.md)
4. [MASTER_SLAVE_QUICKSTART.md](MASTER_SLAVE_QUICKSTART.md) when using a second board
5. [TESTING.md](TESTING.md) before production use

## Notes

- Older Wokwi-specific preview documentation has been removed from the active doc set.
- The active OLED validation paths are the PlatformIO display-test environments in [../platformio.ini](../platformio.ini).