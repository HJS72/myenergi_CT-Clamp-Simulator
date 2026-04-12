# Wokwi Simulation

This project includes a ready-to-run Wokwi setup for:
- ESP32 firmware with MQTT input
- 1.3 inch OLED I2C display (SSD1306 128x64)
- live display of 3-phase current values

## Files
- `diagram.json`
- `wokwi.toml`
- `platformio.ini` environment: `esp32-wokwi`
- `diagram_master_slave.json`
- `wokwi-master-slave.toml`

## Pin mapping used in simulation
- OLED VCC -> ESP32 3V3
- OLED GND -> ESP32 GND
- OLED SCL -> ESP32 GPIO22
- OLED SDA -> ESP32 GPIO21

## MQTT defaults in Wokwi mode
When built with `-D WOKWI_SIM`:
- WiFi SSID: `Wokwi-GUEST`
- WiFi password: empty
- MQTT broker: `broker.hivemq.com`
- Topics:
  - `home/power/phase_a/current`
  - `home/power/phase_b/current`
  - `home/power/phase_c/current`

## Start simulation (VS Code + Wokwi extension)
1. Build firmware:
   - `pio run -e esp32-wokwi`
2. Open `diagram.json`.
3. Start simulation from Wokwi extension.

## Publish test values
From any MQTT client, publish float values to:
- `home/power/phase_a/current`
- `home/power/phase_b/current`
- `home/power/phase_c/current`

Example payloads:
- `25.0`
- `23.5`
- `24.2`

The OLED updates every 500 ms and shows A/B/C current values.

## Master-Slave simulation (2x ESP32 + UART + OLED)

Use this when you want to test the full architecture:
- Master ESP32 firmware: `esp32-master`
- Slave ESP32 firmware: `esp32-slave`
- OLED attached to master I2C (GPIO22/GPIO21)

### Build
1. `pio run -e esp32-master`
2. `pio run -e esp32-slave`

### Start
1. Open `diagram_master_slave.json`.
2. Start Wokwi simulation.
3. In Wokwi chip settings, assign firmware:
  - master -> `.pio/build/esp32-master/firmware.bin`
  - slave -> `.pio/build/esp32-slave/firmware.bin`

### UART wiring in this diagram
- Master GPIO17 -> Slave GPIO16
- Master GPIO16 <- Slave GPIO17
- Common GND between both boards
