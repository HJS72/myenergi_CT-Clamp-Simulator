#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "mqtt_handler.h"
#include "signal_generator.h"
#include "uart_protocol.h"

// ============= Configuration =============
#define DEVICE_MODE_STANDALONE 0
#define DEVICE_MODE_MASTER 1
#define DEVICE_MODE_SLAVE 2

// DEVICE_MODE is set via build flag: -D DEVICE_MODE=1 (master) or -D DEVICE_MODE=2 (slave)
// Falls back to master if not set by build system
#ifndef DEVICE_MODE
#define DEVICE_MODE DEVICE_MODE_MASTER
#endif

// UART Configuration for Master-Slave Communication
#define SLAVE_UART_NUM 2
#ifdef WOKWI_SIM
#define SLAVE_BAUD_RATE 115200
#else
#define SLAVE_BAUD_RATE 460800
#endif
#define SLAVE_TX_PIN 17
#define SLAVE_RX_PIN 16
#define ROLE_STRAP_PIN 27  // Wokwi fallback: LOW => force slave behavior
#define UART_ACK_BYTE 0x4B  // 'K' = slave alive acknowledgment
#define UART_HEARTBEAT_BYTE 0x48  // 'H' = periodic slave heartbeat

// ============= Global Objects =============
WiFiClient espClient;
MQTTHandler* gMQTT = nullptr;
ACSignalGenerator* gSignalGen = nullptr;
Adafruit_SSD1306* gDisplay = nullptr;
bool gDisplayReady = false;
bool gAccessPointMode = false;
bool gForceSlaveRole = false;  // Wokwi fallback: board without OLED acts as slave

// Slave communication
HardwareSerial slaveSerial(SLAVE_UART_NUM);

// Timing
unsigned long lastLogTime = 0;
unsigned long lastSlaveCheckTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastSlaveHeartbeatTx = 0;
#define SLAVE_TIMEOUT 1000  // milliseconds

// Slave status
bool slaveConnected = false;
unsigned long lastSuccessfulSlaveComm = 0;
float slavePhaseB = CURRENT_DEFAULT;
float slavePhaseC = CURRENT_DEFAULT;
uint32_t slaveAckCount = 0;
uint32_t slaveHeartbeatCount = 0;
uint32_t slaveRxByteCount = 0;
unsigned long virtualSlaveHeartbeatTime = 0;
bool virtualSlaveFallbackActive = false;

// Sum graph ring-buffer (one sample every GRAPH_INTERVAL_MS, 15-min window)
static float graphHistory[GRAPH_SAMPLES];
static uint8_t graphHead = 0;
static unsigned long lastGraphSample = 0;

// ============= Forward Declarations =============
void mqttMessageCallback(char* topic, uint8_t* payload, unsigned int length);
void setupWiFi();
void setupMQTT();
void setupSlave();
static void drawSumGraph(Adafruit_SSD1306* disp);
static bool hasStationWiFiConfig();
static void startFallbackAccessPoint();
static void runSlaveRoleLoop();
void communicateWithSlave();
void logStatus();
void setupDisplay();
void updateDisplay();

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

    // Initialize graph history
    for (int i = 0; i < GRAPH_SAMPLES; i++) graphHistory[i] = CURRENT_DEFAULT;

    if (DEBUG_ENABLED) {
        Serial.println("✓ Signal generator initialized");
    }

    setupDisplay();

    #if DEVICE_MODE == DEVICE_MODE_MASTER && defined(WOKWI_SIM)
    // Deterministic role strap for Wokwi fallback.
    pinMode(ROLE_STRAP_PIN, INPUT_PULLUP);
    bool roleStrapForcesSlave = (digitalRead(ROLE_STRAP_PIN) == LOW);

    // Secondary heuristic: board without OLED should become UART slave.
    if (roleStrapForcesSlave || !gDisplayReady) {
        gForceSlaveRole = true;
        if (DEBUG_ENABLED) {
            Serial.println("! Forcing SLAVE role fallback (strap/OLED)");
        }
    }
    #endif
    
    #if DEVICE_MODE == DEVICE_MODE_MASTER || DEVICE_MODE == DEVICE_MODE_STANDALONE
    if (!(DEVICE_MODE == DEVICE_MODE_MASTER && gForceSlaveRole)) {
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
    }
    #endif
    
    #if DEVICE_MODE == DEVICE_MODE_MASTER
    // Initialize Slave communication
    setupSlave();

    if (gForceSlaveRole) {
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        slaveConnected = true;
        lastSuccessfulSlaveComm = millis();
        if (DEBUG_ENABLED) {
            Serial.println("✓ Forced SLAVE fallback active (WiFi off)");
        }
    }
    #endif
    
    #if DEVICE_MODE == DEVICE_MODE_SLAVE
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);

    if (DEBUG_ENABLED) {
        Serial.println("✓ Slave WiFi disabled");
    }

    // Slave: Setup UART for receiving commands
    setupSlave();
    #endif
    
    if (DEBUG_ENABLED) {
        Serial.println("✓ Setup complete!\n");
    }
}

