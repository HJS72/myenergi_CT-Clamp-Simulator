# Myenergi Harvi/Zappi CT Sensor Simulator

ESP32-based simulator that replaces current transformers (CT sensors) in Myenergi Harvi/Zappi devices. Receives RMS current values via MQTT and generates AC voltage signals that simulate actual current flow through CT sensors.

## Overview

### Problem
The Myenergi Harvi and Zappi devices measure AC current using split-core current transformers (CT sensors). These produce AC voltage signals proportional to the current flowing through power lines. 

### Solution
This ESP32 firmware provides **three deployment modes**:

**1. Standalone (Single ESP32)** - Simplest
- Connects to WiFi and MQTT broker
- Receives RMS current values for three AC phases
- Generates AC signals for all three phases

**2. Master-Slave (Two ESP32s)** - Recommended
- **Master**: Handles WiFi/MQTT, generates Phase A, controls Slave
- **Slave**: Generates Phase B+C via ultra-low-latency UART link
- Better power efficiency, extensible architecture

**3. Single Master** - WiFi only
- Master generates only Phase A
- For scenarios needing dedicated signal generation boards

## Hardware Requirements

### ESP32 Board
- ESP32 DOIT DevKit v1 (or compatible)
- USB cable for programming & power

### Connections
```
GPIO25 (DAC1) → Phase A signal output
GPIO26 (DAC2) → Phase B signal output
GPIO32 (GPIO) → Phase C signal output (PWM or external DAC)

GND → Common ground with CT sensor circuit
```

### Optional: CT Sensor Burden Circuit
If integrating with existing CT circuits, add burden resistor:
```
DAC output → [16Ω resistor] → CT sensor secondary
                                        ├─ CT output
                                        └─ to ADC/comparator
```

## Configuration

Edit `include/config.h` to customize:

### WiFi Settings
```cpp
#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-password"
```

### MQTT Settings
```cpp
#define MQTT_SERVER "192.168.1.100"
#define MQTT_PORT 1883

#define MQTT_TOPIC_PHASE_A "home/power/phase_a/current"
#define MQTT_TOPIC_PHASE_B "home/power/phase_b/current"
#define MQTT_TOPIC_PHASE_C "home/power/phase_c/current"
```

### Signal Generation
```cpp
#define AC_FREQUENCY 50      // 50Hz (EU) or 60Hz (US)
#define CURRENT_MAX 100      // Maximum current (Amperes)
#define UPDATE_INTERVAL 20   // 20ms for 50Hz signal
```

**For Master-Slave Setup**, see [MASTER_SLAVE.md](MASTER_SLAVE.md)

## MQTT Interface

### Subscribe Topics (Inputs)
Firmware subscribes to and listens for RMS current values:

```
home/power/phase_a/current    → Current Phase A (float, Amperes)
home/power/phase_b/current    → Current Phase B (float, Amperes)
home/power/phase_c/current    → Current Phase C (float, Amperes)
```

**Example MQTT Publish:**
```bash
mosquitto_pub -h 192.168.1.100 -t home/power/phase_a/current -m "23.5"
mosquitto_pub -h 192.168.1.100 -t home/power/phase_b/current -m "21.2"
mosquitto_pub -h 192.168.1.100 -t home/power/phase_c/current -m "25.8"
```

### Publish Topics (Outputs)
```
myenergi/harvi/status         → Device status ("online"/"offline")
```

## Technical Details

### AC Signal Generation

The firmware generates 50Hz AC sine waves with amplitude proportional to RMS current:

1. **Three-Phase**: Outputs are phase-shifted by 120°
2. **DC Bias**: Output centered at 2.5V (128/255 of 5V DAC range) to represent 0A
3. **Amplitude**: ±2.5V max = ±100A RMS
4. **Frequency**: 50Hz or 60Hz (configurable)

**DAC Output Calculation:**
```
DAC_value = 128 + (sine × current_RMS × 128 / CURRENT_MAX)
```

- DAC range: 0-255 (8-bit)
- 128 = 0 Amperes (DC bias)
- 0-127 = Negative half-cycle
- 129-255 = Positive half-cycle

### Typical CT Sensor Specs
- **Model**: YHDC SCT-013-000 (or similar)
- **Turns Ratio**: 1000:1
- **Burden**: ~16Ω
- **Output**: 0-3V AC at rated current

## Building & Uploading

### Prerequisites
- PlatformIO installed
- ESP32 USB drivers installed

### Build

```bash
# Standalone (original single ESP32)
cd myenergi_CT-Clamp-Simulator
pio run -e esp32

# Master (WiFi + MQTT + Phase A)
pio run -e esp32-master

# Slave (Phase B + C via UART)
pio run -e esp32-slave
```

### Upload
```bash
pio run -e esp32 -t upload                # Standalone
pio run -e esp32-master -t upload         # Master
pio run -e esp32-slave -t upload          # Slave
```

### Monitor
```bash
pio run -e esp32 -t monitor               # Standalone
pio run -e esp32-master -t monitor        # Master
pio run -e esp32-slave -t monitor         # Slave
```

## Serial Output Example

```
=== Myenergi Harvi CT Simulator ===
Initializing...
✓ Signal generator initialized
✓ MQTT handler initialized
Connecting to WiFi: MyHome
✓ WiFi connected!
  IP: 192.168.1.50
Connecting to MQTT: 192.168.1.100:1883
✓ MQTT connected!
✓ Setup complete!

📡 MQTT [home/power/phase_a/current]: 23.50
📡 MQTT [home/power/phase_b/current]: 21.20
📡 MQTT [home/power/phase_c/current]: 25.80

--- Status Report ---
WiFi: ✓ Connected
MQTT: ✓ Connected
Phase A: 23.50A (DAC: 186)
Phase B: 21.20A (DAC: 180)
Phase C: 25.80A (DAC: 192)
```

## Circuit Diagram

```
                    ESP32
    ┌───────────────────────────────┐
    │                               │
    │  GPIO25 (DAC1) ─────────────┬─┤ ◄── AC Signal (Phase A)
    │                            │ │
    │                           [R]│ resistor ~16Ω (burden)
    │                            │ │
    │                            └─►─ To CT secondary
    │
    │  GPIO26 (DAC2) ─────────────┬─┤ ◄── AC Signal (Phase B)
    │                            │ │
    │                           [R]│
    │                            │ │
    │  GND ─────────────────────┴─┤ ◄── Ground
    │
    └───────────────────────────────┘
```

## Troubleshooting

### No MQTT output
- Check WiFi connection: Serial monitor shows `✓ WiFi connected`
- Verify MQTT broker is running and accessible
- Check topic names match configuration

### Incorrect signal amplitude
- Verify DAC pins are not used by other functions
- Check `CURRENT_MAX` configuration matches system expectations
- Measure DAC output with oscilloscope at different current levels

### Signal distortion
- Use shielded cables for DAC outputs
- Add 0.1µF capacitor to DAC output for noise filtering
- Keep DAC cables short and away from high-frequency signals

## Future Enhancements

- [ ] Web dashboard for configuration
- [ ] Over-the-air firmware updates
- [ ] Internal current calculation from power readings
- [ ] Signal harmonics simulation
- [ ] Phase correlation checking with actual grid
- [ ] SD card logging of signal data

## References

- [OpenEnergyMonitor - CT Sensors](https://docs.openenergymonitor.org/electricity-monitoring/ct-sensors/index.html)
- [YHDC SCT-013-000 Datasheet](https://www.yhdc.com/)
- [ESP32 Technical Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)

## License

MIT License - Feel free to modify and use as needed.

## Support

For issues, improvements, or questions, refer to the source repository.
