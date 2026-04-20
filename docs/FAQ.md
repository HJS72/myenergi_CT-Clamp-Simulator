# FAQ & Troubleshooting

## Frequently Asked Questions

### Q: What's the difference between this and using real CT sensors?

**A:** Real CT sensors:
- ✓ Measure actual current from the grid
- ✗ Expensive and complex installation
- ✗ Cannot simulate arbitrary values

This simulator:
- ✓ Simulates any current value via MQTT
- ✓ Easy testing and development
- ✓ Works without grid connection
- ✗ Requires MQTT data source

### Q: Can I use this with Zappi directly?

**A:** Not directly. Zappi expects signals from actual CT sensors on the mains cables. This simulator provides the signal path - you would need to:

1. Intercept the normal CT sensor output wires from Zappi
2. Disconnect them
3. Connect this simulator's DAC output instead
4. OR: Route both sensors and simulator output to same input

### Q: What current range does it support?

**A:** Default: -100A to +100A (configurable in `config.h` as `CURRENT_MAX`)

Changing the range:
```cpp
#define CURRENT_MAX 200    // For -200A..+200A range
```

### Q: Can I use GPIO32 for phase C PWM instead of DAC?

**A:** Yes! PWM mode is implemented but disabled by default:

```cpp
#define DAC_ENABLED false
#define PWM_ENABLED true  // Use PWM for GPIO32
```

Note: PWM signal needs RC filtering for smooth AC output.

### Q: What happens if MQTT connection drops?

**A:** The simulator automatically:
1. Detects disconnection
2. Retries every 5 seconds
3. Maintains last known current values on DAC
4. Continues outputting signals

### Q: Can I use this over WiFi mesh or weak signal?

**A:** MQTT is very efficient (minimal bandwidth), so it works on weak signals. However:

- Latency may be higher
- Connection timeouts may occur
- Consider wired Ethernet bridge for reliability

### Q: Is this safe? Can it damage my Harvi?

**A:** Yes, designed to be safe:

- ✓ Signals are low voltage (0-3.3V)
- ✓ Proper impedance matching via burden resistor
- ✓ Galvanically isolated approach
- ⚠ Do NOT connect to mains voltage
- ⚠ Proper grounding is critical

### Q: Can I monitor multiple Harvi devices?

**A:** Yes! Deploy multiple ESP32s:

Each device needs unique topic names:

```
Device 1:
  home/power/harvi1/phase_a/current
  home/power/harvi1/phase_b/current

Device 2:
  home/power/harvi2/phase_a/current
  home/power/harvi2/phase_b/current
```

## Troubleshooting

### Serial Monitor Shows No Output

**Problem:** Nothing appears after upload

**Solutions:**
1. Check USB cable is properly connected
2. Verify correct COM port: `pio device list`
3. Manual reset: Press RST button on ESP32
4. Check power: LED should be lit

### WiFi Connection Fails

**Problem:** Shows "✗ WiFi connection failed!"

**Check:**
- WiFi SSID is correct (case-sensitive)
- WiFi password is correct
- 2.4 GHz band is available
- Distance from router (<5m recommended)

**Try:**
```cpp
WiFi.setAutoConnect(true);
WiFi.setAutoReconnect(true);
```

### MQTT Connection Fails

**Problem:** Shows "✗ MQTT connection failed!"

**Check:**
```bash
# Is broker running?
mosquitto_pub -h 192.168.1.100 -t test -m "hello"

# Can ESP32 reach broker?
pio run -e esp32 -t monitor  # Check serial for "✓ WiFi connected"

# Is port 1883 open?
sudo netstat -tulpn | grep 1883
```

**Try:**
- Change MQTT_SERVER to IP instead of hostname
- Increase MQTT_TIMEOUT in config.h
- Check firewall isn't blocking port 1883

### No DAC Output

**Problem:** GPIO25/26 stay at 0V

**Check:**
```cpp
// Verify DAC_ENABLED = true
#define DAC_ENABLED true

// Check MQTT messages are received
// Serial should show: "📡 MQTT [/esp32CTSimulator/PhaseA_Amp]: X.XX"
```

**Test:**
```cpp
// Add manual test in setup():
dacWrite(25, 128);   // Should output ~1.65V
```

### Signal Distorted or Noisy

**Problem:** Oscilloscope shows noisy/distorted sine wave

**Solutions:**
1. Add 0.1µF capacitor across burden resistor
2. Use shielded cable for signal
3. Keep DAC output cable <30cm
4. Verify power supply is stable (5V ±0.1V)

### Harvi Shows Erratic Values

**Problem:** Current readings jump around

**Check:**
- Phase shifts: Should be exactly 120° apart
- Frequency: Should be exactly 50Hz or 60Hz
- Signal amplitude: Should match Harvi's CT sensor specs

**Debug:**
- Record oscilloscope data
- Check MQTT update frequency
- Verify no EMI from nearby equipment

### Memory Issues (OOM)

**Problem:** Device resets or becomes unstable

**Solutions:**
- Reduce MQTT_MAX_PACKET_SIZE in platformio.ini
- Disable DEBUG_ENABLED if not needed
- Use `pio run -e esp32 -t monitor` to check stack

## Performance Tips

### Optimize for Low Latency

```cpp
#define UPDATE_INTERVAL 10       // 10ms for faster response
#define MQTT_RECONNECT_INTERVAL 1000  // Try reconnect more often
```

### Optimize for Low Power

```cpp
#define UPDATE_INTERVAL 100      // 100ms slows down updates
#define DEBUG_ENABLED false      // Disable serial output
WiFi.setSleep(WIFI_PS_MIN_MODEM);  // Reduce WiFi power
```

### Optimize for Low Memory

```cpp
#define MQTT_MAX_PACKET_SIZE 512  // Smaller buffer
#define SIGNAL_SAMPLES 128        // Fewer samples per cycle
```

## Advanced Debugging

### Enable Verbose MQTT Logging

Add to `mqtt_handler.cpp`:

```cpp
void MQTTHandler::loop() {
    if (!client.connected()) {
        Serial.println("MQTT disconnected!");
    }
    client.loop();
}
```

### Monitor Heap Memory

```cpp
Serial.print("Free heap: ");
Serial.println(ESP.getFreeHeap());
```

### Check WiFi Signal Strength

```cpp
Serial.print("RSSI: ");
Serial.println(WiFi.RSSI());  // -30 to -90 dBm
```

## Still Having Issues?

1. **Check the logs**: Enable `DEBUG_ENABLED true` and review serial output
2. **Consult documentation**: See [README.md](README.md) and [HARDWARE.md](HARDWARE.md)
3. **Review MQTT setup**: See [MQTT.md](MQTT.md)
4. **Community resources**: OpenEnergyMonitor forums for CT sensor questions

---

**Last updated:** 2026-04-03
