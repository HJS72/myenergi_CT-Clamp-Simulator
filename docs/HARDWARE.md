# Hardware Setup Guide

## Overview
This guide describes the practical hardware setup for the ESP32-based Myenergi Harvi/Zappi CT signal simulator.

Supported deployments:
- Standalone: one ESP32 generates all phases.
- Master-Slave: Master handles WiFi/MQTT and Phase A, Slave generates Phase B and Phase C via UART commands.

Display used across project (Wokwi + optional physical build):
- 1.3 inch OLED I2C display (SSD1306 128x64)
- Connected on ESP32 I2C pins: GPIO22 (SCL), GPIO21 (SDA)

For full ASCII schematics and measurement drawings, see `docs/SCHEMATICS.md`.

## Recommended Hardware Topology

### Standalone (Single ESP32)
- GPIO25 (DAC1): Phase A output
- GPIO26 (DAC2): Phase B output
- GPIO32 (PWM or optional external DAC path): Phase C output
- GND: common reference to Harvi/Zappi input ground

### Master-Slave (Recommended)

Master ESP32:
- GPIO25 (DAC1): Phase A output
- GPIO17 (UART TX2): to Slave GPIO16
- GPIO16 (UART RX2): from Slave GPIO17
- WiFi enabled, MQTT enabled

Slave ESP32:
- GPIO25 (DAC1): Phase C output
- GPIO26 (DAC2): Phase B output
- GPIO16 (UART RX2): from Master GPIO17
- GPIO17 (UART TX2): to Master GPIO16
- EN: controlled by Master GPIO32 for serial pass-through OTA
- GPIO0: controlled by Master GPIO33 for serial pass-through OTA boot mode
- WiFi disabled (low power)

OLED display (on Master or standalone board):
- VCC -> 3V3
- GND -> GND
- SCL -> GPIO22
- SDA -> GPIO21

## Electrical Notes

### ESP32 DAC characteristics
- Resolution: 8-bit (0-255)
- Nominal range: 0V to 3.3V
- Midpoint bias for zero current: about 1.65V (DAC value ~128)

### Signal conditioning (recommended)
- Series resistor per output: 10kOhm
- Optional noise filter: 0.1uF to GND near target input
- If required by your interface path: burden resistor around 16Ohm (typical CT context)

### Wiring quality
- Use short signal wires where possible.
- Prefer shielded cable for UART between Master and Slave.
- Keep all grounds common and low-impedance.

## Connection Checklist

### Master board
- [ ] USB 5V power connected
- [ ] GPIO25 wired to Phase A input
- [ ] GPIO17 TX -> Slave GPIO16 RX
- [ ] GPIO16 RX <- Slave GPIO17 TX
- [ ] (Optional) OLED SCL -> GPIO22 and SDA -> GPIO21
- [ ] Common GND connected to Slave and Harvi/Zappi

### Slave board
- [ ] USB 5V power connected
- [ ] GPIO25 wired to Phase C input
- [ ] GPIO26 wired to Phase B input
- [ ] GPIO16 RX <- Master GPIO17 TX
- [ ] GPIO17 TX -> Master GPIO16 RX
- [ ] EN <- Master GPIO32 (or via transistor stage)
- [ ] GPIO0 <- Master GPIO33 (or via transistor stage)
- [ ] Common GND connected

## Oscilloscope Test Points

Recommended probes:
- CH1: Master GPIO25 (Phase A)
- CH2: Slave GPIO25 (Phase C)
- CH3: Slave GPIO26 (Phase B)
- Scope GND: common system GND

Recommended scope setup:
- Time base: 5ms/div (for 50Hz period visibility)
- Trigger: rising edge on CH1
- Verify phase shift between channels is about 120 degrees for equal current setpoints.

Expected baseline:
- At 0A command: near-flat line around 1.65V
- At non-zero current: centered AC waveform around 1.65V

Detailed oscilloscope procedures are in `docs/TESTING.md` and `docs/SCHEMATICS.md`.

## Powering Options

- Development bench: one USB cable per ESP32 for easiest flashing and serial monitoring.
- Field setup: stable 5V supply with sufficient current margin.
- Add bulk decoupling near boards if supply cables are long.

## Safety

Important:
- DAC outputs are low-voltage analog outputs and are not mains-isolated.
- Never connect ESP32 GPIO or DAC pins directly to mains conductors.
- Validate isolation strategy before interfacing with any CT-related or grid-adjacent wiring.
- Disconnect power before rewiring.

## Troubleshooting Quick Hits

No output waveform:
1. Confirm firmware mode and pin mapping.
2. Check serial logs for MQTT/UART activity.
3. Verify output pin with multimeter (around 1.65V idle).

Slave not updating:
1. Re-check UART TX/RX crossover and common GND.
2. Lower UART baud rate if cable is long or noisy.
3. Confirm Master receives MQTT Phase B/C values.

Harvi/Zappi not reading correctly:
1. Recheck amplitude scaling and frequency.
2. Verify wiring to the expected phase inputs.
3. Verify phase relationship under equal test currents.

## Related Documents
- `docs/SCHEMATICS.md` for full wiring and circuit drawings
- `docs/TESTING.md` for validation procedure and oscilloscope workflow
- `docs/MASTER_SLAVE_QUICKSTART.md` for bring-up sequence
- `docs/SHOPPING_LIST.md` for bill of materials
- `docs/WOKWI.md` for OLED wiring used in simulation

## References
- ESP32 documentation: https://espressif.com
- PlatformIO: https://platformio.org
