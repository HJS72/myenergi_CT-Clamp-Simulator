#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "mqtt_handler.h"
#include "signal_generator.h"

// Global objects
WiFiClient espClient;
MQTTHandler* gMQTT = nullptr;
ACSignalGenerator* gSignalGen = nullptr;

// Forward declarations
void mqttMessageCallback(char* topic, uint8_t* payload, unsigned int length);
void setupWiFi();
void setupMQTT();
void logStatus();

// Timing
unsigned long lastLogTime = 0;

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(500);
    
    if (DEBUG_ENABLED) {
        Serial.println("\n\n=== Myenergi Harvi CT Simulator ===");
        Serial.println("Initializing...");
    }
    
    // Initialize signal generator
    gSignalGen = new ACSignalGenerator();
    gSignalGen->begin();
    
    if (DEBUG_ENABLED) {
        Serial.println("✓ Signal generator initialized");
    }
    
    // Initialize MQTT
    gMQTT = new MQTTHandler(espClient);
    gMQTT->setCallback(mqttMessageCallback);
    
    if (DEBUG_ENABLED) {
        Serial.println("✓ MQTT handler initialized");
    }
    
    // Setup WiFi
    setupWiFi();
    
    // Setup MQTT connection
    setupMQTT();
    
    if (DEBUG_ENABLED) {
        Serial.println("✓ Setup complete!\n");
    }
}

void loop() {
    // Maintain WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        if (DEBUG_ENABLED) {
            Serial.println("⚠ WiFi disconnected, attempting to reconnect...");
        }
        setupWiFi();
    }
    
    // Maintain MQTT connection
    if (!gMQTT->isConnected()) {
        gMQTT->reconnect();
    }
    
    // Process MQTT messages
    gMQTT->loop();
    
    // Update AC signal output
    gSignalGen->update();
    
    // Periodic status logging
    if (millis() - lastLogTime > LOG_INTERVAL) {
        logStatus();
        lastLogTime = millis();
    }
}

void setupWiFi() {
    if (DEBUG_ENABLED) {
        Serial.print("Connecting to WiFi: ");
        Serial.println(WIFI_SSID);
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
        delay(500);
        if (DEBUG_ENABLED) {
            Serial.print(".");
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        if (DEBUG_ENABLED) {
            Serial.println("\n✓ WiFi connected!");
            Serial.print("  IP: ");
            Serial.println(WiFi.localIP());
        }
    } else {
        if (DEBUG_ENABLED) {
            Serial.println("\n✗ WiFi connection failed!");
        }
    }
}

void setupMQTT() {
    gMQTT->begin("myenergi_CT-Clamp-Simulator-ESP32");
    
    if (DEBUG_ENABLED) {
        Serial.print("Connecting to MQTT: ");
        Serial.print(MQTT_SERVER);
        Serial.print(":");
        Serial.println(MQTT_PORT);
    }
    
    if (gMQTT->connect(MQTT_SERVER, MQTT_PORT)) {
        if (DEBUG_ENABLED) {
            Serial.println("✓ MQTT connected!");
        }
        gMQTT->subscribe();
        gMQTT->publishStatus("online");
    } else {
        if (DEBUG_ENABLED) {
            Serial.println("✗ MQTT connection failed!");
        }
    }
}

void mqttMessageCallback(char* topic, uint8_t* payload, unsigned int length) {
    // Null-terminate payload
    char message[256];
    if (length > 255) length = 255;
    strncpy(message, (char*)payload, length);
    message[length] = '\0';
    
    float value = atof(message);
    
    if (DEBUG_ENABLED) {
        Serial.print("📡 MQTT [");
        Serial.print(topic);
        Serial.print("]: ");
        Serial.println(value);
    }
    
    // Update signal generator with new current values
    if (strcmp(topic, MQTT_TOPIC_PHASE_A) == 0) {
        gSignalGen->setCurrentPhaseA(value);
    } else if (strcmp(topic, MQTT_TOPIC_PHASE_B) == 0) {
        gSignalGen->setCurrentPhaseB(value);
    } else if (strcmp(topic, MQTT_TOPIC_PHASE_C) == 0) {
        gSignalGen->setCurrentPhaseC(value);
    }
}

void logStatus() {
    if (!DEBUG_ENABLED) return;
    
    Serial.println("\n--- Status Report ---");
    Serial.print("WiFi: ");
    Serial.println(WiFi.isConnected() ? "✓ Connected" : "✗ Disconnected");
    
    Serial.print("MQTT: ");
    Serial.println(gMQTT->isConnected() ? "✓ Connected" : "✗ Disconnected");
    
    Serial.print("Phase A: ");
    Serial.print(gMQTT->getCurrentPhaseA());
    Serial.print("A (DAC: ");
    Serial.print(gSignalGen->getLastDACValueA());
    Serial.println(")");
    
    Serial.print("Phase B: ");
    Serial.print(gMQTT->getCurrentPhaseB());
    Serial.print("A (DAC: ");
    Serial.print(gSignalGen->getLastDACValueB());
    Serial.println(")");
    
    Serial.print("Phase C: ");
    Serial.print(gMQTT->getCurrentPhaseC());
    Serial.print("A (DAC: ");
    Serial.print(gSignalGen->getLastDACValueC());
    Serial.println(")");
    
    Serial.println();
}
