#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "config.h"
#include "mqtt_handler.h"
#include "signal_generator.h"

using OledDisplay = Adafruit_SH1106G;
#define OLED_WHITE SH110X_WHITE

// Global objects
WiFiClient espClient;
MQTTHandler* gMQTT = nullptr;
ACSignalGenerator* gSignalGen = nullptr;
OledDisplay* gDisplay = nullptr;
bool gDisplayReady = false;
bool gAccessPointMode = false;

// Forward declarations
void mqttMessageCallback(char* topic, uint8_t* payload, unsigned int length);
void setupWiFi();
void setupMQTT();
void logStatus();
void setupDisplay();
void updateDisplay();
static bool hasStationWiFiConfig();
static void startFallbackAccessPoint();

// Timing
unsigned long lastLogTime = 0;
unsigned long lastDisplayUpdate = 0;

// Sum graph ring-buffer (one sample every GRAPH_INTERVAL_MS, 15-min window)
static float graphHistory[GRAPH_SAMPLES];
static uint8_t graphHead = 0;
static unsigned long lastGraphSample = 0;
static void drawSumGraph(OledDisplay* disp);

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(500);
    
    if (DEBUG_ENABLED) {
        Serial.println("\n\n=== CT Clamp Simulator ===");
        Serial.println("Initializing...");
    }
    
    // Initialize signal generator
    gSignalGen = new ACSignalGenerator();
    gSignalGen->begin();
    
    if (DEBUG_ENABLED) {
        Serial.println("✓ Signal generator initialized");
    }
    
        // Initialize graph history
        for (int i = 0; i < GRAPH_SAMPLES; i++) graphHistory[i] = CURRENT_DEFAULT;
    
    // Initialize MQTT
    gMQTT = new MQTTHandler(espClient);
    gMQTT->setCallback(mqttMessageCallback);
    
    if (DEBUG_ENABLED) {
        Serial.println("✓ MQTT handler initialized");
    }

    setupDisplay();
    
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
    
    // Update AC signal output
    gSignalGen->update();

    if (millis() - lastDisplayUpdate > OLED_UPDATE_INTERVAL) {
        updateDisplay();
        lastDisplayUpdate = millis();
    }

    // Record graph sample every GRAPH_INTERVAL_MS
    if (millis() - lastGraphSample >= GRAPH_INTERVAL_MS) {
        lastGraphSample = millis();
        float ga = gMQTT->getCurrentPhaseA();
        float gb = gMQTT->getCurrentPhaseB();
        float gc = gMQTT->getCurrentPhaseC();
        float gSum = 0.0f; bool gAny = false;
        if (ga != CURRENT_DEFAULT) { gSum += ga; gAny = true; }
        if (gb != CURRENT_DEFAULT) { gSum += gb; gAny = true; }
        if (gc != CURRENT_DEFAULT) { gSum += gc; gAny = true; }
        graphHistory[graphHead] = gAny ? gSum : CURRENT_DEFAULT;
        graphHead = (graphHead + 1) % GRAPH_SAMPLES;
    }
    
    // Periodic status logging
    if (millis() - lastLogTime > LOG_INTERVAL) {
        logStatus();
        lastLogTime = millis();
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

    gMQTT->begin("ctsim-esp32");
    
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
        gMQTT->setCurrentPhaseA(value);
        gSignalGen->setCurrentPhaseA(value);
    } else if (strcmp(topic, MQTT_TOPIC_PHASE_B) == 0) {
        gMQTT->setCurrentPhaseB(value);
        gSignalGen->setCurrentPhaseB(value);
    } else if (strcmp(topic, MQTT_TOPIC_PHASE_C) == 0) {
        gMQTT->setCurrentPhaseC(value);
        gSignalGen->setCurrentPhaseC(value);
    }
}

