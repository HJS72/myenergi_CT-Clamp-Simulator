# Build And Test Summary

Current release status as of 2026-04-18.

## Firmware State

- Master web workflow is active and flash-tested
- Standalone, master and slave PlatformIO targets remain available
- OLED runtime summary and live graph are aligned with the current UI behavior

## Included Test Utilities

- `mqtt_test_generator.py`
- `test_mqtt.sh`

## Removed Obsolete Files

- `portal_preview.html`
- `check_mqtt_values.py`
- `test_mqtt_values.py`

These were old preview or device-specific debug artifacts and were not part of the active build or documentation flow anymore.

## Recommended Validation Path

1. Flash the master with `.venv/bin/pio run -e esp32-master -t upload`
2. Open the web UI and verify fallback AP or station mode
3. Publish `PhaseA_Amp`, `PhaseB_Amp`, `PhaseC_Amp` and `SumPower_kW`
4. Confirm header status, relative timestamps and graph updates
5. Flash the slave and verify UART and slave OTA path if using two boards

## MQTT Path Reminder

Default normalized path:

```text
/esp32CTSimulator/PhaseA_Amp
/esp32CTSimulator/PhaseB_Amp
/esp32CTSimulator/PhaseC_Amp
/esp32CTSimulator/SumPower_kW
/esp32CTSimulator/Status
```

## Documentation To Read Next

- [README.md](README.md)
- [QUICKSTART.md](QUICKSTART.md)
- [MQTT.md](MQTT.md)
- [MASTER_SLAVE_QUICKSTART.md](MASTER_SLAVE_QUICKSTART.md)
- [TESTING.md](TESTING.md)