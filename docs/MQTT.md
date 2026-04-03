# MQTT Configuration Guide

## Overview

The Myenergi Harvi Simulator communicates with external systems via MQTT. It receives RMS current values and outputs corresponding AC signals through DAC pins.

## MQTT Topics

### Input Topics (Subscribed by ESP32)

The simulator subscribes to three topics for receiving RMS current values:

```
home/power/phase_a/current    - Phase A current (Amperes)
home/power/phase_b/current    - Phase B current (Amperes)
home/power/phase_c/current    - Phase C current (Amperes)
```

**Payload Format:**
- Type: Float or Integer
- Unit: Amperes (A)
- Range: 0 - CURRENT_MAX (default: 0-100A)
- Update Frequency: As needed (can be 1Hz or higher)

**Example:**
```json
Topic: home/power/phase_a/current
Payload: 23.5
```

### Output Topics (Published by ESP32)

The simulator publishes status information:

```
myenergi/harvi/status         - Device status and heartbeat
```

**Payload Format (JSON):**
```json
{
  "status": "online",
  "timestamp": 1700000000
}
```

## MQTT Configuration

### In `config.h`

Customize MQTT settings:

```cpp
// Broker connection
#define MQTT_SERVER "192.168.1.100"      // IP or hostname
#define MQTT_PORT 1883                    // Default MQTT port

// Input topics
#define MQTT_TOPIC_PHASE_A "home/power/phase_a/current"
#define MQTT_TOPIC_PHASE_B "home/power/phase_b/current"
#define MQTT_TOPIC_PHASE_C "home/power/phase_c/current"

// Connection parameters
#define MQTT_TIMEOUT 5000                 // Connection timeout
#define MQTT_RECONNECT_INTERVAL 5000      // Reconnection retry interval
```

## Setting Up MQTT Broker

### Mosquitto (Linux/Mac)

**Installation:**
```bash
# macOS
brew install mosquitto

# Linux (Ubuntu/Debian)
sudo apt install mosquitto

# Linux (Fedora/RHEL)
sudo dnf install mosquitto
```

**Start broker:**
```bash
# macOS
brew services start mosquitto

# Linux
sudo systemctl start mosquitto
```

**Configuration file:** `/etc/mosquitto/mosquitto.conf` (Linux) or `/usr/local/etc/mosquitto/mosquitto.conf` (macOS)

### Docker

```bash
docker run -d --name mosquitto -p 1883:1883 eclipse-mosquitto
```

### Home Assistant

If using Home Assistant, MQTT is already available on port 1883.

## Testing MQTT Connection

### Using mosquitto_pub/sub

**Subscribe to status:**
```bash
mosquitto_sub -h 192.168.1.100 -t "myenergi/harvi/status"
```

**Publish current value:**
```bash
mosquitto_pub -h 192.168.1.100 -t "home/power/phase_a/current" -m "25.5"
```

### Using Python

```python
import paho.mqtt.client as mqtt

def on_message(client, userdata, msg):
    print(f"{msg.topic}: {msg.payload.decode()}")

client = mqtt.Client()
client.on_message = on_message
client.connect("192.168.1.100", 1883)
client.subscribe("myenergi/harvi/status")
client.loop_forever()
```

## Publishing Current Values

### From Home Assistant

Create an automation to publish current values:

```yaml
automation:
  - alias: "Send power to Harvi simulator"
    trigger:
      platform: time_pattern
      seconds: "/5"  # Update every 5 seconds
    action:
      service: mqtt.publish
      data:
        topic: "home/power/phase_a/current"
        payload_template: "{{ (states('sensor.phase_a_current') | float(0)) | round(2) }}"
```

### From Python

```python
import paho.mqtt.client as mqtt
import time
import math

client = mqtt.Client()
client.connect("192.168.1.100", 1883)

# Publish a simple value
client.publish("home/power/phase_a/current", 25.5)

# Publish from a sensor
while True:
    # Simulate sine wave from sensor
    value = 50 + 30 * math.sin(time.time())
    client.publish("home/power/phase_a/current", f"{value:.2f}")
    time.sleep(1)
```

### From Node.js

```javascript
const mqtt = require('mqtt');
const client = mqtt.connect('mqtt://192.168.1.100');

client.on('connect', () => {
  console.log('Connected');
  client.publish('home/power/phase_a/current', '25.5');
});
```

## Message Flow Diagram

```
┌─────────────────────────┐
│   Power Meter/Sensor    │
│   (e.g., Shelly, ...)   │
└────────────┬────────────┘
             │
             │ MQTT Publish
             ↓
    ┌────────────────┐
    │  MQTT Broker   │
    │ (Mosquitto)    │
    └────────┬───────┘
             │
             │ MQTT Subscribe
             ↓
    ┌────────────────────┐
    │   ESP32 Simulator  │
    │ ├─ MQTT Handler   │
    │ ├─ DAC Generator  │
    │ └─ Signal Output  │ ──→ To Harvi CT Sensors
    └────────────────────┘
```

## Troubleshooting

### ESP32 Can't Connect to MQTT

1. **Check broker accessibility:**
   ```bash
   ping 192.168.1.100
   ```

2. **Test with mosquitto_pub:**
   ```bash
   mosquitto_pub -h 192.168.1.100 -p 1883 -t test -m "hello"
   ```

3. **Check firewall:**
   ```bash
   # macOS
   sudo lsof -i :1883
   
   # Linux
   sudo netstat -tulpn | grep 1883
   ```

4. **Enable debug in config.h:**
   ```cpp
   #define DEBUG_ENABLED true
   ```

### Messages Not Being Received

1. **Verify topic names exactly match** (case-sensitive)
2. **Check topic format in serial monitor**
3. **Ensure payload is valid number (float/int)**

### High Latency or Dropped Messages

1. **Increase MQTT buffer size in platformio.ini:**
   ```ini
   build_flags =
       -D MQTT_MAX_PACKET_SIZE=2048
   ```

2. **Reduce publish frequency if flooding**

3. **Increase heartbeat interval:**
   ```cpp
   #define MQTT_RECONNECT_INTERVAL 10000
   ```

## Advanced: MQTT with Authentication

### Configure Broker for Authentication

**mosquitto.conf:**
```
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
```

**Create user:**
```bash
mosquitto_passwd -c /etc/mosquitto/passwd username
```

### Configure ESP32

```cpp
// In mqtt_handler.cpp, modify connect():
bool MQTTHandler::connect(const char* server, uint16_t port) {
    return client.connect("myenergi_CT-Clamp-Simulator", "username", "password");
}
```

## Performance Optimization

### Publish Rate

Current optimal rates by use case:

| Use Case | Rate | Interval |
|----------|------|----------|
| UI dashboard | 1 Hz | 1000ms |
| Energy calculation | 2 Hz | 500ms |
| Reactive control | 5 Hz | 200ms |
| High precision | 10 Hz+ | 100ms |

**Configure in code:**
```cpp
#define MQTT_UPDATE_INTERVAL 500  // 2 Hz update rate
```

### Network Bandwidth

Example at 2 Hz update rate:
- Message size: ~20 bytes per topic × 3 = 60 bytes
- Bandwidth: 60 × 2 = 120 bytes/sec ≈ 1 kbps
- Very efficient on typical home networks

## References

- [MQTT 3.1.1 Specification](https://mqtt.org/mqtt-specification-v3-1-1)
- [Mosquitto Documentation](https://mosquitto.org/documentation/)
- [PubSubClient Library](https://github.com/knolleary/pubsubclient)
