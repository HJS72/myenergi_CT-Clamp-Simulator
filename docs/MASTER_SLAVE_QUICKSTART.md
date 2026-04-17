# Master-Slave Setup - Quick Start

## Why Master-Slave?

```
✓ Master handles WiFi/MQTT (1 board with connectivity)
✓ Slave handles 2 DAC signals (dedicated signal generation)
✓ Ultra-low power slave (can run on batteries)
✓ Clean separation of concerns
```

## Hardware You Need

```
1x ESP32 (Master)  - WiFi module
1x ESP32 (Slave)   - Signal generation
3x USB cables (for programming/power)
3-wire UART cable (TX, RX, GND)
```

## Step 1: Prepare Master ESP32

1. Edit `include/config.h` - your WiFi & MQTT settings
2. Build:
   ```bash
   pio run -e esp32-master
   ```
3. Connect Master ESP32 via USB
4. Upload:
   ```bash
   pio run -e esp32-master -t upload
   ```
5. Verify:
   ```bash
   pio run -e esp32-master -t monitor
   ```
   Should show:
   ```
   Mode: MASTER (Phase A + MQTT)
   ✓ WiFi connected!
   ✓ MQTT connected!
   Slave UART initialized: 460800 baud
   ```

## Step 2: Prepare Slave ESP32

1. Build (no config needed):
   ```bash
   pio run -e esp32-slave
   ```
2. Connect Slave ESP32 via USB
3. Upload:
   ```bash
   pio run -e esp32-slave -t upload
   ```
4. Verify:
   ```bash
   pio run -e esp32-slave -t monitor
   ```
   Should show:
   ```
   Mode: SLAVE (Phase B, C via UART)
   Slave UART listening: 460800 baud
   ```

## Step 3: Wire Master ↔ Slave (UART)

```
Master ESP32          Slave ESP32
TX (GPIO17) ────────► RX (GPIO16)
RX (GPIO16) ◄──────── TX (GPIO17)
GND ───────────────── GND
```

**Use shielded cable** or 3 individual wires (twisted pair preferred):
- Short runs (<1m): Any wire works
- Medium runs (1-3m): Shielded recommended  
- Long runs (>3m): Lower baud rate (`#define SLAVE_BAUD_RATE 115200`)

## Step 4: Monitor Both (Optional)

**Terminal 1 - Master:**
```bash
pio run -e esp32-master -t monitor
```

**Terminal 2 - Slave (new terminal):**
```bash
pio run -e esp32-slave -t monitor
```

Watch for "Slave: ✓ Connected" in Master logs.

## Step 5: Test with MQTT

Send current values:
```bash
mkdir_pub -h 192.168.1.100 -t home/power/phase_a/current -m "25"
mqitto_pub -h 192.168.1.100 -t home/power/phase_b/current -m "23"
mosquitto_pub -h 192.168.1.100 -t home/power/phase_c/current -m "24"
```

**Master logs should show:**
```
📡 MQTT [home/power/phase_a/current]: 25.00
📡 MQTT [home/power/phase_b/current]: 23.00
📡 MQTT [home/power/phase_c/current]: 24.00
--- Status Report ---
Phase A: 25.00A (DAC: 195)
Phase B: 23.00A
Phase C: 24.00A
→ Slave: B=23.00A, C=24.00A
```

**Slave logs should show:**
```
Master: ✓ Connected
Phase B: 23.00A (DAC: 188)
Phase C: 24.00A (DAC: 193)
```

## Step 6: Connect to Harvi

Now connect DAC outputs:

**Master:**
```
GPIO25 (DAC1) → Harvi Phase A input
GND           → Harvi Ground
```

**Slave:**
```
GPIO25 (DAC1) → Harvi Phase C input
GPIO26 (DAC2) → Harvi Phase B input
GND           → Harvi Ground
```

## Done! 🎉

Your system is now:
- ✅ Receiving current data via MQTT (Master)
- ✅ Sending commands to Slave (UART)
- ✅ Generating AC signals for all 3 phases
- ✅ Harvi receives simulated current data

## Troubleshooting

### Slave shows "Master: ✗ Offline"

**Check:**
1. UART cable properly connected
2. Both devices powered on
3. Same baud rate in both (460800)
4. Try this: Is Master's Serial2 actually open?

**Debug:**
- Press RST button on Master - Slave should reconnect
- Check TX/RX pins aren't swapped
- Try lower baud rate: `#define SLAVE_BAUD_RATE 115200`

### Slave not receiving Phase B/C values

**Check:**
1. Master shows MQTT connected
2. Master shows correct Phase B/C in status report
3. Master shows "→ Slave: ..." in logs

**If not:**
- Verify MQTT broker is sending the values
- Check topic names match `config.h`

### DAC outputs not showing on oscilloscope

**Check:**
1. Slave shows "Master: ✓ Connected"
2. Slave shows Phase B/C values in status report
3. GPIO25/26 not used elsewhere

**Test:**
- Measure DAC pins with multimeter - should show ~1.65V at rest
- Verify with: `dacWrite(25, 128)` commands

## Power Management

**Master:** ~90mA (WiFi running)

**Slave:** 
- Normal: ~40mA (WiFi off)
- With deep sleep: ~10mA (optional feature)

For battery-powered slave, consider:
```cpp
esp_wifi_stop();           // No WiFi
esp_bt_disable();          // No Bluetooth  
setCpuFrequencyMhz(80);    // Lower CPU speed
```

## Next Steps

- See [MASTER_SLAVE.md](MASTER_SLAVE.md) for detailed configuration
- See [MQTT.md](MQTT.md) for publishing current data
- See [HARDWARE.md](HARDWARE.md) for signal verification with oscilloscope

---

**Questions?** Check [FAQ.md](FAQ.md) and [MASTER_SLAVE.md](MASTER_SLAVE.md)
