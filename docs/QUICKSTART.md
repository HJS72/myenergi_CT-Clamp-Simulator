# Quick Start Guide

Get your Myenergi Harvi Simulator up and running in 5 minutes!

## Prerequisites

- ESP32 DOIT DevKit v1 board
- USB cable (USB-A to Micro USB)
- WiFi network with IP-based MQTT broker
- Computer with PlatformIO CLI or VS Code with PlatformIO extension

## Step 1: Configure WiFi & MQTT (2 min)

Edit `include/config.h`:

```cpp
// WiFi
#define WIFI_SSID "MyHome"
#define WIFI_PASSWORD "mypassword123"

// MQTT
#define MQTT_SERVER "192.168.1.100"      // IP of your MQTT broker
#define MQTT_PORT 1883
```

## Step 2: Build & Upload (2 min)

### Option A: Using VS Code + PlatformIO

1. Open the project in VS Code
2. Click **PlatformIO: Upload** in the bottom bar
3. Connect ESP32 via USB
4. Watch the build and upload

### Option B: Using Terminal

```bash
cd myenergi_CT-Clamp-Simulator
pio run -e esp32 -t upload
```

## Step 3: Verify Connection (1 min)

### Monitor Serial Output

```bash
pio run -e esp32 -t monitor
```

You should see:

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
```

## Step 4: Test with MQTT (Optional, 1 min)

Send test current values:

```bash
# Terminal 1: Monitor MQTT messages
mosquitto_sub -h 192.168.1.100 -t "myenergi/harvi/#"

# Terminal 2: Publish current values
mosquitto_pub -h 192.168.1.100 -t "home/power/phase_a/current" -m "25"
mosquitto_pub -h 192.168.1.100 -t "home/power/phase_b/current" -m "23"
mosquitto_pub -h 192.168.1.100 -t "home/power/phase_c/current" -m "24"
```

Watch the serial monitor - you should see:

```
📡 MQTT [home/power/phase_a/current]: 25.00
--- Status Report ---
Phase A: 25.00A (DAC: 195)
Phase B: 23.00A (DAC: 191)
Phase C: 24.00A (DAC: 193)
```

## Step 5: Hardware Connection

Connect GPIO pins to your Harvi/Zappi system:

```
ESP32 GPIO25 (DAC1) ─→ Phase A CT Signal Input
ESP32 GPIO26 (DAC2) ─→ Phase B CT Signal Input
ESP32 GPIO32         ─→ Phase C CT Signal Input (or PWM)
ESP32 GND            ─→ Ground
```

See [Hardware Setup](docs/HARDWARE.md) for detailed wiring.

## Troubleshooting

### ✗ ESP32 doesn't connect to WiFi

**Solution:**
- Verify WiFi SSID and password in `config.h`
- Check that 2.4GHz WiFi is available (not 5GHz only)
- Restart the ESP32

### ✗ MQTT connection fails

**Solution:**
- Verify MQTT broker IP is correct: `ping 192.168.1.100`
- Check broker is running: `mosquitto_pub -h 192.168.1.100 -t test -m test`
- Verify firewall allows port 1883

### ✗ No signal output on GPIO pins

**Solution:**
- Verify ESP32 is receiving MQTT messages (check serial monitor)
- Test DAC with multimeter - should show ~1.65V at rest
- Confirm GPIO25/26 are not used elsewhere

### ✗ Signal appears but Harvi doesn't recognize it

**Solution:**
- Check signal frequency is 50Hz or 60Hz
- Verify amplitude matches settings
- Check phase shifts are correct (120° between phases)
- Add burden resistor (16Ω) if using with actual CT sensor

## Next Steps

- **Fine-tuning**: Adjust `CURRENT_MAX` and phase offsets in `config.h`
- **Automation**: Set up Home Assistant automations to feed current data
- **Monitoring**: View live current values on dashboard
- **Advanced**: See [MQTT Configuration](docs/MQTT.md) for publish scripts

## Support

- Full documentation in `/docs`
- Hardware wiring details: [HARDWARE.md](docs/HARDWARE.md)
- MQTT setup guide: [MQTT.md](docs/MQTT.md)
- Troubleshooting: [README.md](docs/README.md#troubleshooting)

---

**Ready?** Start with Step 1 above! 🚀
