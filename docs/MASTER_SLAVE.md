# Master-Slave Configuration Guide

## Overview

The Myenergi Harvi Simulator supports three deployment modes:

### **1. Standalone (Single ESP32)**
- ✅ WiFi + MQTT
- ✅ All three phases (A, B, C) generated
- ✅ Simplest setup
- ❌ Limited by 2 DAC pins on ESP32

### **2. Master (Master ESP32)**
- ✅ WiFi + MQTT
- ✅ Generates Phase A locally
- ✅ Sends Phase B, C commands to Slave
- ✅ Extensible (can control multiple slaves)

### **3. Slave (Slave ESP32)**
- ✅ Generates Phase B + C locally
- ✅ Listens for commands from Master via UART
- ✅ Ultra-low power (WiFi disabled)
- ✅ Dedicated signal generation

---

## Hardware Setup

### Master-Slave Communication (UART)

```
Master ESP32                Slave ESP32
┌──────────────────┐      ┌──────────────────┐
│ GPIO17 (TX) ────────────► GPIO16 (RX)      │
│ GPIO16 (RX) ◄────────────── GPIO17 (TX)    │
│ GND ─────────────────────── GND             │
│                          │                  │
│ + GPIO25 (Phase A)       │ + GPIO25 (Phase C)
│ + WiFi/MQTT              │ + GPIO26 (Phase B)
│                          │ + Low Power Mode  │
└──────────────────┘      └──────────────────┘
```

### Pin Connections

**Master ESP32:**
```
GPIO25 (DAC1)  → Phase A output (to Harvi)
GPIO26 (DAC2)  → (unused in master mode)
GPIO17 (TX)    → Slave GPIO16 (RX)
GPIO16 (RX)    → Slave GPIO17 (TX)
GND            → Common ground with Slave
```

**Slave ESP32:**
```
GPIO25 (DAC1)  → Phase C output (to Harvi)
GPIO26 (DAC2)  → Phase B output (to Harvi)
GPIO16 (RX)    → Master GPIO17 (TX)
GPIO17 (TX)    → Master GPIO16 (RX)
GND            → Common ground with Master
```

**Serial pass-through OTA control lines (required for slave flashing via master web UI):**
```
Master GPIO32  → Slave EN
Master GPIO33  → Slave GPIO0 (BOOT)
```

Recommended hardware implementation:
- Keep 10k pull-up resistors on Slave EN and Slave GPIO0 to 3V3.
- Use open-drain transistor stages from master control GPIOs to avoid line contention.

### Cable Specification

For UART connection:
- **Type**: Shielded twisted pair or individual shielded wires
- **Length**: ≤3 meters recommended
- **Gauge**: 22-24 AWG
- **Baud Rate**: 460800 (high speed, minimize latency)
- **Configuration**: 8-bit, no parity, 1 stop bit (8N1)

---

## Configuration

### Select Deployment Mode

Edit `src/master_slave_main.cpp`:

```cpp
// Select ONE:
#define DEVICE_MODE DEVICE_MODE_STANDALONE   // Single ESP32
#define DEVICE_MODE DEVICE_MODE_MASTER       // Master (Phase A + MQTT)
#define DEVICE_MODE DEVICE_MODE_SLAVE        // Slave (Phase B, C)
```

### UART Settings (Optional)

```cpp
#define SLAVE_UART_NUM 2          // UART2 on ESP32
#define SLAVE_BAUD_RATE 460800    // High speed communication
#define SLAVE_TX_PIN 17           // Master transmit
#define SLAVE_RX_PIN 16           // Master receive
```

Lower baud rates for longer cables:
```cpp
#define SLAVE_BAUD_RATE 115200    // For 3+ meter cables
```

---

## Communication Protocol

Master and Slave communicate via a simple binary protocol:

### Packet Structure (8 bytes)

```
Byte 0:   SYNC      0x55        (start marker)
Byte 1:   LEN       0x02        (payload length)
Byte 2:   PHASE_B   0x00-0xFF   (0-100A mapped to 0-255)
Byte 3:   PHASE_C   0x00-0xFF   (0-100A mapped to 0-255)
Byte 4:   CRC_LOW   ........     (CRC16 low byte)
Byte 5:   CRC_HIGH  ........     (CRC16 high byte)
Byte 6:   END       0xAA        (end marker)
Byte 7:   PAD       0x00        (reserved)
```

### Example Data Flow

```
MQTT Input:
  home/power/phase_a/current → 25.5A
  home/power/phase_b/current → 23.0A
  home/power/phase_c/current → 24.8A

Master ←MQTT→ Broker
  ↓
  Generates Phase A (25.5A) locally
  ↓
  Creates packet: [0x55, 0x02, 0xEB, 0xE0, CRC, CRC, 0xAA, 0x00]
                  (23A, 24.8A mapped to bytes)
  ↓
  Sends packet to Slave via UART
  ↓
Slave ←UART← Master
  ↓
  Receives and validates packet
  ↓
  Generates Phase B (23A) on GPIO26
  Generates Phase C (24.8A) on GPIO25
```

---

## Building & Deployment

### Build All Targets