void setupDisplay() {
    if (!OLED_ENABLED) return;

    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    gDisplay = new OledDisplay(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

    if (!gDisplay->begin(OLED_I2C_ADDRESS, true)) {
        if (DEBUG_ENABLED) {
            Serial.println("✗ OLED init failed");
        }
        return;
    }

    if (DEBUG_ENABLED) {
        Serial.print("✓ OLED init address: 0x");
        Serial.println(OLED_I2C_ADDRESS, HEX);
    }

    gDisplayReady = true;
    gDisplay->clearDisplay();
    gDisplay->setTextColor(OLED_WHITE);
    gDisplay->setTextSize(1);
    gDisplay->setCursor(0, 0);
    gDisplay->println("CT Simulator");
    gDisplay->println("OLED online");
    gDisplay->display();
}

static void drawSumGraph(OledDisplay* disp) {
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

    disp->drawFastVLine(axisX, GY0, GY1 - GY0 + 1, OLED_WHITE);
    disp->drawFastHLine(axisX, axisY, GX1 - axisX + 1, OLED_WHITE);

    for (int y = axisY - halfHeight; y <= axisY + halfHeight; y += halfHeight) {
        disp->drawPixel(axisX - 1, y, OLED_WHITE);
        disp->drawPixel(axisX + 1, y, OLED_WHITE);
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
            for (int py = lo; py <= hi; py++) disp->drawPixel(x, py, OLED_WHITE);
        }
        disp->drawPixel(x, y, OLED_WHITE);
        prevY = y;
    }
}

void updateDisplay() {
    if (!OLED_ENABLED || !gDisplayReady || gMQTT == nullptr) return;

    float a = gMQTT->getCurrentPhaseA();
    float b = gMQTT->getCurrentPhaseB();
    float c = gMQTT->getCurrentPhaseC();
    bool commOk = gMQTT->isConnected();

    float sum = 0.0f;
    bool hasAny = false;
    if (a != CURRENT_DEFAULT) { sum += a; hasAny = true; }
    if (b != CURRENT_DEFAULT) { sum += b; hasAny = true; }
    if (c != CURRENT_DEFAULT) { sum += c; hasAny = true; }

    float maxAbs = 0.5f;
    for (int i = 0; i < GRAPH_SAMPLES; i++) {
        float v = graphHistory[i];
        if (v == CURRENT_DEFAULT) continue;
        float absV = fabsf(v);
        if (absV > maxAbs) maxAbs = absV;
    }

    char va[7], vb[7], vc[7], vs[7], vg[7], vt[6], vmq[2], vsl[2];
    if (a == CURRENT_DEFAULT) strcpy(va, "  ---"); else snprintf(va, sizeof(va), "%5.1f", a);
    if (b == CURRENT_DEFAULT) strcpy(vb, "  ---"); else snprintf(vb, sizeof(vb), "%5.1f", b);
    if (c == CURRENT_DEFAULT) strcpy(vc, "  ---"); else snprintf(vc, sizeof(vc), "%5.1f", c);
    if (!hasAny)              strcpy(vs, "  ---"); else snprintf(vs, sizeof(vs), "%5.1f", sum);
    snprintf(vg, sizeof(vg), "%5.1f", maxAbs);
    snprintf(vt, sizeof(vt), "%5s", "5m");
    strcpy(vmq, commOk ? "+" : "-");
    strcpy(vsl, "-");

    gDisplay->clearDisplay();
    gDisplay->setTextColor(OLED_WHITE);
    gDisplay->setTextSize(1);

    gDisplay->setCursor(0, 0);
    gDisplay->print("A:");
    gDisplay->setCursor(12, 0);
    gDisplay->print(va);
    gDisplay->setCursor(0, 8);
    gDisplay->print("B:");
    gDisplay->setCursor(12, 8);
    gDisplay->print(vb);
    gDisplay->setCursor(0, 16);
    gDisplay->print("C:");
    gDisplay->setCursor(12, 16);
    gDisplay->print(vc);
    gDisplay->setCursor(0, 24);
    gDisplay->print("S:");
    gDisplay->setCursor(12, 24);
    gDisplay->print(vs);
    gDisplay->setCursor(0, 32);
    gDisplay->print("Y:");
    gDisplay->setCursor(12, 32);
    gDisplay->print(vg);
    gDisplay->setCursor(0, 40);
    gDisplay->print("T:");
    gDisplay->setCursor(12, 40);
    gDisplay->print(vt);
    gDisplay->setCursor(0, 48);
    gDisplay->print("MQTT:");
    gDisplay->setCursor(38, 48);
    gDisplay->print(vmq);
    gDisplay->setCursor(0, 56);
    gDisplay->print("SLAVE:");
    gDisplay->setCursor(38, 56);
    gDisplay->print(vsl);

    drawSumGraph(gDisplay);

    gDisplay->display();
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