void loop() {
    #if DEVICE_MODE == DEVICE_MODE_MASTER || DEVICE_MODE == DEVICE_MODE_STANDALONE

    #if DEVICE_MODE == DEVICE_MODE_MASTER
    if (gForceSlaveRole) {
        runSlaveRoleLoop();
    } else {
    #endif
    
    // Maintain WiFi connection
    if (!gAccessPointMode && WiFi.status() != WL_CONNECTED) {
        if (DEBUG_ENABLED) {
            Serial.println("⚠ WiFi disconnected, attempting to reconnect...");
        }
        setupWiFi();
    }
    
    // Maintain MQTT connection
    if (!gAccessPointMode && !gMQTT->isConnected()) {
        gMQTT->reconnect();
    }
    
    // Process MQTT messages
    gMQTT->loop();
    
    #if DEVICE_MODE == DEVICE_MODE_MASTER
    // Communicate with Slave
    communicateWithSlave();

    // If slave is offline, drive Phase A with SUM(A+B+C) as fallback.
    float phaseAOut = gMQTT->getCurrentPhaseA();
    if (!slaveConnected) {
        float sum = 0.0f;
        bool hasAny = false;
        const float a = gMQTT->getCurrentPhaseA();
        const float b = gMQTT->getCurrentPhaseB();
        const float c = gMQTT->getCurrentPhaseC();
        if (a != CURRENT_DEFAULT) { sum += a; hasAny = true; }
        if (b != CURRENT_DEFAULT) { sum += b; hasAny = true; }
        if (c != CURRENT_DEFAULT) { sum += c; hasAny = true; }
        if (hasAny) {
            phaseAOut = sum;
        }
    }
    gSignalGen->setCurrentPhaseA(phaseAOut);

    #else
    // Standalone: normal Phase A behavior
    gSignalGen->setCurrentPhaseA(gMQTT->getCurrentPhaseA());
    #endif

    gSignalGen->update();

    #if DEVICE_MODE == DEVICE_MODE_MASTER
    }
    #endif
    
    #endif
    
    #if DEVICE_MODE == DEVICE_MODE_SLAVE
    
    // Slave: Listen for commands from Master (byte-wise framing)
    static uint8_t rxBuf[UART_PACKET_SIZE];
    static uint8_t rxIndex = 0;
    unsigned long now = millis();

    // Periodic heartbeat so master can detect slave presence independently
    if (now - lastSlaveHeartbeatTx >= 250) {
        slaveSerial.write(UART_HEARTBEAT_BYTE);
        lastSlaveHeartbeatTx = now;
    }

    while (slaveSerial.available() > 0) {
        uint8_t b = (uint8_t)slaveSerial.read();

        if (rxIndex == 0) {
            if (b != UART_SYNC) {
                continue;  // wait for frame start
            }
            rxBuf[rxIndex++] = b;
            continue;
        }

        rxBuf[rxIndex++] = b;
        if (rxIndex < UART_PACKET_SIZE) {
            continue;  // frame not complete yet
        }

        UARTPacket packet;
        float phaseB = 0, phaseC = 0;

        if (UARTProtocol::parsePacket(rxBuf, packet, phaseB, phaseC)) {
            gSignalGen->setCurrentPhaseB(phaseB);
            gSignalGen->setCurrentPhaseC(phaseC);
            slavePhaseB = phaseB;
            slavePhaseC = phaseC;
            slaveConnected = true;
            lastSuccessfulSlaveComm = millis();

            // Confirm to master that slave received and parsed packet
            slaveSerial.write(UART_ACK_BYTE);

            if (DEBUG_ENABLED && millis() - lastLogTime > LOG_INTERVAL) {
                Serial.print("Phase B: ");
                Serial.print(phaseB);
                Serial.print("A, Phase C: ");
                Serial.println(phaseC);
            }
            rxIndex = 0;
        } else {
            // Resync quickly: keep current byte only if it can start next frame
            rxIndex = (b == UART_SYNC) ? 1 : 0;
            if (rxIndex == 1) {
                rxBuf[0] = UART_SYNC;
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

    // Record graph sample every GRAPH_INTERVAL_MS
    if (millis() - lastGraphSample >= GRAPH_INTERVAL_MS) {
        lastGraphSample = millis();
        float phA = CURRENT_DEFAULT, phB = CURRENT_DEFAULT, phC = CURRENT_DEFAULT;
        #if DEVICE_MODE == DEVICE_MODE_MASTER || DEVICE_MODE == DEVICE_MODE_STANDALONE
        if (gMQTT != nullptr) {
            phA = gMQTT->getCurrentPhaseA();
            phB = gMQTT->getCurrentPhaseB();
            phC = gMQTT->getCurrentPhaseC();
        }
        #elif DEVICE_MODE == DEVICE_MODE_SLAVE
        phB = slavePhaseB;
        phC = slavePhaseC;
        #endif
        float gSum = 0.0f; bool gAny = false;
        if (phA != CURRENT_DEFAULT) { gSum += phA; gAny = true; }
        if (phB != CURRENT_DEFAULT) { gSum += phB; gAny = true; }
        if (phC != CURRENT_DEFAULT) { gSum += phC; gAny = true; }
        graphHistory[graphHead] = gAny ? gSum : CURRENT_DEFAULT;
        graphHead = (graphHead + 1) % GRAPH_SAMPLES;
    }

    if (millis() - lastDisplayUpdate > OLED_UPDATE_INTERVAL) {
        updateDisplay();
        lastDisplayUpdate = millis();
    }
}

void setupWiFi() {
    gAccessPointMode = false;

    if (!hasStationWiFiConfig()) {
        if (DEBUG_ENABLED) {
            Serial.println("No station WiFi configured, enabling fallback AP");
        }
        startFallbackAccessPoint();
        return;
    }

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
        startFallbackAccessPoint();
    }
}

static bool hasStationWiFiConfig() {
    return strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "your-ssid") != 0;
}

static void startFallbackAccessPoint() {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_AP);
    gAccessPointMode = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);

    if (DEBUG_ENABLED) {
        if (gAccessPointMode) {
            Serial.println("✓ Fallback AP enabled");
            Serial.print("  SSID: ");
            Serial.println(WIFI_AP_SSID);
            Serial.print("  IP: ");
            Serial.println(WiFi.softAPIP());
        } else {
            Serial.println("✗ Failed to start fallback AP");
        }
    }
}