```bash
# Standalone (original single ESP32)
pio run -e esp32

# Master (with MQTT, Phase A only)
pio run -e esp32-master

# Slave (UART only, Phase B + C)
pio run -e esp32-slave
```

### Upload to Different Boards

**Master first (needs MQTT config):**
```bash
pio run -e esp32-master -t upload
# Connect to Master ESP32 via USB
# Configure WiFi/MQTT as usual
```

**Then Slave (just needs UART):**
```bash
pio run -e esp32-slave -t upload
# Connect to Slave ESP32 via USB
# No configuration needed
```

### Verify Connections

**Master serial monitor:**
```bash
pio run -e esp32-master -t monitor
```

Expected output:
```
=== Myenergi Harvi CT Simulator ===
Mode: MASTER (Phase A + MQTT)
...
✓ WiFi connected! IP: 192.168.1.50
✓ MQTT connected!
Slave UART initialized: 460800 baud
✓ Setup complete!

--- Status Report ---
WiFi: ✓ Connected
MQTT: ✓ Connected
Slave: ✓ Connected
Phase A: 25.50A (DAC: 195)
Phase B: 23.00A
Phase C: 24.80A
```

**Slave serial monitor:**
```bash
pio run -e esp32-slave -t monitor
```

Expected output:
```
=== Myenergi Harvi CT Simulator ===
Mode: SLAVE (Phase B, C via UART)
...
Slave UART listening: 460800 baud
✓ Setup complete!

--- Status Report ---
Master: ✓ Connected
Phase B: 23.00A (DAC: 188)
Phase C: 24.80A (DAC: 193)
```

---

## Troubleshooting

### Slave Not Connecting

**Check:**
1. Terminal connection physically wired correctly
2. Baud rates match on both devices
3. TX/RX pins not swapped

**Test UART with both monitors open:**
```bash
# Terminal 1: Master monitor
pio run -e esp32-master -t monitor

# Terminal 2: Slave monitor  
pio run -e esp32-slave -t monitor
```

Should show "✓ Connected" within 5 seconds on Slave.

### Invalid CRC Errors

**Check:**
1. UART cable shield integrity
2. No EMI interference nearby
3. Baud rate too high for cable length

**Try:**
```cpp
#define SLAVE_BAUD_RATE 115200  // Lower baud for reliability
```

### Slave Timeouts

**Check:**
- Master is running and MQTT receiving values
- Look for `Slave: ✗ Offline` in master logs

**Timeout configured as:**
```cpp
#define SLAVE_TIMEOUT 1000  // milliseconds
```

If slave repeatedly times out, increase timeout:
```cpp
#define SLAVE_TIMEOUT 2000  // More lenient
```

### Phase Values Not Updating on Slave

**Check:**
1. MQTT topics are sending values to Master
2. Master shows correct Phase B/C values in status report
3. Slave is receiving packets (add debug logging)

**Debug: Enable verbose logging**
```cpp
if (DEBUG_ENABLED && millis() - lastLogTime > LOG_INTERVAL) {
    Serial.print("→ Slave: B=");
    Serial.print(phaseB);
    Serial.print("A, C=");
    Serial.print(phaseC);
    Serial.println("A");
}
```

---

## Performance Characteristics

| Metric | Standalone | Master | Slave |
|--------|-----------|--------|-------|
| **WiFi** | Yes | Yes | No |
| **MQTT** | Yes | Yes | No |
| **DAC Outputs** | A, B, C | A only | B, C |
| **Power Usage** | ~80mA | ~90mA | ~40mA |
| **UART Latency** | - | <1ms | <1ms |
| **Total Latency** | ~MQTT latency | MQTT + <1ms | <1ms |
| **Update Rate** | 50Hz | 50Hz | 50Hz |

---

## Power Management (Slave)

The Slave ESP32 can be powered very efficiently:

```cpp
// In slave_main.cpp setup():
esp_wifi_stop();                    // Disable WiFi
esp_bt_controller_disable();        // Disable Bluetooth
esp_sleep_enable_timer_wakeup(...); // Optional: deep sleep modes

// Current consumption:
// - WiFi enabled: ~80mA
// - WiFi disabled: ~40mA
// - Deep sleep: ~10mA
```

---

## Scaling to Multiple Slaves

The current design supports 1 Master + 1 Slave. To scale:

```cpp
// Future: Master with multiple slaves
#define NUM_SLAVES 2

// Each slave on separate UART:
HardwareSerial slave1(UART_NUM_1);
HardwareSerial slave2(UART_NUM_2);

// Round-robin communication
void communicateWithSlaves() {
    sendToSlave(slave1, phaseB, phaseC_unused);
    sendToSlave(slave2, phaseBx, phaseCx);
}
```

ESP32 has 3 UARTs, so max 2 slaves (UART0 reserved for serial debug).

---

## Quick Reference

### Start Standalone
```bash
pio run -e esp32 -t upload
```

### Start Master-Slave Pair
```bash
# Master
pio run -e esp32-master -t upload

# Slave  
pio run -e esp32-slave -t upload
```

### Monitor Connectivity
```bash
# Master
pio run -e esp32-master -t monitor | grep "Slave:"

# Slave
pio run -e esp32-slave -t monitor | grep "Master:"
```

---

**Last updated:** 2026-04-03
