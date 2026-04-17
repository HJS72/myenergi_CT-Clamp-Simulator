#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============= WiFi Configuration =============
#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-password"
#define WIFI_AP_SSID "CTSimulator"
#define WIFI_AP_PASSWORD "CTSimulator"
#define WIFI_TIMEOUT 10000  // milliseconds

// ============= MQTT Configuration =============
#define MQTT_SERVER "192.168.1.100"
#define MQTT_PORT 1883
#define MQTT_RECONNECT_INTERVAL 5000  // milliseconds
#define MQTT_TIMEOUT 5000

// MQTT Topics for current values (RMS Amperes)
#define MQTT_TOPIC_PHASE_A "home/power/phase_a/current"
#define MQTT_TOPIC_PHASE_B "home/power/phase_b/current"
#define MQTT_TOPIC_PHASE_C "home/power/phase_c/current"

// ============= DAC/Signal Generation =============
// ESP32 DAC pins: GPIO25 (DAC1), GPIO26 (DAC2)
#define DAC_PIN_PHASE_A 25  // Pin for Phase A signal
#define DAC_PIN_PHASE_B 26  // Pin for Phase B signal
#define DAC_PIN_PHASE_C 32  // GPIO32 (for PWM if needed)

// Signal parameters
#define DAC_ENABLED true
#define PWM_ENABLED false    // Alternative: use PWM instead of DAC

// AC Signal generation
#define AC_FREQUENCY 50      // Hz (European standard)
#define SIGNAL_SAMPLES 256   // Samples per AC period for smooth waveform
#define UPDATE_INTERVAL 20   // milliseconds (50Hz = 20ms per cycle)

// Current range
#define CURRENT_MAX 100          // Maximum expected current (Amperes)
#define DAC_MAX_VALUE 255        // 8-bit DAC resolution
#define CURRENT_DEFAULT -99.0f   // Sentinel: no MQTT value received yet (-99A = no data/no connection)

// Phase shift (120 degrees for 3-phase)
#define PHASE_A_OFFSET 0     // 0 degrees
#define PHASE_B_OFFSET 120   // 120 degrees
#define PHASE_C_OFFSET 240   // 240 degrees

// ============= Logging & Debug =============
#define DEBUG_ENABLED true
#define SERIAL_BAUD 115200
#define LOG_INTERVAL 5000    // milliseconds

// ============= Hardware Specifics =============
// Burden resistor configuration (typical for CT sensors)
#define BURDEN_RESISTOR 16   // Ohms (typical for YHDC SCT-013)
#define CT_TURNS_RATIO 1000  // 1000:1 for SCT-013-000
#define AMP_TO_DAC_RATIO (DAC_MAX_VALUE / CURRENT_MAX)

// ============= OLED Display (I2C SH1106) =============
#define OLED_ENABLED true
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_I2C_ADDRESS 0x3C
#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define OLED_UPDATE_INTERVAL 500  // milliseconds

// ============= Sum Graph History =============
// Ring buffer: 128 samples over 5 minutes, resampled to the graph width on display
#define GRAPH_SAMPLES     128
#define GRAPH_INTERVAL_MS 2344

#endif // CONFIG_H
