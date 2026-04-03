#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "mqtt_handler.h"
#include "signal_generator.h"
#include "uart_protocol.h"

// ============= Configuration =============
#define DEVICE_MODE_STANDALONE 0
#define DEVICE_MODE_MASTER 1
#define DEVICE_MODE_SLAVE 2

// Set device mode here:
#define DEVICE_MODE DEVICE_MODE_MASTER

// UART Configuration for Master-Slave Communication
#define SLAVE_UART_NUM 2
#define SLAVE_BAUD_RATE 460800
#define SLAVE_TX_PIN 17
#define SLAVE_RX_PIN 16

// ============= Global Objects =============
WiFiClient espClient;
MQTTHandler* gMQTT = nullptr;
ACSignalGenerator* gSignalGen = nullptr;

// Slave communication
HardwareSerial slaveSerial(SLAVE_UART_NUM);

// Timing
unsigned long lastLogTime = 0;
unsigned long lastSlaveCheckTime = 0;
#define SLAVE_TIMEOUT 1000  // milliseconds

// Slave status
bool slaveConnected = false;
unsigned long lastSuccessfulSlaveComm = 0;

// ============= Forward Declarations =============
void mqttMessageCallback(char* topic, uint8_t* payload, unsigned int length);
void setupWiFi();
void setupMQTT();
void setupSlave();
void communicateWithSlave();
void logStatus();

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(500);
    
    if (DEBUG_ENABLED) {
        Serial.println("\n\n=== Myenergi Harvi CT Simulator ===");
        Serial.print("Mode: ");
        #if DEVICE_MODE == DEVICE_MODE_MASTER
        Serial.println("MASTER (Phase A + MQTT)");
        #elif DEVICE_MODE == DEVICE_MODE_SLAVE
        Serial.println("SLAVE (Phase B, C via UART)");
        #else
        Serial.println("STANDALONE (All phases)");
        #endif
        Serial.println("Initializing...");
    }
    
    // Initialize signal generator
    gSignalGen = new ACSignalGenerator();
    gSignalGen->begin();
    
    if (DEBUG_ENABLED) {
        Serial.println("✓ Signal generator initialized");
    }
    
    #if DEVICE_MODE == DEVICE_MODE_MASTER || DEVICE_MODE == DEVICE_MODE_STANDALONE
    // Initialize MQTT (Master only)
    gMQTT = new MQTTHandler(espClient);
    gMQTT->setCallback(mqttMessageCallback);
    
    if (DEBUG_ENABLED) {
        Serial.println("✓ MQTT handler initialized");
    }
    
    // Setup WiFi
    setupWiFi();
    
    // Setup MQTT connection
    setupMQTT();
    #endif
    
    #if DEVICE_MODE == DEVICE_MODE_MASTER
    // Initialize Slave communication
    setupSlave();
    #endif
    
    #if DEVICE_MODE == DEVICE_MODE_SLAVE
    // Slave: Setup UART for receiving commands
    setupSlave();
    #endif
    
    if (DEBUG_ENABLED) {
        Serial.println("✓ Setup complete!\n");
    }
}