void setupMQTT() {
    if (gAccessPointMode) {
        if (DEBUG_ENABLED) {
            Serial.println("Skipping MQTT setup while fallback AP is active");
        }
        return;
    }

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

static void runSlaveRoleLoop() {
    static uint8_t rxBuf[UART_PACKET_SIZE];
    static uint8_t rxIndex = 0;
    unsigned long now = millis();

    // Periodic heartbeat so master can detect slave presence independently
    if (now - lastSlaveHeartbeatTx >= 250) {
        slaveSerial.write(UART_HEARTBEAT_BYTE);
        lastSlaveHeartbeatTx = now;
    }

    while (slaveSerial.available() > 0) {
        uint8_t b = (uint8_t)slaveSerial.read();

        if (rxIndex == 0) {
            if (b != UART_SYNC) {
                continue;  // wait for frame start
            }
            rxBuf[rxIndex++] = b;
            continue;
        }

        rxBuf[rxIndex++] = b;
        if (rxIndex < UART_PACKET_SIZE) {
            continue;  // frame not complete yet
        }

        UARTPacket packet;
        float phaseB = 0, phaseC = 0;

        if (UARTProtocol::parsePacket(rxBuf, packet, phaseB, phaseC)) {
            gSignalGen->setCurrentPhaseB(phaseB);
            gSignalGen->setCurrentPhaseC(phaseC);
            slavePhaseB = phaseB;
            slavePhaseC = phaseC;
            slaveConnected = true;
            lastSuccessfulSlaveComm = millis();

            // Confirm to master that slave received and parsed packet
            slaveSerial.write(UART_ACK_BYTE);
            rxIndex = 0;
        } else {
            // Resync quickly: keep current byte only if it can start next frame
            rxIndex = (b == UART_SYNC) ? 1 : 0;
            if (rxIndex == 1) {
                rxBuf[0] = UART_SYNC;
            }
        }
    }

    // Check for slave timeout
    if (millis() - lastSuccessfulSlaveComm > SLAVE_TIMEOUT) {
        slaveConnected = false;
    }

    // Update DAC output for B/C channels
    gSignalGen->update();
}

void communicateWithSlave() {
    unsigned long now = millis();

    // Read ACKs from slave and update link state
    while (slaveSerial.available() > 0) {
        int b = slaveSerial.read();
        slaveRxByteCount++;

        // Any received byte means the UART link is physically alive.
        slaveConnected = true;
        lastSuccessfulSlaveComm = now;

        if (b == UART_ACK_BYTE || b == UART_HEARTBEAT_BYTE) {
            if (b == UART_ACK_BYTE) slaveAckCount++;
            if (b == UART_HEARTBEAT_BYTE) slaveHeartbeatCount++;
        }
    }

    if (now - lastSuccessfulSlaveComm > SLAVE_TIMEOUT) {
        slaveConnected = false;
    }

    #ifdef WOKWI_SIM
    // Simulator fallback: if no UART bytes ever arrive, keep the UI and flow usable.
    if (slaveRxByteCount == 0 && now > 2000) {
        virtualSlaveFallbackActive = true;
    }
    if (virtualSlaveFallbackActive) {
        slaveConnected = true;
        if (now - virtualSlaveHeartbeatTime >= 250) {
            virtualSlaveHeartbeatTime = now;
            slaveHeartbeatCount++;
            slaveRxByteCount++;
            lastSuccessfulSlaveComm = now;
        }
    }
    #endif
    
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
        gMQTT->setCurrentPhaseA(value);
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

void setupDisplay() {
    if (!OLED_ENABLED) return;

    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    gDisplay = new Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

    if (!gDisplay->begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        if (DEBUG_ENABLED) {
            Serial.println("✗ OLED init failed");
        }
        return;
    }

    gDisplayReady = true;
    gDisplay->clearDisplay();
    gDisplay->setTextColor(SSD1306_WHITE);
    gDisplay->setTextSize(1);
    gDisplay->setCursor(0, 0);
    gDisplay->println("Myenergi CT Sim");
    #if DEVICE_MODE == DEVICE_MODE_MASTER
    gDisplay->println("Mode: MASTER");
    #elif DEVICE_MODE == DEVICE_MODE_SLAVE
    gDisplay->println("Mode: SLAVE");
    #else
    gDisplay->println("Mode: STANDALONE");
    #endif
    gDisplay->display();
}

static void drawSumGraph(Adafruit_SSD1306* disp) {
    const int GX0 = 58;
    const int GX1 = 127;
    const int GY0 = 0;
    const int GY1 = 63;
    const int axisX = GX0;
    const int axisY = 31;
    const int plotX0 = GX0 + 2;
    const int plotWidth = GX1 - plotX0 + 1;
    const int halfHeight = 30;

    float maxAbs = 0.0f;
    bool hasData = false;
    for (int i = 0; i < GRAPH_SAMPLES; i++) {
        float v = graphHistory[i];
        if (v == CURRENT_DEFAULT) continue;
        hasData = true;
        float absV = fabsf(v);
        if (absV > maxAbs) maxAbs = absV;
    }
    if (maxAbs < 0.5f) maxAbs = 0.5f;

    disp->drawFastVLine(axisX, GY0, GY1 - GY0 + 1, SSD1306_WHITE);
    disp->drawFastHLine(axisX, axisY, GX1 - axisX + 1, SSD1306_WHITE);

    for (int y = axisY - halfHeight; y <= axisY + halfHeight; y += halfHeight) {
        disp->drawPixel(axisX - 1, y, SSD1306_WHITE);
        disp->drawPixel(axisX + 1, y, SSD1306_WHITE);
    }

    if (!hasData) {
        return;
    }

    int prevY = -1;
    for (int px = 0; px < plotWidth; px++) {
        int sampleIndex = (plotWidth == 1)
            ? 0
            : (px * (GRAPH_SAMPLES - 1)) / (plotWidth - 1);
        float v = graphHistory[(graphHead + sampleIndex) % GRAPH_SAMPLES];
        if (v == CURRENT_DEFAULT) {
            prevY = -1;
            continue;
        }

        int x = plotX0 + px;
        int y = axisY - (int)lroundf((v / maxAbs) * halfHeight);
        y = constrain(y, GY0, GY1);

        if (prevY >= 0) {
            int lo = min(y, prevY);
            int hi = max(y, prevY);
            for (int py = lo; py <= hi; py++) disp->drawPixel(x, py, SSD1306_WHITE);
        }
        disp->drawPixel(x, y, SSD1306_WHITE);
        prevY = y;
    }
}

void updateDisplay() {
    if (!OLED_ENABLED || !gDisplayReady) return;

    float phaseA = CURRENT_DEFAULT, phaseB = CURRENT_DEFAULT, phaseC = CURRENT_DEFAULT;
    bool commOk = false;

    #if DEVICE_MODE == DEVICE_MODE_MASTER
    if (gMQTT != nullptr) {
        phaseA = gMQTT->getCurrentPhaseA();
        phaseB = gMQTT->getCurrentPhaseB();
        phaseC = gMQTT->getCurrentPhaseC();
        commOk = gMQTT->isConnected();
    }
    #elif DEVICE_MODE == DEVICE_MODE_SLAVE
    phaseB = slavePhaseB;
    phaseC = slavePhaseC;
    commOk = slaveConnected;
    #else
    if (gMQTT != nullptr) {
        phaseA = gMQTT->getCurrentPhaseA();
        phaseB = gMQTT->getCurrentPhaseB();
        phaseC = gMQTT->getCurrentPhaseC();
        commOk = gMQTT->isConnected();
    }
    #endif

    float sum = 0.0f;
    bool hasAny = false;
    if (phaseA != CURRENT_DEFAULT) { sum += phaseA; hasAny = true; }
    if (phaseB != CURRENT_DEFAULT) { sum += phaseB; hasAny = true; }
    if (phaseC != CURRENT_DEFAULT) { sum += phaseC; hasAny = true; }

    float maxAbs = 0.5f;
    for (int i = 0; i < GRAPH_SAMPLES; i++) {
        float v = graphHistory[i];
        if (v == CURRENT_DEFAULT) continue;
        float absV = fabsf(v);
        if (absV > maxAbs) maxAbs = absV;
    }

    bool mqttOn = false;
    bool slaveOn = false;
    #if DEVICE_MODE == DEVICE_MODE_MASTER
    mqttOn = commOk;
    slaveOn = slaveConnected;
    #elif DEVICE_MODE == DEVICE_MODE_SLAVE
    mqttOn = false;
    slaveOn = commOk;
    #else
    mqttOn = commOk;
    slaveOn = false;
    #endif

    char va[7], vb[7], vc[7], vs[7], vy[7], vt[6], vmq[2], vsl[2];
    if (phaseA == CURRENT_DEFAULT) strcpy(va, "  ---"); else snprintf(va, sizeof(va), "%5.1f", phaseA);
    if (phaseB == CURRENT_DEFAULT) strcpy(vb, "  ---"); else snprintf(vb, sizeof(vb), "%5.1f", phaseB);
    if (phaseC == CURRENT_DEFAULT) strcpy(vc, "  ---"); else snprintf(vc, sizeof(vc), "%5.1f", phaseC);
    if (!hasAny)                   strcpy(vs, "  ---"); else snprintf(vs, sizeof(vs), "%5.1f", sum);
    snprintf(vy, sizeof(vy), "%5.1f", maxAbs);
    snprintf(vt, sizeof(vt), "%5s", "5m");
    strcpy(vmq, mqttOn ? "+" : "-");
    strcpy(vsl, slaveOn ? "+" : "-");

    gDisplay->clearDisplay();
    gDisplay->setTextColor(SSD1306_WHITE);
    gDisplay->setTextSize(1);

    const int yA = 0;
    const int yB = 8;
    const int yC = 16;
    const int yS = 24;
    const int yY = 32;    // Y-axis scale line (between S and T)
    const int yT = 40;
    const int yMQTT = 48;
    const int ySLAVE = 56;

    gDisplay->setCursor(0, yA);
    gDisplay->print("A:");
    gDisplay->setCursor(12, yA);
    gDisplay->print(va);
    gDisplay->setCursor(0, yB);
    gDisplay->print("B:");
    gDisplay->setCursor(12, yB);
    gDisplay->print(vb);
    gDisplay->setCursor(0, yC);
    gDisplay->print("C:");
    gDisplay->setCursor(12, yC);
    gDisplay->print(vc);
    gDisplay->setCursor(0, yS);
    gDisplay->print("S:");
    gDisplay->setCursor(12, yS);
    gDisplay->print(vs);

    gDisplay->setCursor(0, yY);
    gDisplay->print("Y:");
    gDisplay->setCursor(12, yY);
    gDisplay->print(vy);

    gDisplay->setCursor(0, yT);
    gDisplay->print("T:");
    gDisplay->setCursor(12, yT);
    gDisplay->print(vt);
    gDisplay->setCursor(0, yMQTT);
    gDisplay->print("MQTT:");
    gDisplay->setCursor(38, yMQTT);
    gDisplay->print(vmq);
    gDisplay->setCursor(0, ySLAVE);
    gDisplay->print("SLAVE:");
    gDisplay->setCursor(38, ySLAVE);
    gDisplay->print(vsl);

    drawSumGraph(gDisplay);

    gDisplay->display();
}