void loop() {
    #if DEVICE_MODE == DEVICE_MODE_MASTER || DEVICE_MODE == DEVICE_MODE_STANDALONE
    
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
    
    // Update Phase A signal locally
    gSignalGen->setCurrentPhaseA(gMQTT->getCurrentPhaseA());
    gSignalGen->update();
    
    #if DEVICE_MODE == DEVICE_MODE_MASTER
    // Communicate with Slave
    communicateWithSlave();
    #endif
    
    #endif
    
    #if DEVICE_MODE == DEVICE_MODE_SLAVE
    
    // Slave: Listen for commands from Master
    if (slaveSerial.available() >= UART_PACKET_SIZE) {
        uint8_t buffer[UART_PACKET_SIZE];
        slaveSerial.readBytes(buffer, UART_PACKET_SIZE);
        
        UARTPacket packet;
        float phaseB = 0, phaseC = 0;
        
        if (UARTProtocol::parsePacket(buffer, packet, phaseB, phaseC)) {
            gSignalGen->setCurrentPhaseB(phaseB);
            gSignalGen->setCurrentPhaseC(phaseC);
            slaveConnected = true;
            lastSuccessfulSlaveComm = millis();
            
            if (DEBUG_ENABLED && millis() - lastLogTime > LOG_INTERVAL) {
                Serial.print("Phase B: ");
                Serial.print(phaseB);
                Serial.print("A, Phase C: ");
                Serial.println(phaseC);
            }
        }
    }
    
    // Check for slave timeout
    if (millis() - lastSuccessfulSlaveComm > SLAVE_TIMEOUT) {
        slaveConnected = false;
    }
    
    // Update DAC output
    gSignalGen->update();
    
    #endif
    
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
    gMQTT->begin("myenergi_CT-Clamp-Simulator-Master");
    
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

void setupSlave() {
    #if DEVICE_MODE == DEVICE_MODE_MASTER
    
    slaveSerial.begin(SLAVE_BAUD_RATE, SERIAL_8N1, SLAVE_RX_PIN, SLAVE_TX_PIN);
    
    if (DEBUG_ENABLED) {
        Serial.print("Slave UART initialized: ");
        Serial.print(SLAVE_BAUD_RATE);
        Serial.println(" baud");
    }
    
    #elif DEVICE_MODE == DEVICE_MODE_SLAVE
    
    slaveSerial.begin(SLAVE_BAUD_RATE, SERIAL_8N1, SLAVE_RX_PIN, SLAVE_TX_PIN);
    
    if (DEBUG_ENABLED) {
        Serial.print("Slave UART listening: ");
        Serial.print(SLAVE_BAUD_RATE);
        Serial.println(" baud");
    }
    
    slaveConnected = true;
    lastSuccessfulSlaveComm = millis();
    
    #endif
}

void communicateWithSlave() {
    unsigned long now = millis();
    
    // Send commands to slave every update interval
    if (now - lastSlaveCheckTime >= UPDATE_INTERVAL) {
        lastSlaveCheckTime = now;
        
        // Create and send packet
        UARTPacket packet;
        float phaseB = gMQTT->getCurrentPhaseB();
        float phaseC = gMQTT->getCurrentPhaseC();
        
        UARTProtocol::createPacket(packet, phaseB, phaseC);
        
        uint8_t buffer[UART_PACKET_SIZE];
        UARTProtocol::serialize(packet, buffer, UART_PACKET_SIZE);
        
        slaveSerial.write(buffer, UART_PACKET_SIZE);
        
        if (DEBUG_ENABLED && now - lastLogTime > LOG_INTERVAL) {
            Serial.print("→ Slave: B=");
            Serial.print(phaseB);
            Serial.print("A, C=");
            Serial.print(phaseC);
            Serial.println("A");
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
        gMQTT->setCurrentPhaseB(value);
    } else if (strcmp(topic, MQTT_TOPIC_PHASE_C) == 0) {
        gMQTT->setCurrentPhaseC(value);
    }
}

void logStatus() {
    if (!DEBUG_ENABLED) return;
    
    Serial.println("\n--- Status Report ---");
    
    #if DEVICE_MODE == DEVICE_MODE_MASTER || DEVICE_MODE == DEVICE_MODE_STANDALONE
    Serial.print("WiFi: ");
    Serial.println(WiFi.isConnected() ? "✓ Connected" : "✗ Disconnected");
    
    Serial.print("MQTT: ");
    Serial.println(gMQTT->isConnected() ? "✓ Connected" : "✗ Disconnected");
    
    Serial.print("Phase A: ");
    Serial.print(gMQTT->getCurrentPhaseA());
    Serial.print("A (DAC: ");
    Serial.print(gSignalGen->getLastDACValueA());
    Serial.println(")");
    #endif
    
    #if DEVICE_MODE == DEVICE_MODE_MASTER
    Serial.print("Slave: ");
    Serial.println(slaveConnected ? "✓ Connected" : "✗ Offline");
    
    Serial.print("Phase B: ");
    Serial.print(gMQTT->getCurrentPhaseB());
    Serial.println("A");
    
    Serial.print("Phase C: ");
    Serial.print(gMQTT->getCurrentPhaseC());
    Serial.println("A");
    #endif
    
    #if DEVICE_MODE == DEVICE_MODE_SLAVE
    Serial.print("Master: ");
    Serial.println(slaveConnected ? "✓ Connected" : "✗ Offline");
    
    Serial.print("Phase B: ");
    Serial.print(gSignalGen->getLastDACValueB());
    Serial.print("A (DAC: ");
    Serial.print(gSignalGen->getLastDACValueB());
    Serial.println(")");
    
    Serial.print("Phase C: ");
    Serial.print(gSignalGen->getLastDACValueC());
    Serial.print("A (DAC: ");
    Serial.print(gSignalGen->getLastDACValueC());
    Serial.println(")");
    #endif
    
    Serial.println();
}
