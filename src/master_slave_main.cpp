#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Update.h>
#include <cstring>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "config.h"
#include "mqtt_handler.h"
#include "signal_generator.h"
#include "uart_protocol.h"

using OledDisplay = Adafruit_SH1106G;
#define OLED_WHITE SH110X_WHITE

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
#define SLAVE_BAUD_RATE 115200
#define SLAVE_TX_PIN 17
#define SLAVE_RX_PIN 16
#define ROLE_STRAP_PIN 27
#define UART_ACK_BYTE 0x4B  // 'K' = slave alive acknowledgment
#define UART_HEARTBEAT_BYTE 0x48  // 'H' = periodic slave heartbeat
#define SLAVE_EN_CONTROL_PIN 32
#define SLAVE_BOOT_CONTROL_PIN 33

// Serial OTA protocol framing
#define OTA_SYNC_BYTE 0x5A
#define OTA_END_BYTE 0xA5
#define OTA_TYPE_START 0x01
#define OTA_TYPE_DATA 0x02
#define OTA_TYPE_END 0x03
#define OTA_TYPE_ACK 0x80
#define OTA_TYPE_NACK 0x81
#define OTA_MAX_PAYLOAD 220
#define OTA_ACK_TIMEOUT_MS 1500

// ============= Global Objects =============
WiFiClient espClient;
MQTTHandler* gMQTT = nullptr;
ACSignalGenerator* gSignalGen = nullptr;
OledDisplay* gDisplay = nullptr;
bool gDisplayReady = false;
bool gAccessPointMode = false;
bool gForceSlaveRole = false;
unsigned long gBootStartMs = 0;
char gWifiConnectError[64] = "";  // empty = no error, non-empty = shown on boot display

struct RuntimeConfig {
    char wifiSsid[64] = "";
    char wifiPass[64] = "";
    char hostname[64] = "ctsim-master";
    char mqttServer[64] = MQTT_SERVER;
    uint16_t mqttPort = MQTT_PORT;
    char mqttPath[64] = "esp32CTSimulator";
    char mqttUser[64] = "";
    char mqttPass[64] = "";
    char webUser[32] = "";
    char webPass[32] = "";
};

RuntimeConfig gRuntimeConfig;
Preferences gPrefs;
WebServer gWebServer(80);
DNSServer gDnsServer;
bool gWebServerStarted = false;
bool gCaptiveDnsStarted = false;
bool gReconfigureRequested = false;
bool gSlaveOtaInProgress = false;
bool gSlaveOtaLastResult = false;
uint16_t gSlaveOtaSeq = 0;
uint32_t gSlaveOtaTotalBytes = 0;
uint32_t gSlaveOtaSentBytes = 0;
char gSlaveOtaStatus[64] = "idle";
bool gMasterOtaInProgress = false;
bool gMasterOtaLastResult = false;
uint32_t gMasterOtaTotalBytes = 0;
uint32_t gMasterOtaReceivedBytes = 0;
char gMasterOtaStatus[64] = "idle";
char gMasterOtaLastMessage[96] = "idle";
bool gSlaveOtaSelfTestLastResult = false;
char gSlaveOtaSelfTestMessage[96] = "not-run";
bool gSlaveOtaSelfTestSessionPassed = false;
char gSlaveOtaLastMessage[96] = "idle";
char gLastMqttTopic[160] = "";
char gLastMqttPayload[160] = "";
unsigned long gLastMqttMessageMs = 0;
float gLastPublishedSumWatt = CURRENT_DEFAULT;
float gLastPublishedSumAmp = CURRENT_DEFAULT;
float gLastReceivedSumWatt = CURRENT_DEFAULT;
bool gHasReceivedSumWatt = false;
bool gIgnoreFirstSumWatt = true;
bool gGraphPReady = false;

// Slave OTA receiver state
bool gSlaveOtaReceiving = false;
uint16_t gSlaveOtaExpectedSeq = 0;
uint32_t gSlaveOtaExpectedBytes = 0;
uint32_t gSlaveOtaReceivedBytes = 0;

// Slave communication
HardwareSerial slaveSerial(SLAVE_UART_NUM);

// Timing
unsigned long lastLogTime = 0;
unsigned long lastSlaveCheckTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastSlaveHeartbeatTx = 0;
unsigned long lastSumPublishAttemptMs = 0;
unsigned long lastApReconnectAttemptMs = 0;
unsigned long lastCtsimStatusPublishMs = 0;
unsigned long gApAsyncLastActionMs = 0;
unsigned long gApAsyncConnectStartMs = 0;
bool gApAsyncConnectInProgress = false;
unsigned int gApAsyncAttemptCount = 0;
char lastCtsimStatus[32] = "";
#define SLAVE_TIMEOUT 1000  // milliseconds
#define AP_RETRY_INTERVAL_MS 120000UL
#define AP_RETRY_INTERVAL_NO_CLIENT_MS 30000UL
#define CTSIM_STATUS_PUBLISH_INTERVAL_MS 60000UL

// Slave status
bool slaveConnected = false;
bool gSlaveConnectedPrev = false;
unsigned long gSlaveOnlineSinceMs = 0;
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
#define BOOT_INFO_DURATION_MS 30000UL
#define SUM_WATT_PUBLISH_INTERVAL_MS 1000UL
#define SUM_WATT_CHANGE_EPSILON 0.05f
#define SUM_WATT_LINE_VOLTAGE 230.0f

// ============= Forward Declarations =============
void mqttMessageCallback(char* topic, uint8_t* payload, unsigned int length);
static bool parseMqttNumericPayload(const char* payload, float& outValue);
void setupWiFi();
void setupMQTT();
void setupSlave();
static void drawSumGraph(OledDisplay* disp);
static bool hasStationWiFiConfig();
static unsigned long getApRetryIntervalMs();
static void serviceApModeAsyncWifiCheck();
static void buildMqttClientId(char* out, size_t outSize);
static void startFallbackAccessPoint();
static void loadRuntimeConfig();
static void saveRuntimeConfig();
static void setupWebServer();
static void ensureNetworkServices();
static void serviceNetworkServices();
static void requestRuntimeReconfigure();
static uint8_t monthFromDateToken(const char* monthToken);
static void getProjectVersion(char* out, size_t outSize);
static String buildDashboardHtml();
static void drawOtaRunningDisplay();
static String jsonEscape(const String& input);
static String mqttStatusJson();
static void updateSlaveOnlineSince();
static uint16_t computeOtaFrameCrc(uint8_t type, uint8_t len, const uint8_t* payload);
static size_t buildOtaFrame(uint8_t type, const uint8_t* payload, uint8_t payloadLen, uint8_t* out, size_t outSize);
static bool readNextOtaFrame(uint8_t& typeOut, uint8_t* payloadOut, uint8_t& payloadLenOut, unsigned long timeoutMs);
static bool waitForSlaveOtaAck(uint8_t expectedType, unsigned long timeoutMs);
static bool sendSlaveOtaStart(uint32_t imageSize);
static bool sendSlaveOtaDataChunk(const uint8_t* chunk, uint16_t chunkLen);
static bool sendSlaveOtaEnd();
static bool runSlaveOtaSelfTest(String& detail);
static bool processIncomingOtaFrame(uint8_t type, const uint8_t* payload, uint8_t payloadLen);
static bool tryParseSlaveOtaByte(uint8_t b);
static void runSlaveRoleLoop();
void communicateWithSlave();
void logStatus();
void setupDisplay();
void updateDisplay();
static void drawBootInfoDisplay();
static void drawApModeDisplay();

void setup() {
    Serial.begin(SERIAL_BAUD);
    gBootStartMs = millis();
    delay(500);
    
    if (DEBUG_ENABLED) {
        Serial.println("\n\n=== CT Clamp Simulator ===");
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

    #if DEVICE_MODE == DEVICE_MODE_MASTER || DEVICE_MODE == DEVICE_MODE_STANDALONE
    loadRuntimeConfig();
    #endif
    
    #if DEVICE_MODE == DEVICE_MODE_MASTER || DEVICE_MODE == DEVICE_MODE_STANDALONE
    if (!(DEVICE_MODE == DEVICE_MODE_MASTER && gForceSlaveRole)) {
        // Initialize MQTT (Master only)
        gMQTT = new MQTTHandler(espClient);
        gMQTT->setCallback(mqttMessageCallback);
        gMQTT->setConnectionConfig(
            gRuntimeConfig.mqttServer,
            gRuntimeConfig.mqttPort,
            gRuntimeConfig.mqttUser,
            gRuntimeConfig.mqttPass
        );

        if (DEBUG_ENABLED) {
            Serial.println("✓ MQTT handler initialized");
        }

        // Setup WiFi
        setupWiFi();

        // Setup portal/status web server and captive DNS in AP mode
        setupWebServer();
        ensureNetworkServices();

        // Setup MQTT connection
        setupMQTT();
    }
    #endif
    
    #if DEVICE_MODE == DEVICE_MODE_MASTER
    // Initialize Slave communication
    setupSlave();
    // Keep control lines high-impedance during normal runtime.
    // Some boards use inverting transistor stages; actively driving these pins can hold
    // the slave in reset/boot mode and break UART communication.
    pinMode(SLAVE_EN_CONTROL_PIN, INPUT_PULLUP);
    pinMode(SLAVE_BOOT_CONTROL_PIN, INPUT_PULLUP);

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
        ensureNetworkServices();
    }

    // In AP mode, run non-blocking background check for configured station WiFi.
    serviceApModeAsyncWifiCheck();
    
    // Maintain MQTT connection
    if (!gAccessPointMode && !gMQTT->isConnected()) {
        gMQTT->reconnect();
    }

    // Periodically ensure all datapoints exist on the broker
    if (!gAccessPointMode && gMQTT->isConnected()) {
        gMQTT->periodicDatapointCheck(gRuntimeConfig.mqttPath);
    }
    
    // Process MQTT messages
    gMQTT->loop();

    if (gMQTT != nullptr && gMQTT->isConnected()) {
        char runtimeStatus[32] = "online";
        if (gAccessPointMode || WiFi.status() != WL_CONNECTED) {
            strncpy(runtimeStatus, "ap_mode", sizeof(runtimeStatus) - 1);
            runtimeStatus[sizeof(runtimeStatus) - 1] = '\0';
        }
        #if DEVICE_MODE == DEVICE_MODE_MASTER
        else if (!slaveConnected) {
            strncpy(runtimeStatus, "slave_offline", sizeof(runtimeStatus) - 1);
            runtimeStatus[sizeof(runtimeStatus) - 1] = '\0';
        }
        #endif

        unsigned long now = millis();
        bool changed = strcmp(runtimeStatus, lastCtsimStatus) != 0;
        bool periodic = (now - lastCtsimStatusPublishMs >= CTSIM_STATUS_PUBLISH_INTERVAL_MS);
        if (changed || periodic) {
            if (gMQTT->publishStatus(runtimeStatus)) {
                strncpy(lastCtsimStatus, runtimeStatus, sizeof(lastCtsimStatus) - 1);
                lastCtsimStatus[sizeof(lastCtsimStatus) - 1] = '\0';
                lastCtsimStatusPublishMs = now;
            }
        }
    }

    serviceNetworkServices();

    if (gReconfigureRequested) {
        gReconfigureRequested = false;
        setupWiFi();
        ensureNetworkServices();
        if (!gAccessPointMode) {
            setupMQTT();
        }
    }
    
    #if DEVICE_MODE == DEVICE_MODE_MASTER
    // Communicate with Slave
    if (!gSlaveOtaInProgress) {
        communicateWithSlave();
    }
    updateSlaveOnlineSince();

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

        if (tryParseSlaveOtaByte(b)) {
            continue;
        }

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
        if (gGraphPReady) {
            graphHistory[graphHead] = gLastReceivedSumWatt;
            graphHead = (graphHead + 1) % GRAPH_SAMPLES;
        }
    }

    // P/SumPower_kW is intentionally read-only from MQTT input.
    // Do not publish SumPower_kW from this firmware.

    if (millis() - lastDisplayUpdate > OLED_UPDATE_INTERVAL) {
        updateDisplay();
        lastDisplayUpdate = millis();
    }
}

void setupWiFi() {
    #if DEVICE_MODE != DEVICE_MODE_MASTER
    // WiFi disabled on slave
    if (DEBUG_ENABLED) Serial.println("WiFi disabled on slave mode");
    return;
    #endif

    gAccessPointMode = false;
    gWifiConnectError[0] = '\0';  // clear any previous error

    if (!hasStationWiFiConfig()) {
        if (DEBUG_ENABLED) {
            Serial.println("No station WiFi configured, enabling fallback AP");
        }
        snprintf(gWifiConnectError, sizeof(gWifiConnectError), "No SSID configured");
        startFallbackAccessPoint();
        return;
    }

    if (DEBUG_ENABLED) {
        Serial.print("Connecting to WiFi: ");
        Serial.println(gRuntimeConfig.wifiSsid);
    }

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(gRuntimeConfig.hostname);

    bool connected = false;
    wl_status_t wifiStatus = WL_IDLE_STATUS;

    // Single STA attempt at boot/reconfigure; periodic retries are handled in loop().
    if (OLED_ENABLED && gDisplayReady) {
        char ssidBuf[17];
        strncpy(ssidBuf, gRuntimeConfig.wifiSsid, 16);
        ssidBuf[16] = '\0';
        gDisplay->clearDisplay();
        gDisplay->setTextColor(OLED_WHITE);
        gDisplay->setTextSize(1);
        gDisplay->setCursor(0, 0);
        gDisplay->println("WiFi connecting...");
        gDisplay->setCursor(0, 12);
        gDisplay->print("SSID: ");
        gDisplay->println(ssidBuf);
        gDisplay->setCursor(0, 24);
        gDisplay->println("Initial attempt...");
        gDisplay->display();
    }

    WiFi.disconnect(false);
    WiFi.begin(gRuntimeConfig.wifiSsid, gRuntimeConfig.wifiPass);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
        delay(500);
        if (DEBUG_ENABLED) Serial.print(".");
    }

    wifiStatus = WiFi.status();
    connected = (wifiStatus == WL_CONNECTED);

    if (connected) {
        if (DEBUG_ENABLED) {
            Serial.println("\n✓ WiFi connected!");
            Serial.print("  IP: ");
            Serial.println(WiFi.localIP());
        }
    } else {
        if (DEBUG_ENABLED) {
            Serial.println("\n✗ WiFi connection failed on initial attempt!");
            Serial.println("--- Connection Failure Details ---");
            Serial.print("  SSID: ");
            Serial.println(gRuntimeConfig.wifiSsid);
            Serial.print("  Status Code: ");
            Serial.println((int)wifiStatus);
            Serial.println("  Switching to Fallback AP mode...");
        }
        // Build human-readable error for display
        if (wifiStatus == WL_NO_SSID_AVAIL) {
            snprintf(gWifiConnectError, sizeof(gWifiConnectError), "SSID not found");
        } else if (wifiStatus == WL_CONNECT_FAILED) {
            snprintf(gWifiConnectError, sizeof(gWifiConnectError), "Wrong password");
        } else if (wifiStatus == WL_DISCONNECTED) {
            snprintf(gWifiConnectError, sizeof(gWifiConnectError), "SSID not found/timeout");
        } else {
            snprintf(gWifiConnectError, sizeof(gWifiConnectError), "Failed (code %d)", (int)wifiStatus);
        }
        // Show error on OLED before switching to AP mode
        if (OLED_ENABLED && gDisplayReady) {
            char ssidBuf[17];
            strncpy(ssidBuf, gRuntimeConfig.wifiSsid, 16);
            ssidBuf[16] = '\0';
            gDisplay->clearDisplay();
            gDisplay->setTextColor(OLED_WHITE);
            gDisplay->setTextSize(1);
            gDisplay->setCursor(0, 0);
            gDisplay->println("WiFi Error!");
            gDisplay->setCursor(0, 10);
            gDisplay->print("SSID: ");
            gDisplay->println(ssidBuf);
            gDisplay->setCursor(0, 20);
            gDisplay->println(gWifiConnectError);
            gDisplay->setCursor(0, 32);
            gDisplay->println("Starting AP mode...");
            gDisplay->setCursor(0, 42);
            gDisplay->print("AP: ");
            gDisplay->println(WIFI_AP_SSID);
            gDisplay->display();
            delay(3000);  // pause so user can read the error
        }
        startFallbackAccessPoint();
    }
}

static bool hasStationWiFiConfig() {
    return strlen(gRuntimeConfig.wifiSsid) > 0 && strcmp(gRuntimeConfig.wifiSsid, "your-ssid") != 0;
}

static unsigned long getApRetryIntervalMs() {
    // Retry faster when nobody is connected to AP; keep a slower interval while a client is active.
    int stationCount = WiFi.softAPgetStationNum();
    return (stationCount <= 0) ? AP_RETRY_INTERVAL_NO_CLIENT_MS : AP_RETRY_INTERVAL_MS;
}

static void serviceApModeAsyncWifiCheck() {
    if (!gAccessPointMode || !hasStationWiFiConfig()) {
        gApAsyncConnectInProgress = false;
        return;
    }

    unsigned long now = millis();

    if (gApAsyncConnectInProgress) {
        wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED) {
            gApAsyncConnectInProgress = false;
            gAccessPointMode = false;
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            gWifiConnectError[0] = '\0';
            if (DEBUG_ENABLED) {
                Serial.print("✓ AP async WiFi reconnect succeeded. IP: ");
                Serial.println(WiFi.localIP());
            }
            ensureNetworkServices();
            setupMQTT();
            return;
        }

        if (now - gApAsyncConnectStartMs >= WIFI_TIMEOUT) {
            gApAsyncConnectInProgress = false;
            if (status == WL_CONNECT_FAILED) {
                snprintf(gWifiConnectError, sizeof(gWifiConnectError), "Wrong password");
            } else {
                snprintf(gWifiConnectError, sizeof(gWifiConnectError), "SSID not found/timeout");
            }
            if (DEBUG_ENABLED) {
                Serial.print("⚠ AP async WiFi connect failed, status=");
                Serial.println((int)status);
            }
            gApAsyncLastActionMs = now;
        }
        return;
    }

    int scanState = WiFi.scanComplete();
    if (scanState == WIFI_SCAN_RUNNING) {
        return;
    }

    if (scanState >= 0) {
        bool configuredSsidPresent = false;
        for (int i = 0; i < scanState; i++) {
            if (WiFi.SSID(i) == String(gRuntimeConfig.wifiSsid)) {
                configuredSsidPresent = true;
                break;
            }
        }
        WiFi.scanDelete();

        if (configuredSsidPresent) {
            if (DEBUG_ENABLED) {
                Serial.println("↻ AP async check: configured SSID found, trying STA connect...");
            }
            WiFi.mode(WIFI_AP_STA);
            WiFi.setHostname(gRuntimeConfig.hostname);
            WiFi.begin(gRuntimeConfig.wifiSsid, gRuntimeConfig.wifiPass);
            gApAsyncConnectInProgress = true;
            gApAsyncConnectStartMs = now;
            gApAsyncAttemptCount++;
        } else {
            snprintf(gWifiConnectError, sizeof(gWifiConnectError), "SSID not found");
            if (DEBUG_ENABLED) {
                Serial.println("↻ AP async check: configured SSID not present");
            }
        }
        gApAsyncLastActionMs = now;
        return;
    }

    const unsigned long retryIntervalMs = getApRetryIntervalMs();
    if (now - gApAsyncLastActionMs < retryIntervalMs) {
        return;
    }

    if (DEBUG_ENABLED) {
        Serial.println("↻ AP async check: starting WiFi scan...");
    }
    WiFi.mode(WIFI_AP_STA);
    WiFi.scanNetworks(true, true);
    gApAsyncLastActionMs = now;
}

static void buildMqttClientId(char* out, size_t outSize) {
    if (out == nullptr || outSize == 0) return;

    const char* path = gRuntimeConfig.mqttPath;
    if (path == nullptr || path[0] == '\0') {
        strncpy(out, "esp32CTSimulator", outSize - 1);
        out[outSize - 1] = '\0';
        return;
    }

    const char* start = path;
    while (*start == '/' || *start == ' ') start++;

    const char* end = path + strlen(path);
    while (end > start && (end[-1] == '/' || end[-1] == ' ')) end--;

    if (end <= start) {
        strncpy(out, "esp32CTSimulator", outSize - 1);
        out[outSize - 1] = '\0';
        return;
    }

    size_t writeIndex = 0;
    for (const char* p = start; p < end && writeIndex < outSize - 1; ++p) {
        char ch = *p;
        bool ok = (ch >= 'a' && ch <= 'z') ||
                  (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') ||
                  ch == '-' || ch == '_';
        out[writeIndex++] = ok ? ch : '-';
    }

    if (writeIndex == 0) {
        strncpy(out, "esp32CTSimulator", outSize - 1);
        out[outSize - 1] = '\0';
        return;
    }

    out[writeIndex] = '\0';
}

static void startFallbackAccessPoint() {
    gApAsyncConnectInProgress = false;
    gApAsyncLastActionMs = 0;
    gApAsyncConnectStartMs = 0;
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
    // Always configure client ID, path and connection params so that
    // reconnect() uses correct values even when called after AP-mode retry.
    char clientId[64];
    buildMqttClientId(clientId, sizeof(clientId));
    gMQTT->begin(clientId);
    gMQTT->setMqttPath(gRuntimeConfig.mqttPath);
    gMQTT->setConnectionConfig(
        gRuntimeConfig.mqttServer,
        gRuntimeConfig.mqttPort,
        gRuntimeConfig.mqttUser,
        gRuntimeConfig.mqttPass
    );

    if (gAccessPointMode) {
        if (DEBUG_ENABLED) {
            Serial.println("MQTT config set; skipping connect while fallback AP is active");
        }
        return;
    }
    
    if (DEBUG_ENABLED) {
        Serial.print("Connecting to MQTT: ");
        Serial.print(gRuntimeConfig.mqttServer);
        Serial.print(":");
        Serial.println(gRuntimeConfig.mqttPort);
    }
    
    bool mqttConnected = false;
    if (strlen(gRuntimeConfig.mqttUser) > 0) {
        mqttConnected = gMQTT->connect(
            gRuntimeConfig.mqttServer,
            gRuntimeConfig.mqttPort,
            gRuntimeConfig.mqttUser,
            gRuntimeConfig.mqttPass
        );
    } else {
        mqttConnected = gMQTT->connect(
            gRuntimeConfig.mqttServer,
            gRuntimeConfig.mqttPort
        );
    }

    if (mqttConnected) {
        if (DEBUG_ENABLED) {
            Serial.println("✓ MQTT connected!");
        }
        gMQTT->subscribe();
        gMQTT->publishStatus("online");
        gMQTT->initializeDatapoints(gRuntimeConfig.mqttPath);
    } else {
        if (DEBUG_ENABLED) {
            Serial.println("✗ MQTT connection failed!");
        }
    }
}

static void loadRuntimeConfig() {
    memset(&gRuntimeConfig, 0, sizeof(gRuntimeConfig));
    strncpy(gRuntimeConfig.mqttServer, MQTT_SERVER, sizeof(gRuntimeConfig.mqttServer) - 1);
    gRuntimeConfig.mqttPort = MQTT_PORT;

    gPrefs.begin("ctsim", true);
    String wifiSsid = gPrefs.getString("wifi_ssid", WIFI_SSID);
    String wifiPass = gPrefs.getString("wifi_pass", WIFI_PASSWORD);
    String hostname = gPrefs.getString("hostname", "ctsim-master");
    String mqttServer = gPrefs.getString("mqtt_srv", MQTT_SERVER);
    uint16_t mqttPort = gPrefs.getUShort("mqtt_port", MQTT_PORT);
    String mqttPath = gPrefs.getString("mqtt_path", "esp32CTSimulator");
    String mqttUser = gPrefs.getString("mqtt_user", "");
    String mqttPass = gPrefs.getString("mqtt_pass", "");
    String webUser = gPrefs.getString("web_user", "");
    String webPass = gPrefs.getString("web_pass", "");
    gPrefs.end();

    strncpy(gRuntimeConfig.wifiSsid, wifiSsid.c_str(), sizeof(gRuntimeConfig.wifiSsid) - 1);
    strncpy(gRuntimeConfig.wifiPass, wifiPass.c_str(), sizeof(gRuntimeConfig.wifiPass) - 1);
    strncpy(gRuntimeConfig.hostname, hostname.c_str(), sizeof(gRuntimeConfig.hostname) - 1);
    strncpy(gRuntimeConfig.mqttServer, mqttServer.c_str(), sizeof(gRuntimeConfig.mqttServer) - 1);
    gRuntimeConfig.mqttPort = mqttPort > 0 ? mqttPort : MQTT_PORT;
    strncpy(gRuntimeConfig.mqttPath, mqttPath.c_str(), sizeof(gRuntimeConfig.mqttPath) - 1);
    strncpy(gRuntimeConfig.mqttUser, mqttUser.c_str(), sizeof(gRuntimeConfig.mqttUser) - 1);
    strncpy(gRuntimeConfig.mqttPass, mqttPass.c_str(), sizeof(gRuntimeConfig.mqttPass) - 1);
    strncpy(gRuntimeConfig.webUser, webUser.c_str(), sizeof(gRuntimeConfig.webUser) - 1);
    strncpy(gRuntimeConfig.webPass, webPass.c_str(), sizeof(gRuntimeConfig.webPass) - 1);
    gRuntimeConfig.mqttPort = mqttPort > 0 ? mqttPort : MQTT_PORT;
    strncpy(gRuntimeConfig.mqttUser, mqttUser.c_str(), sizeof(gRuntimeConfig.mqttUser) - 1);
    strncpy(gRuntimeConfig.mqttPass, mqttPass.c_str(), sizeof(gRuntimeConfig.mqttPass) - 1);

    if (strcmp(gRuntimeConfig.wifiSsid, "your-ssid") == 0) {
        gRuntimeConfig.wifiSsid[0] = '\0';
        gRuntimeConfig.wifiPass[0] = '\0';
    }
    if (strlen(gRuntimeConfig.mqttPath) == 0) {
        strncpy(gRuntimeConfig.mqttPath, "esp32CTSimulator", sizeof(gRuntimeConfig.mqttPath) - 1);
        gRuntimeConfig.mqttPath[sizeof(gRuntimeConfig.mqttPath) - 1] = '\0';
    }

    if (DEBUG_ENABLED) {
        Serial.println("Runtime config loaded from NVS");
    }
}

static void saveRuntimeConfig() {
    gPrefs.begin("ctsim", false);
    gPrefs.putString("wifi_ssid", gRuntimeConfig.wifiSsid);
    gPrefs.putString("wifi_pass", gRuntimeConfig.wifiPass);
    gPrefs.putString("hostname", gRuntimeConfig.hostname);
    gPrefs.putString("mqtt_srv", gRuntimeConfig.mqttServer);
    gPrefs.putUShort("mqtt_port", gRuntimeConfig.mqttPort);
    gPrefs.putString("mqtt_path", gRuntimeConfig.mqttPath);
    gPrefs.putString("mqtt_user", gRuntimeConfig.mqttUser);
    gPrefs.putString("mqtt_pass", gRuntimeConfig.mqttPass);
    gPrefs.putString("web_user", gRuntimeConfig.webUser);
    gPrefs.putString("web_pass", gRuntimeConfig.webPass);
    gPrefs.end();
}

static String jsonEscape(const String& input) {
    String out;
    out.reserve(input.length() + 8);
    for (size_t i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        if (c == '\\' || c == '"') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

static uint8_t monthFromDateToken(const char* monthToken) {
    if (monthToken == nullptr) return 0;
    if (strncmp(monthToken, "Jan", 3) == 0) return 1;
    if (strncmp(monthToken, "Feb", 3) == 0) return 2;
    if (strncmp(monthToken, "Mar", 3) == 0) return 3;
    if (strncmp(monthToken, "Apr", 3) == 0) return 4;
    if (strncmp(monthToken, "May", 3) == 0) return 5;
    if (strncmp(monthToken, "Jun", 3) == 0) return 6;
    if (strncmp(monthToken, "Jul", 3) == 0) return 7;
    if (strncmp(monthToken, "Aug", 3) == 0) return 8;
    if (strncmp(monthToken, "Sep", 3) == 0) return 9;
    if (strncmp(monthToken, "Oct", 3) == 0) return 10;
    if (strncmp(monthToken, "Nov", 3) == 0) return 11;
    if (strncmp(monthToken, "Dec", 3) == 0) return 12;
    return 0;
}

static void getProjectVersion(char* out, size_t outSize) {
    if (out == nullptr || outSize == 0) return;

    // Encode the build timestamp into the UI version string so field devices
    // can be matched to an exact flashed build without a separate release file.
    const char* date = __DATE__;  // "Mmm dd yyyy"
    const char* time = __TIME__;  // "hh:mm:ss"

    char monthToken[4] = { date[0], date[1], date[2], '\0' };
    const uint8_t mm = monthFromDateToken(monthToken);

    int dd = 0;
    if (date[4] == ' ') {
        dd = date[5] - '0';
    } else {
        dd = (date[4] - '0') * 10 + (date[5] - '0');
    }

    const int hh = (time[0] - '0') * 10 + (time[1] - '0');
    const int min = (time[3] - '0') * 10 + (time[4] - '0');

    snprintf(out, outSize, "1.%02u%02d.%02d%02d", (unsigned)mm, dd, hh, min);
}
static String formatDurationAdaptive(unsigned long totalSeconds) {
    const unsigned long SEC_PER_MIN = 60UL;
    const unsigned long SEC_PER_HOUR = 60UL * SEC_PER_MIN;
    const unsigned long SEC_PER_DAY = 24UL * SEC_PER_HOUR;
    const unsigned long SEC_PER_MONTH = 30UL * SEC_PER_DAY;
    const unsigned long SEC_PER_YEAR = 365UL * SEC_PER_DAY;

    unsigned long years = totalSeconds / SEC_PER_YEAR;
    totalSeconds %= SEC_PER_YEAR;
    unsigned long months = totalSeconds / SEC_PER_MONTH;
    totalSeconds %= SEC_PER_MONTH;
    unsigned long days = totalSeconds / SEC_PER_DAY;
    totalSeconds %= SEC_PER_DAY;
    unsigned long hours = totalSeconds / SEC_PER_HOUR;
    totalSeconds %= SEC_PER_HOUR;
    unsigned long mins = totalSeconds / SEC_PER_MIN;
    unsigned long secs = totalSeconds % SEC_PER_MIN;

    String out;
    if (years > 0) out += String(years) + "y ";
    if (months > 0) out += String(months) + "m ";
    if (days > 0) out += String(days) + "d ";
    if (hours > 0) out += String(hours) + "h ";
    if (mins > 0) out += String(mins) + "min ";
    if (secs > 0 || out.length() == 0) out += String(secs) + "s";

    out.trim();
    return out;
}

static String buildDashboardHtml() {
    String ip = gAccessPointMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    String wifiState = gAccessPointMode ? "AP (Provisioning)" : (WiFi.status() == WL_CONNECTED ? "STA Connected" : "Disconnected");
    String mqttState = (gMQTT && gMQTT->isConnected()) ? "Connected" : "Disconnected";
    char projectVersion[20];
    getProjectVersion(projectVersion, sizeof(projectVersion));

    String html;
    html.reserve(11000);
    html += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>CT Clamp Simulator</title><style>";
    html += "body{font-family:Verdana,sans-serif;background:linear-gradient(135deg,#f5f8ff,#e6fff0);margin:0;padding:20px;color:#1f2937}";
    html += ".wrap{max-width:960px;margin:auto}.card{background:#fff;border-radius:14px;padding:16px;margin-bottom:14px;box-shadow:0 8px 24px rgba(0,0,0,.08)}";
    html += "h1{margin:0 0 10px 0}.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}.full{grid-column:1/-1}";
    html += "input,button{padding:10px;border-radius:8px;border:1px solid #cbd5e1;width:100%;box-sizing:border-box}button{background:#0f766e;color:#fff;border:none;font-weight:700}";
    html += ".row{display:flex;gap:8px;align-items:center}.btn-alt{background:#0f172a}.mono{font-family:monospace}";
    html += ".bar{height:12px;background:#e2e8f0;border-radius:999px;overflow:hidden}.bar>span{display:block;height:100%;width:0;background:#16a34a;transition:width .25s ease}";
    html += ".values-col{display:flex;flex-direction:column;gap:6px}.status-pill{display:inline-block;padding:2px 8px;border-radius:999px;font-weight:700}.status-ok{background:#dcfce7;color:#166534}.status-bad{background:#fee2e2;color:#991b1b}";
    html += "small{color:#64748b}.kpi{font-size:18px;font-weight:700}.sep{border:none;border-top:1px solid #e5e7eb;margin:10px 0}";
    html += "@media(max-width:700px){.grid{grid-template-columns:1fr}}";
    html += "</style></head><body><div class='wrap'>";

    // Compute all status variables before building HTML
    float a = CURRENT_DEFAULT, b = CURRENT_DEFAULT, c = CURRENT_DEFAULT;
    if (gMQTT) {
        a = gMQTT->getCurrentPhaseA();
        b = gMQTT->getCurrentPhaseB();
        c = gMQTT->getCurrentPhaseC();
    }
    String sa = (a == CURRENT_DEFAULT) ? "---" : String(a, 1) + " A";
    String sb = (b == CURRENT_DEFAULT) ? "---" : String(b, 1) + " A";
    String sc = (c == CURRENT_DEFAULT) ? "---" : String(c, 1) + " A";
    float sum = 0.0f;
    bool hasAny = false;
    if (a != CURRENT_DEFAULT) { sum += a; hasAny = true; }
    if (b != CURRENT_DEFAULT) { sum += b; hasAny = true; }
    if (c != CURRENT_DEFAULT) { sum += c; hasAny = true; }
    String ssum = hasAny ? String(sum, 1) + " A" : "---";
    String slavePillClass = slaveConnected ? "status-pill status-ok" : "status-pill status-bad";
    String slavePillText = slaveConnected ? "online" : "offline";
    unsigned long slaveOnlineSinceS = (slaveConnected && gSlaveOnlineSinceMs > 0) ? ((millis() - gSlaveOnlineSinceMs) / 1000UL) : 0;
    String slaveOnlineSinceText = slaveConnected ? ("Online since " + formatDurationAdaptive(slaveOnlineSinceS)) : "";
    // The runtime has no RTC/NTP dependency, so the dashboard shows MQTT age
    // relative to the local receive time instead of an absolute wall-clock time.
    unsigned long lastChangeSinceS = (gLastMqttMessageMs > 0) ? ((millis() - gLastMqttMessageMs) / 1000UL) : 0;
    String lastChangeText = (gLastMqttMessageMs > 0) ? ("Last Change: " + formatDurationAdaptive(lastChangeSinceS)) : "";
    String mqttSessionClass = "status-pill status-bad";
    String mqttSessionText = "offline";
    String mqttServerClass = "status-pill status-bad";
    String mqttServerText = "failed";
    String mqttLoginClass = "status-pill";
    String mqttLoginText = "not used";
    String mqttStateText = "not initialized";
    String mqttStateCode = "n/a";
    if (gMQTT != nullptr) {
        const bool mqttConnected = gMQTT->isConnected();
        const bool mqttServerOk = gMQTT->isServerReachable();
        const bool mqttLoginConfigured = gMQTT->isLoginConfigured();
        const bool mqttLoginOk = gMQTT->isLoginOk();
        mqttSessionClass = mqttConnected ? "status-pill status-ok" : "status-pill status-bad";
        mqttSessionText = mqttConnected ? "online" : "offline";
        mqttServerClass = mqttServerOk ? "status-pill status-ok" : "status-pill status-bad";
        mqttServerText = mqttServerOk ? "OK" : "failed";
        if (mqttLoginConfigured) {
            mqttLoginClass = mqttLoginOk ? "status-pill status-ok" : "status-pill status-bad";
            mqttLoginText = mqttLoginOk ? "OK" : "failed";
        }
        mqttStateText = String(gMQTT->getConnectionStateText());
        mqttStateCode = String(gMQTT->getConnectionState());
    }
    String sp = gHasReceivedSumWatt ? String(gLastReceivedSumWatt, 1) + " W" : "---";

    // Header card — WiFi / MQTT / Slave status pills
    String wifiPillClass = (WiFi.status() == WL_CONNECTED && !gAccessPointMode) ? "status-pill status-ok" : "status-pill status-bad";
    String wifiPillText  = gAccessPointMode ? "AP mode" : (WiFi.status() == WL_CONNECTED ? "online" : "offline");
    html += "<div class='card'><h1>CT Clamp Simulator <small style='font-size:.52em;color:#64748b;font-weight:600'>v" + String(projectVersion) + "</small></h1>";
    html += "<div style='display:flex;gap:16px;flex-wrap:wrap;align-items:center'>";
    html += "<div>WiFi: <span class='" + wifiPillClass + "'>" + wifiPillText + "</span></div>";
    html += "<div>MQTT: <span id='mqtt-session-pill' class='" + mqttSessionClass + "'>" + mqttSessionText + "</span></div>";
    html += "<div>Slave: <span id='slave-link-pill' class='" + slavePillClass + "'>" + slavePillText + "</span> <span id='slave-online-since' style='color:#6b7280;font-size:.9em'>" + slaveOnlineSinceText + "</span></div>";
    html += "<div><span id='last-change-time' style='color:#6b7280;font-size:.9em'>" + lastChangeText + "</span></div>";
    html += "</div>";
    html += "</div>";

    // Current Values card: values on left, power graph on right
    html += "<div class='card'><h2>Current Values</h2>";
    html += "<div style='display:flex;gap:24px;align-items:flex-start'>";
    html += "<div class='values-col' style='min-width:150px'>";
    html += "<div>Phase A: <b>" + sa + "</b></div>";
    html += "<div>Phase B: <b>" + sb + "</b></div>";
    html += "<div>Phase C: <b>" + sc + "</b></div>";
    html += "<div style='border-top:1px solid #e5e7eb;padding-top:6px;margin-top:4px'>SUM: <b>" + ssum + "</b></div>";
    html += "<div style='font-size:1.15em;margin-top:4px'>P: <b id='power-value'>" + sp + "</b></div>";
    html += "</div>";
    html += "<div style='flex:1;min-width:0'>";
    html += "<canvas id='power-graph' style='width:100%;height:280px;display:block;border-radius:6px'></canvas>";
    html += "</div>";
    html += "</div>";
    html += "</div>";

    html += "<div class='card'><h2>WiFi Configuration</h2><form method='POST' action='/save'><div class='grid'>";
    String displaySsid = (strlen(gRuntimeConfig.wifiSsid) == 0 || gAccessPointMode) ? WIFI_AP_SSID : String(gRuntimeConfig.wifiSsid);
    String displayPass = (strlen(gRuntimeConfig.wifiPass) == 0 || gAccessPointMode) ? WIFI_AP_PASSWORD : String(gRuntimeConfig.wifiPass);
    html += "<div><label>WiFi SSID <small id='wifi-scan-status' style='color:#64748b'>(scanning...)</small></label>";
    html += "<select id='wifi-ssid-select' name='wifi_ssid' style='width:100%'>";
    html += "<option value='" + jsonEscape(displaySsid) + "' selected>" + jsonEscape(displaySsid) + "</option>";
    html += "</select>";
    html += "<div class='row' style='margin-top:4px'><label style='font-size:0.8em'><input type='checkbox' id='wifi-manual-toggle' style='width:auto'> Enter SSID manually</label></div>";
    html += "<input id='wifi-ssid-manual' name='wifi_ssid_manual' placeholder='Hidden SSID...' style='display:none;margin-top:4px'>";
    html += "</div>";
    html += "<div><label>WiFi Password</label><input id='wifi-pass' type='password' name='wifi_pass' value='" + jsonEscape(displayPass) + "'><div class='row'><label><input id='wifi-pass-toggle' type='checkbox' style='width:auto'> Klartext anzeigen</label></div></div>";
    html += "<div><label>Device Hostname</label><input name='hostname' value='" + jsonEscape(String(gRuntimeConfig.hostname)) + "'></div>";
    html += "<div class='full'><button type='submit'>Save WiFi</button></div></div></form></div>";

    html += "<div class='card'><h2>MQTT Configuration</h2><form method='POST' action='/save'><div class='grid'>";
    html += "<div><label>MQTT Server</label><input name='mqtt_server' value='" + jsonEscape(String(gRuntimeConfig.mqttServer)) + "'></div>";
    html += "<div><label>MQTT Port</label><input name='mqtt_port' value='" + String(gRuntimeConfig.mqttPort) + "'></div>";
    html += "<div><label>MQTT Path</label><input name='mqtt_path' value='" + jsonEscape(String(gRuntimeConfig.mqttPath)) + "' placeholder='esp32CTSimulator'></div>";
    html += "<div><label>MQTT User</label><input name='mqtt_user' value='" + jsonEscape(String(gRuntimeConfig.mqttUser)) + "'></div>";
    html += "<div><label>MQTT Password</label><input type='password' name='mqtt_pass' value='" + jsonEscape(String(gRuntimeConfig.mqttPass)) + "'></div>";
    html += "<div class='full'><button type='submit'>Save MQTT</button></div></div></form></div>";

    html += "<div class='card'><h2>Web Access Security</h2><form method='POST' action='/save'><div class='grid'>";
    html += "<div><label>Web Username (optional)</label><input name='web_user' value='" + jsonEscape(String(gRuntimeConfig.webUser)) + "' placeholder='Leave empty to disable'></div>";
    html += "<div><label>Web Password (optional)</label><input type='password' name='web_pass' value='" + jsonEscape(String(gRuntimeConfig.webPass)) + "' placeholder='Leave empty to disable'></div>";
    html += "<small>Leave both empty to disable login protection. When both are set, login will be required.</small>";
    html += "<div class='full'><button type='submit'>Save Security</button></div></div></form></div>";

    html += "<div class='card'><h2>OTA Update</h2><form id='master-ota-form' method='POST' action='/ota' enctype='multipart/form-data'>";
    html += "<input id='master-ota-file' type='file' name='firmware'><br><br><button type='submit'>Upload Firmware</button></form>";
    html += "<div style='margin-top:10px' class='bar'><span id='master-ota-bar'></span></div>";
    html += "<div id='master-ota-text' class='mono' style='margin-top:6px'>status: idle</div></div>";

    html += "<div class='card'><h2>Slave Serial OTA</h2><form id='slave-ota-form' method='POST' action='/slave-ota' enctype='multipart/form-data'>";
    html += "<input id='slave-ota-file' type='file' name='firmware'><br><br><button type='submit'>Flash Slave via UART</button></form>";
    html += "<small>Requires wiring: Master GPIO32 -> Slave EN and Master GPIO33 -> Slave IO0.</small>";
    html += "<small>Flashing is blocked until EN/BOOT self-test has passed in this boot session.</small>";
    html += "<div style='margin-top:10px' class='bar'><span id='slave-ota-bar'></span></div>";
    html += "<div id='slave-ota-text' class='mono' style='margin-top:6px'>status: idle</div>";
    html += "</div>";

    html += "<div class='card'><h2>Slave OTA Wiring Self-Test</h2>";
    html += "<div class='row'><button id='slave-selftest-btn' class='btn-alt' type='button'>Run EN/BOOT Self-Test</button></div>";
    html += "<div id='slave-selftest-text' class='mono' style='margin-top:8px'>self-test: not-run</div>";
    html += "<small>The test toggles slave EN and GPIO0 (BOOT) and checks UART responsiveness.</small>";
    html += "</div>";

    html += "<script>";
    html += "async function pollMasterOta(){try{const r=await fetch('/ota-progress');const d=await r.json();";
    html += "const pct=Math.max(0,Math.min(100,d.progress_pct||0));document.getElementById('master-ota-bar').style.width=pct+'%';";
    html += "document.getElementById('master-ota-text').textContent='status: '+(d.status||'unknown')+' | '+pct.toFixed(1)+'% ('+(d.received_bytes||0)+'/'+(d.total_bytes||0)+')';";
    html += "if(d.last_message){document.getElementById('master-ota-text').textContent+=' | msg: '+d.last_message;}";
    html += "if(d.last_result===true){document.getElementById('master-ota-text').textContent+=' | last: success';}else if(d.last_result===false){document.getElementById('master-ota-text').textContent+=' | last: fail';}}catch(e){}}";
    html += "async function pollSlaveOta(){try{const r=await fetch('/slave-ota-progress');const d=await r.json();";
    html += "const pct=Math.max(0,Math.min(100,d.progress_pct||0));document.getElementById('slave-ota-bar').style.width=pct+'%';";
    html += "const gate=d.self_test_session_passed?'gate: open':'gate: locked';";
    html += "document.getElementById('slave-ota-text').textContent='status: '+(d.status||'unknown')+' | '+pct.toFixed(1)+'% ('+(d.sent_bytes||0)+'/'+(d.total_bytes||0)+') | '+gate;";
    html += "if(d.last_message){document.getElementById('slave-ota-text').textContent+=' | msg: '+d.last_message;}";
    html += "if(d.last_result===true){document.getElementById('slave-ota-text').textContent+=' | last: success';}else if(d.last_result===false){document.getElementById('slave-ota-text').textContent+=' | last: fail';}}catch(e){}}";
    html += "const fmtDuration=(totalSeconds)=>{";
    html += "totalSeconds=Math.max(0,Math.floor(Number(totalSeconds)||0));";
    html += "const SEC_PER_MIN=60,SEC_PER_HOUR=3600,SEC_PER_DAY=86400,SEC_PER_MONTH=30*SEC_PER_DAY,SEC_PER_YEAR=365*SEC_PER_DAY;";
    html += "const years=Math.floor(totalSeconds/SEC_PER_YEAR);totalSeconds%=SEC_PER_YEAR;";
    html += "const months=Math.floor(totalSeconds/SEC_PER_MONTH);totalSeconds%=SEC_PER_MONTH;";
    html += "const days=Math.floor(totalSeconds/SEC_PER_DAY);totalSeconds%=SEC_PER_DAY;";
    html += "const hours=Math.floor(totalSeconds/SEC_PER_HOUR);totalSeconds%=SEC_PER_HOUR;";
    html += "const mins=Math.floor(totalSeconds/SEC_PER_MIN);const secs=totalSeconds%SEC_PER_MIN;";
    html += "const parts=[];if(years)parts.push(years+'y');if(months)parts.push(months+'m');if(days)parts.push(days+'d');if(hours)parts.push(hours+'h');if(mins)parts.push(mins+'min');if(secs||parts.length===0)parts.push(secs+'s');";
    html += "return parts.join(' ');};";
    html += "async function pollMqttStatus(){try{const r=await fetch('/mqtt-status');const d=await r.json();";
    html += "const setPill=(id,text,ok,neutral)=>{const el=document.getElementById(id);if(!el)return;el.textContent=text;el.className=neutral?'status-pill':('status-pill '+(ok?'status-ok':'status-bad'));};";
    html += "setPill('mqtt-session-pill',d.connected?'online':'offline',!!d.connected,false);";
    html += "setPill('mqtt-server-pill',d.server_ok?'OK':'failed',!!d.server_ok,false);";
    html += "setPill('mqtt-login-pill',d.login_configured?(d.login_ok?'OK':'failed'):'not used',!!d.login_ok,!d.login_configured);";
    html += "const stateText=document.getElementById('mqtt-state-text');if(stateText)stateText.textContent=d.state_text||'unknown';";
    html += "const stateCode=document.getElementById('mqtt-state-code');if(stateCode)stateCode.textContent='('+((d.state_code!==undefined)?d.state_code:'n/a')+')';";
    html += "const lastTopic=document.getElementById('mqtt-last-topic');if(lastTopic)lastTopic.textContent=d.last_topic||'-';";
    html += "const lastPayload=document.getElementById('mqtt-last-payload');if(lastPayload)lastPayload.textContent=d.last_payload||'-';";
    html += "const slPill=document.getElementById('slave-link-pill');if(slPill){slPill.textContent=d.slave_connected?'online':'offline';slPill.className=d.slave_connected?'status-pill status-ok':'status-pill status-bad';}";
    html += "const slSince=document.getElementById('slave-online-since');if(slSince){slSince.textContent=d.slave_connected?('Online since '+fmtDuration(d.slave_online_since_s||0)):'';}";
    html += "const slUart=document.getElementById('slave-uart-rx');if(slUart)slUart.textContent='UART bytes='+(d.slave_rx_bytes||0)+' hb='+(d.slave_heartbeat_count||0)+' ack='+(d.slave_ack_count||0);";
    html += "const lastChangeEl=document.getElementById('last-change-time');if(lastChangeEl){lastChangeEl.textContent=d.last_message_age_s>0?('Last Change: '+fmtDuration(d.last_message_age_s)):(d.last_message_ms>0?'Last Change: 0s':'');}";
    html += "}catch(e){}}";
    html += "const uploadWithProgress=(formId,fileId,url,barId,textId,label)=>{const form=document.getElementById(formId);const fileEl=document.getElementById(fileId);if(!form||!fileEl)return;form.addEventListener('submit',(ev)=>{ev.preventDefault();if(!fileEl.files||fileEl.files.length===0){const t=document.getElementById(textId);if(t)t.textContent='status: no file selected';return;}const fd=new FormData();fd.append('firmware',fileEl.files[0]);const xhr=new XMLHttpRequest();xhr.open('POST',url,true);const t=document.getElementById(textId);const b=document.getElementById(barId);if(t)t.textContent='status: uploading...';xhr.upload.onprogress=(e)=>{if(!e.lengthComputable)return;const pct=Math.max(0,Math.min(100,(e.loaded*100.0)/e.total));if(b)b.style.width=pct.toFixed(1)+'%';if(t)t.textContent='status: '+label+' upload '+pct.toFixed(1)+'% ('+e.loaded+'/'+e.total+')';};xhr.onload=()=>{if(b)b.style.width='100%';if(t)t.textContent='status: '+label+' upload finished ('+xhr.status+')';};xhr.onerror=()=>{if(t)t.textContent='status: '+label+' upload failed';};xhr.send(fd);});};";
    html += "uploadWithProgress('master-ota-form','master-ota-file','/ota','master-ota-bar','master-ota-text','master');";
    html += "uploadWithProgress('slave-ota-form','slave-ota-file','/slave-ota','slave-ota-bar','slave-ota-text','slave');";
    html += "document.getElementById('slave-selftest-btn').addEventListener('click',async()=>{";
    html += "const el=document.getElementById('slave-selftest-text');el.textContent='self-test: running...';";
    html += "try{const r=await fetch('/slave-ota-self-test',{method:'POST'});const d=await r.json();el.textContent='self-test: '+(d.ok?'PASS':'FAIL')+' | '+(d.message||'');}catch(e){el.textContent='self-test: request failed';}});";
    html += "const wifiPassToggle=document.getElementById('wifi-pass-toggle');const wifiPassInput=document.getElementById('wifi-pass');";
    html += "if(wifiPassToggle&&wifiPassInput){wifiPassToggle.addEventListener('change',()=>{wifiPassInput.type=wifiPassToggle.checked?'text':'password';});}";
    html += "const wifiManualToggle=document.getElementById('wifi-manual-toggle');";
    html += "const wifiSelect=document.getElementById('wifi-ssid-select');";
    html += "const wifiManualInput=document.getElementById('wifi-ssid-manual');";
    html += "if(wifiManualToggle){wifiManualToggle.addEventListener('change',()=>{";
    html += "const manual=wifiManualToggle.checked;";
    html += "wifiSelect.style.display=manual?'none':'block';wifiSelect.disabled=manual;";
    html += "wifiManualInput.style.display=manual?'block':'none';wifiManualInput.disabled=!manual;";
    html += "if(manual)wifiManualInput.name='wifi_ssid'; else{wifiManualInput.name='wifi_ssid_manual';wifiSelect.name='wifi_ssid';}";
    html += "});}";
    html += "(async()=>{const statusEl=document.getElementById('wifi-scan-status');";
    html += "try{const r=await fetch('/wifi-scan');const d=await r.json();";
    html += "if(d.networks&&d.networks.length>0){";
    html += "const cur=wifiSelect.options[0]?wifiSelect.options[0].value:'';";
    html += "wifiSelect.innerHTML='';";
    html += "d.networks.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{";
    html += "const o=document.createElement('option');o.value=n.ssid;";
    html += "const bar=n.rssi>=-60?'▂▄▆█':n.rssi>=-70?'▂▄▆_':n.rssi>=-80?'▂▄__':'▂___';";
    html += "o.textContent=n.ssid+' '+bar+(n.open?' (open)':'');";
    html += "if(n.ssid===cur)o.selected=true;";
    html += "wifiSelect.appendChild(o);});";
    html += "statusEl.textContent='('+d.count+' networks found)';}else{statusEl.textContent='(no networks found)';}}";
    html += "catch(e){statusEl.textContent='(scan failed)';}})();";
    html += "async function updatePowerGraph(){";
    html += "try{const r=await fetch('/graph');const d=await r.json();";
    html += "const pv=document.getElementById('power-value');";
    html += "if(pv){pv.textContent=(d.power_w!==undefined&&d.power_w>-999)?d.power_w.toFixed(1)+' kW':'---';}";
    html += "const canvas=document.getElementById('power-graph');";
    html += "if(!canvas||!d.history)return;";
    html += "const cw=canvas.clientWidth||600;const ch=canvas.clientHeight||280;";
    html += "canvas.width=cw;canvas.height=ch;";
    html += "const ctx=canvas.getContext('2d');";
    html += "const PL=58,PB=32,PT=16,PR=14;";
    html += "const pw=cw-PL-PR,ph=ch-PB-PT;";
    html += "ctx.fillStyle='#f9fafb';ctx.fillRect(0,0,cw,ch);";
    html += "const hist=d.history||[];";
    html += "const valid=hist.filter(v=>v>-999);";
    html += "let minVal=valid.length?Math.min(...valid):-1;";
    html += "let maxVal=valid.length?Math.max(...valid):1;";
    html += "if(!isFinite(minVal)||!isFinite(maxVal)){minVal=-1;maxVal=1;}";
    html += "if(minVal===maxVal){minVal-=0.5;maxVal+=0.5;}";
    html += "const targetStep=(maxVal-minVal)/4;";
    html += "const niceSteps=[0.1,0.2,0.5,1,2,5,10,20,50];";
    html += "let yStep=niceSteps.find(s=>s>=targetStep)||niceSteps[niceSteps.length-1];";
    html += "let yMin=Math.floor(minVal/yStep)*yStep;";
    html += "let yMax=Math.ceil(maxVal/yStep)*yStep;";
    html += "yMin=Math.min(yMin,0);yMax=Math.max(yMax,0);";
    html += "if(yMin===yMax){yMin-=yStep;yMax+=yStep;}";
    html += "const yTickCount=Math.max(1,Math.min(4,Math.round((yMax-yMin)/yStep)));";
    html += "const yLabelDecimals=yStep<1?1:0;";
    html += "const mapY=(v)=>PT+ph-(((v-yMin)/(yMax-yMin))*ph);";
    html += "ctx.font='11px Verdana,sans-serif';";
    html += "ctx.strokeStyle='#e5e7eb';ctx.lineWidth=1;ctx.setLineDash([3,3]);";
    html += "for(let i=0;i<=yTickCount;i++){";
    html += "const v=yMin+i*yStep;";
    html += "const y=mapY(v);";
    html += "ctx.beginPath();ctx.moveTo(PL,y);ctx.lineTo(PL+pw,y);ctx.stroke();";
    html += "ctx.fillStyle='#6b7280';ctx.textAlign='right';ctx.textBaseline='middle';";
    html += "ctx.fillText(v.toFixed(yLabelDecimals)+' kW',PL-5,y);}";
    html += "ctx.setLineDash([]);";
    html += "const yZero=mapY(0);";
    html += "const totalMs=(d.samples||128)*(d.interval_ms||2344);";
    html += "const xTicks=5;";
    html += "ctx.textAlign='center';ctx.fillStyle='#6b7280';";
    html += "for(let i=0;i<=xTicks;i++){";
    html += "const x=PL+(i/xTicks)*pw;";
    html += "const minAgo=Math.round((totalMs-(i/xTicks)*totalMs)/60000);";
    html += "ctx.strokeStyle='#9ca3af';ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(x,yZero-3);ctx.lineTo(x,yZero+3);ctx.stroke();";
    html += "const label=i===xTicks?'now':'-'+minAgo+'m';";
    html += "if(yZero+16<=PT+ph){ctx.textBaseline='top';ctx.fillText(label,x,yZero+5);}else{ctx.textBaseline='bottom';ctx.fillText(label,x,yZero-5);}";
    html += "}";
    html += "if(hist.length>0){";
    html += "const xStep=hist.length>1?pw/(hist.length-1):0;";
    html += "for(let i=0;i<hist.length;i++){";
    html += "const v=hist[i];if(v<=-999)continue;";
    html += "const x=PL+i*xStep;const y=mapY(v);";
    html += "ctx.beginPath();ctx.moveTo(x,yZero);ctx.lineTo(x,y);";
    html += "ctx.strokeStyle=v>=0?'rgba(22,163,74,0.16)':'rgba(220,38,38,0.16)';ctx.lineWidth=1;ctx.stroke();}";
    html += "let prevOk=false,prevX=0,prevY=0,prevV=0;";
    html += "ctx.lineWidth=2.2;";
    html += "for(let i=0;i<hist.length;i++){";
    html += "const v=hist[i];";
    html += "if(v<=-999){prevOk=false;continue;}";
    html += "const x=PL+i*xStep;const y=mapY(v);";
    html += "if(prevOk){";
    html += "const segColor=(prevV>=0&&v>=0)?'#16a34a':(prevV<0&&v<0)?'#dc2626':'#6b7280';";
    html += "ctx.beginPath();ctx.moveTo(prevX,prevY);ctx.lineTo(x,y);ctx.strokeStyle=segColor;ctx.stroke();}";
    html += "prevOk=true;prevX=x;prevY=y;prevV=v;}";
    html += "for(let i=0;i<hist.length;i++){";
    html += "const v=hist[i];if(v<=-999)continue;";
    html += "const x=PL+i*xStep;const y=mapY(v);";
    html += "ctx.fillStyle=v>=0?'#16a34a':'#dc2626';";
    html += "ctx.beginPath();ctx.arc(x,y,1.6,0,Math.PI*2);ctx.fill();}}";
    html += "ctx.strokeStyle='#374151';ctx.lineWidth=1.5;ctx.setLineDash([]);";
    html += "ctx.beginPath();ctx.moveTo(PL,PT);ctx.lineTo(PL,PT+ph);ctx.moveTo(PL,yZero);ctx.lineTo(PL+pw,yZero);ctx.stroke();";
    html += "ctx.save();ctx.translate(13,PT+ph/2);ctx.rotate(-Math.PI/2);";
    html += "ctx.textAlign='center';ctx.textBaseline='middle';ctx.fillStyle='#374151';";
    html += "ctx.font='11px Verdana,sans-serif';ctx.fillText('Power (kW)',0,0);ctx.restore();";
    html += "ctx.textAlign='center';ctx.textBaseline='bottom';ctx.fillStyle='#374151';";
    html += "ctx.fillText('Time',PL+pw/2,ch-2);}catch(e){}}";
    html += "updatePowerGraph();setInterval(updatePowerGraph,2500);";
    html += "pollMasterOta();setInterval(pollMasterOta,1000);pollSlaveOta();setInterval(pollSlaveOta,1000);pollMqttStatus();setInterval(()=>{updatePowerGraph();pollMqttStatus();},2500);";
    html += "</script>";
    html += "</div></body></html>";
    return html;
}

static String mqttStatusJson() {
    bool connected = false;
    bool serverOk = false;
    bool loginOk = false;
    bool loginConfigured = false;
    String stateText = "not initialized";
    int stateCode = -999;

    if (gMQTT != nullptr) {
        connected = gMQTT->isConnected();
        serverOk = gMQTT->isServerReachable();
        loginOk = gMQTT->isLoginOk();
        loginConfigured = gMQTT->isLoginConfigured();
        stateText = String(gMQTT->getConnectionStateText());
        stateCode = gMQTT->getConnectionState();
    }

    unsigned long slaveOnlineSinceS = (slaveConnected && gSlaveOnlineSinceMs > 0) ? ((millis() - gSlaveOnlineSinceMs) / 1000UL) : 0;

    String body = "{";
    body += "\"connected\":" + String(connected ? "true" : "false") + ",";
    body += "\"server_ok\":" + String(serverOk ? "true" : "false") + ",";
    body += "\"login_ok\":" + String(loginOk ? "true" : "false") + ",";
    body += "\"login_configured\":" + String(loginConfigured ? "true" : "false") + ",";
    body += "\"state_text\":\"" + jsonEscape(stateText) + "\",";
    body += "\"state_code\":" + String(stateCode) + ",";
    body += "\"last_topic\":\"" + jsonEscape(String(gLastMqttTopic)) + "\",";
    body += "\"last_payload\":\"" + jsonEscape(String(gLastMqttPayload)) + "\",";
    // Expose both raw receive time and derived age so the web UI can refresh
    // relative timestamps without inferring device uptime on the client.
    unsigned long lastMessageAgeS = (gLastMqttMessageMs > 0) ? ((millis() - gLastMqttMessageMs) / 1000UL) : 0;
    body += "\"last_message_ms\":" + String(gLastMqttMessageMs) + ",";
    body += "\"last_message_age_s\":" + String(lastMessageAgeS) + ",";
    body += "\"server\":\"" + jsonEscape(String(gRuntimeConfig.mqttServer)) + "\",";
    body += "\"port\":" + String(gRuntimeConfig.mqttPort) + ",";
    body += "\"slave_connected\":" + String(slaveConnected ? "true" : "false") + ",";
    body += "\"slave_online_since_s\":" + String(slaveOnlineSinceS) + ",";
    body += "\"slave_rx_bytes\":" + String(slaveRxByteCount) + ",";
    body += "\"slave_heartbeat_count\":" + String(slaveHeartbeatCount) + ",";
    body += "\"slave_ack_count\":" + String(slaveAckCount);
    body += "}";
    return body;
}

static void updateSlaveOnlineSince() {
    if (slaveConnected && !gSlaveConnectedPrev) {
        gSlaveOnlineSinceMs = millis();
    } else if (!slaveConnected) {
        gSlaveOnlineSinceMs = 0;
    }
    gSlaveConnectedPrev = slaveConnected;
}

static String buildRebootingHtml() {
    String html;
    html.reserve(1200);
    html += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>Rebooting</title><style>body{font-family:Verdana,sans-serif;background:linear-gradient(135deg,#f5f8ff,#e6fff0);margin:0;padding:24px;color:#1f2937}.wrap{max-width:700px;margin:auto}.card{background:#fff;border-radius:14px;padding:20px;box-shadow:0 8px 24px rgba(0,0,0,.08)}h1{margin:0 0 8px 0}.mono{font-family:monospace;color:#0f172a}</style></head><body><div class='wrap'><div class='card'>";
    html += "<h1>Rebooting ESP32</h1>";
    html += "<p>Settings saved for WLAN/MQTT. The device is rebooting now.</p>";
    html += "<p class='mono'>Returning to main view in <span id='sec'>30</span>s...</p>";
    html += "<p>If auto-refresh does not work, open <a href='/'>main view</a> manually.</p>";
    html += "</div></div><script>let s=30;const el=document.getElementById('sec');const t=setInterval(()=>{s--;if(el){el.textContent=String(Math.max(0,s));}if(s<=0){clearInterval(t);window.location.href='/';}},1000);</script></body></html>";
    return html;
}

static void requestRuntimeReconfigure() {
    gReconfigureRequested = true;
}

static void setupWebServer() {
    if (gWebServerStarted) {
        return;
    }

    gWebServer.on("/", HTTP_GET, []() {
        gWebServer.send(200, "text/html", buildDashboardHtml());
    });

    gWebServer.on("/health", HTTP_GET, []() {
        String ip = gAccessPointMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        String body = "{";
        body += "\"status\":\"ok\",";
        body += "\"ap_mode\":" + String(gAccessPointMode ? "true" : "false") + ",";
        body += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
        body += "\"mqtt_connected\":" + String((gMQTT && gMQTT->isConnected()) ? "true" : "false") + ",";
        body += "\"mqtt_server_ok\":" + String((gMQTT && gMQTT->isServerReachable()) ? "true" : "false") + ",";
        body += "\"mqtt_login_ok\":" + String((gMQTT && gMQTT->isLoginOk()) ? "true" : "false") + ",";
        body += "\"mqtt_login_configured\":" + String((gMQTT && gMQTT->isLoginConfigured()) ? "true" : "false") + ",";
        body += "\"mqtt_state\":\"" + jsonEscape(gMQTT ? String(gMQTT->getConnectionStateText()) : String("not initialized")) + "\",";
        body += "\"slave_connected\":" + String(slaveConnected ? "true" : "false") + ",";
        body += "\"slave_rx_bytes\":" + String(slaveRxByteCount) + ",";
        body += "\"slave_heartbeat_count\":" + String(slaveHeartbeatCount) + ",";
        body += "\"slave_ack_count\":" + String(slaveAckCount) + ",";
        body += "\"ip\":\"" + jsonEscape(ip) + "\",";
        body += "\"uptime_s\":" + String((unsigned long)(millis() / 1000UL));
        body += "}";
        gWebServer.send(200, "application/json", body);
    });

    gWebServer.on("/mqtt-status", HTTP_GET, []() {
        gWebServer.send(200, "application/json", mqttStatusJson());
    });

    // Dynamic /{mqtt_path}/status endpoint for MQTT phase data
    gWebServer.onNotFound([]() {
        String path = gWebServer.uri();
        if (path.startsWith("/") && path.endsWith("/status")) {
            String urlPath = path.substring(1, path.length() - 7);  // Extract path from /path/status
            
            // Get short name from configured mqtt_path (text after last "/")
            String configPath = String(gRuntimeConfig.mqttPath);
            while (configPath.endsWith("/")) {
                configPath.remove(configPath.length() - 1);
            }
            int lastSlash = configPath.lastIndexOf('/');
            String shortName = (lastSlash >= 0) ? configPath.substring(lastSlash + 1) : configPath;
            
            if (urlPath == shortName) {
                float a = CURRENT_DEFAULT, b = CURRENT_DEFAULT, c = CURRENT_DEFAULT;
                if (gMQTT) {
                    a = gMQTT->getCurrentPhaseA();
                    b = gMQTT->getCurrentPhaseB();
                    c = gMQTT->getCurrentPhaseC();
                }
                String json = "{";
                json += "\"phase_a_a\":" + String(a, 2) + ",";
                json += "\"phase_b_a\":" + String(b, 2) + ",";
                json += "\"phase_c_a\":" + String(c, 2) + ",";
                json += "\"sum_a\":" + String(a + b + c, 2) + ",";
                json += "\"slave_connected\":" + String(slaveConnected ? "true" : "false") + ",";
                json += "\"timestamp_ms\":" + String((unsigned long)millis());
                json += "}";
                gWebServer.send(200, "application/json", json);
                return;
            }
        }
        gWebServer.send(404, "text/plain", "Not Found");
    });

    gWebServer.on("/values", HTTP_GET, []() {
        float a = CURRENT_DEFAULT, b = CURRENT_DEFAULT, c = CURRENT_DEFAULT;
        if (gMQTT) {
            a = gMQTT->getCurrentPhaseA();
            b = gMQTT->getCurrentPhaseB();
            c = gMQTT->getCurrentPhaseC();
        }
        String body = "{";
        body += "\"phase_a\":" + String(a, 3) + ",";
        body += "\"phase_b\":" + String(b, 3) + ",";
        body += "\"phase_c\":" + String(c, 3) + ",";
        body += "\"timestamp_ms\":" + String((unsigned long)millis());
        body += "}";
        gWebServer.send(200, "application/json", body);
    });

    gWebServer.on("/wifi-scan", HTTP_GET, []() {
        int count = WiFi.scanNetworks(false, true);
        String body = "{";
        body += "\"count\":" + String(count) + ",\"networks\":[";
        for (int i = 0; i < count; i++) {
            if (i > 0) body += ",";
            body += "{";
            body += "\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",";
            body += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
            body += "\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false");
            body += "}";
        }
        body += "]}";
        gWebServer.send(200, "application/json", body);
    });

    gWebServer.on("/ota-progress", HTTP_GET, []() {
        float pct = 0.0f;
        if (gMasterOtaTotalBytes > 0) {
            pct = ((float)gMasterOtaReceivedBytes * 100.0f) / (float)gMasterOtaTotalBytes;
            if (pct > 100.0f) pct = 100.0f;
        }

        String body = "{";
        body += "\"in_progress\":" + String(gMasterOtaInProgress ? "true" : "false") + ",";
        body += "\"status\":\"" + String(gMasterOtaStatus) + "\",";
        body += "\"received_bytes\":" + String(gMasterOtaReceivedBytes) + ",";
        body += "\"total_bytes\":" + String(gMasterOtaTotalBytes) + ",";
        body += "\"progress_pct\":" + String(pct, 2) + ",";
        body += "\"last_result\":" + String(gMasterOtaLastResult ? "true" : "false") + ",";
        body += "\"last_message\":\"" + jsonEscape(String(gMasterOtaLastMessage)) + "\"";
        body += "}";
        gWebServer.send(200, "application/json", body);
    });

    gWebServer.on("/slave-ota-progress", HTTP_GET, []() {
        float pct = 0.0f;
        if (gSlaveOtaTotalBytes > 0) {
            pct = ((float)gSlaveOtaSentBytes * 100.0f) / (float)gSlaveOtaTotalBytes;
            if (pct > 100.0f) pct = 100.0f;
        }

        String body = "{";
        body += "\"in_progress\":" + String(gSlaveOtaInProgress ? "true" : "false") + ",";
        body += "\"status\":\"" + String(gSlaveOtaStatus) + "\",";
        body += "\"sent_bytes\":" + String(gSlaveOtaSentBytes) + ",";
        body += "\"total_bytes\":" + String(gSlaveOtaTotalBytes) + ",";
        body += "\"progress_pct\":" + String(pct, 2) + ",";
        body += "\"last_result\":" + String(gSlaveOtaLastResult ? "true" : "false") + ",";
        body += "\"self_test_session_passed\":" + String(gSlaveOtaSelfTestSessionPassed ? "true" : "false") + ",";
        body += "\"self_test_last_result\":" + String(gSlaveOtaSelfTestLastResult ? "true" : "false") + ",";
        body += "\"self_test_message\":\"" + jsonEscape(String(gSlaveOtaSelfTestMessage)) + "\",";
        body += "\"last_message\":\"" + jsonEscape(String(gSlaveOtaLastMessage)) + "\"";
        body += "}";
        gWebServer.send(200, "application/json", body);
    });

    gWebServer.on("/slave-ota-self-test", HTTP_POST, []() {
        String detail;
        const bool ok = runSlaveOtaSelfTest(detail);
        gSlaveOtaSelfTestLastResult = ok;
        if (ok) {
            gSlaveOtaSelfTestSessionPassed = true;
        }
        strncpy(gSlaveOtaSelfTestMessage, detail.c_str(), sizeof(gSlaveOtaSelfTestMessage) - 1);
        gSlaveOtaSelfTestMessage[sizeof(gSlaveOtaSelfTestMessage) - 1] = '\0';

        String body = "{";
        body += "\"ok\":" + String(ok ? "true" : "false") + ",";
        body += "\"message\":\"" + jsonEscape(detail) + "\"";
        body += "}";
        gWebServer.send(ok ? 200 : 500, "application/json", body);
    });

    gWebServer.on("/save", HTTP_POST, []() {
        const bool rebootRequested =
            gWebServer.hasArg("wifi_ssid") ||
            gWebServer.hasArg("wifi_pass") ||
            gWebServer.hasArg("hostname") ||
            gWebServer.hasArg("mqtt_server") ||
            gWebServer.hasArg("mqtt_port") ||
            gWebServer.hasArg("mqtt_path") ||
            gWebServer.hasArg("mqtt_user") ||
            gWebServer.hasArg("mqtt_pass");

        if (gWebServer.hasArg("wifi_ssid")) {
            strncpy(gRuntimeConfig.wifiSsid, gWebServer.arg("wifi_ssid").c_str(), sizeof(gRuntimeConfig.wifiSsid) - 1);
            gRuntimeConfig.wifiSsid[sizeof(gRuntimeConfig.wifiSsid) - 1] = '\0';
        }
        if (gWebServer.hasArg("wifi_pass")) {
            strncpy(gRuntimeConfig.wifiPass, gWebServer.arg("wifi_pass").c_str(), sizeof(gRuntimeConfig.wifiPass) - 1);
            gRuntimeConfig.wifiPass[sizeof(gRuntimeConfig.wifiPass) - 1] = '\0';
        }
        if (gWebServer.hasArg("hostname")) {
            strncpy(gRuntimeConfig.hostname, gWebServer.arg("hostname").c_str(), sizeof(gRuntimeConfig.hostname) - 1);
            gRuntimeConfig.hostname[sizeof(gRuntimeConfig.hostname) - 1] = '\0';
            WiFi.setHostname(gRuntimeConfig.hostname);
        }
        if (gWebServer.hasArg("mqtt_server")) {
            strncpy(gRuntimeConfig.mqttServer, gWebServer.arg("mqtt_server").c_str(), sizeof(gRuntimeConfig.mqttServer) - 1);
            gRuntimeConfig.mqttServer[sizeof(gRuntimeConfig.mqttServer) - 1] = '\0';
        }
        if (gWebServer.hasArg("mqtt_port")) {
            int port = gWebServer.arg("mqtt_port").toInt();
            gRuntimeConfig.mqttPort = (port > 0 && port <= 65535) ? (uint16_t)port : MQTT_PORT;
        }
        if (gWebServer.hasArg("mqtt_path")) {
            strncpy(gRuntimeConfig.mqttPath, gWebServer.arg("mqtt_path").c_str(), sizeof(gRuntimeConfig.mqttPath) - 1);
            gRuntimeConfig.mqttPath[sizeof(gRuntimeConfig.mqttPath) - 1] = '\0';
            if (strlen(gRuntimeConfig.mqttPath) == 0) {
                strncpy(gRuntimeConfig.mqttPath, "esp32CTSimulator", sizeof(gRuntimeConfig.mqttPath) - 1);
                gRuntimeConfig.mqttPath[sizeof(gRuntimeConfig.mqttPath) - 1] = '\0';
            }
        }
        if (gWebServer.hasArg("mqtt_user")) {
            strncpy(gRuntimeConfig.mqttUser, gWebServer.arg("mqtt_user").c_str(), sizeof(gRuntimeConfig.mqttUser) - 1);
            gRuntimeConfig.mqttUser[sizeof(gRuntimeConfig.mqttUser) - 1] = '\0';
        }
        if (gWebServer.hasArg("mqtt_pass")) {
            strncpy(gRuntimeConfig.mqttPass, gWebServer.arg("mqtt_pass").c_str(), sizeof(gRuntimeConfig.mqttPass) - 1);
            gRuntimeConfig.mqttPass[sizeof(gRuntimeConfig.mqttPass) - 1] = '\0';
        }
        if (gWebServer.hasArg("web_user")) {
            strncpy(gRuntimeConfig.webUser, gWebServer.arg("web_user").c_str(), sizeof(gRuntimeConfig.webUser) - 1);
            gRuntimeConfig.webUser[sizeof(gRuntimeConfig.webUser) - 1] = '\0';
        }
        if (gWebServer.hasArg("web_pass")) {
            strncpy(gRuntimeConfig.webPass, gWebServer.arg("web_pass").c_str(), sizeof(gRuntimeConfig.webPass) - 1);
            gRuntimeConfig.webPass[sizeof(gRuntimeConfig.webPass) - 1] = '\0';
        }

        saveRuntimeConfig();
        if (gMQTT) {
            gMQTT->setConnectionConfig(
                gRuntimeConfig.mqttServer,
                gRuntimeConfig.mqttPort,
                gRuntimeConfig.mqttUser,
                gRuntimeConfig.mqttPass
            );
        }
        if (rebootRequested) {
            gWebServer.send(200, "text/html", buildRebootingHtml());
            delay(250);
            ESP.restart();
            return;
        }

        requestRuntimeReconfigure();
        gWebServer.send(200, "text/html", "<html><body><h2>Saved.</h2><a href='/'>Back</a></body></html>");
    });

    gWebServer.on("/ota", HTTP_GET, []() {
        String page = "<html><body><h2>OTA Update</h2><form method='POST' action='/ota' enctype='multipart/form-data'>";
        page += "<input type='file' name='firmware'><button type='submit'>Upload</button></form><a href='/'>Back</a></body></html>";
        gWebServer.send(200, "text/html", page);
    });

    gWebServer.on(
        "/ota",
        HTTP_POST,
        []() {
            bool ok = !Update.hasError();
            gMasterOtaInProgress = false;
            gMasterOtaLastResult = ok;
            if (ok) {
                gMasterOtaReceivedBytes = gMasterOtaTotalBytes;
                strncpy(gMasterOtaStatus, "done", sizeof(gMasterOtaStatus) - 1);
                gMasterOtaStatus[sizeof(gMasterOtaStatus) - 1] = '\0';
                strncpy(gMasterOtaLastMessage, "OTA success, rebooting", sizeof(gMasterOtaLastMessage) - 1);
                gMasterOtaLastMessage[sizeof(gMasterOtaLastMessage) - 1] = '\0';
            } else {
                strncpy(gMasterOtaStatus, "failed", sizeof(gMasterOtaStatus) - 1);
                gMasterOtaStatus[sizeof(gMasterOtaStatus) - 1] = '\0';
                strncpy(gMasterOtaLastMessage, "OTA failed", sizeof(gMasterOtaLastMessage) - 1);
                gMasterOtaLastMessage[sizeof(gMasterOtaLastMessage) - 1] = '\0';
            }
            gWebServer.send(ok ? 200 : 500, "text/plain", ok ? "OTA success, rebooting" : "OTA failed");
            delay(200);
            if (ok) {
                ESP.restart();
            }
        },
        []() {
            HTTPUpload& upload = gWebServer.upload();
            if (upload.status == UPLOAD_FILE_START) {
                gMasterOtaInProgress = true;
                gMasterOtaLastResult = false;
                gMasterOtaTotalBytes = upload.totalSize;
                gMasterOtaReceivedBytes = 0;
                strncpy(gMasterOtaStatus, "starting", sizeof(gMasterOtaStatus) - 1);
                gMasterOtaStatus[sizeof(gMasterOtaStatus) - 1] = '\0';
                strncpy(gMasterOtaLastMessage, "upload started", sizeof(gMasterOtaLastMessage) - 1);
                gMasterOtaLastMessage[sizeof(gMasterOtaLastMessage) - 1] = '\0';
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                    gMasterOtaInProgress = false;
                    strncpy(gMasterOtaStatus, "start-failed", sizeof(gMasterOtaStatus) - 1);
                    gMasterOtaStatus[sizeof(gMasterOtaStatus) - 1] = '\0';
                    strncpy(gMasterOtaLastMessage, "Update.begin failed", sizeof(gMasterOtaLastMessage) - 1);
                    gMasterOtaLastMessage[sizeof(gMasterOtaLastMessage) - 1] = '\0';
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (!gMasterOtaInProgress) {
                    return;
                }
                size_t written = Update.write(upload.buf, upload.currentSize);
                if (written != upload.currentSize) {
                    Update.printError(Serial);
                    gMasterOtaInProgress = false;
                    strncpy(gMasterOtaStatus, "write-failed", sizeof(gMasterOtaStatus) - 1);
                    gMasterOtaStatus[sizeof(gMasterOtaStatus) - 1] = '\0';
                    strncpy(gMasterOtaLastMessage, "flash write failed", sizeof(gMasterOtaLastMessage) - 1);
                    gMasterOtaLastMessage[sizeof(gMasterOtaLastMessage) - 1] = '\0';
                } else {
                    gMasterOtaReceivedBytes += (uint32_t)upload.currentSize;
                    strncpy(gMasterOtaStatus, "writing", sizeof(gMasterOtaStatus) - 1);
                    gMasterOtaStatus[sizeof(gMasterOtaStatus) - 1] = '\0';
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                if (!gMasterOtaInProgress) {
                    return;
                }
                if (!Update.end(true)) {
                    Update.printError(Serial);
                    gMasterOtaInProgress = false;
                    strncpy(gMasterOtaStatus, "end-failed", sizeof(gMasterOtaStatus) - 1);
                    gMasterOtaStatus[sizeof(gMasterOtaStatus) - 1] = '\0';
                    strncpy(gMasterOtaLastMessage, "finalize failed", sizeof(gMasterOtaLastMessage) - 1);
                    gMasterOtaLastMessage[sizeof(gMasterOtaLastMessage) - 1] = '\0';
                } else {
                    strncpy(gMasterOtaStatus, "finalizing", sizeof(gMasterOtaStatus) - 1);
                    gMasterOtaStatus[sizeof(gMasterOtaStatus) - 1] = '\0';
                    strncpy(gMasterOtaLastMessage, "final checks", sizeof(gMasterOtaLastMessage) - 1);
                    gMasterOtaLastMessage[sizeof(gMasterOtaLastMessage) - 1] = '\0';
                }
            }
        }
    );

    gWebServer.on(
        "/slave-ota",
        HTTP_POST,
        []() {
            bool ok = gSlaveOtaLastResult;
            const String message = ok ? "Slave OTA finished" : String("Slave OTA failed: ") + String(gSlaveOtaLastMessage);
            gWebServer.send(ok ? 200 : 500, "text/plain", message);
        },
        []() {
            HTTPUpload& upload = gWebServer.upload();
            if (upload.status == UPLOAD_FILE_START) {
                if (!gSlaveOtaSelfTestSessionPassed) {
                    gSlaveOtaInProgress = false;
                    gSlaveOtaLastResult = false;
                    strncpy(gSlaveOtaStatus, "self-test-required", sizeof(gSlaveOtaStatus) - 1);
                    gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
                    strncpy(gSlaveOtaLastMessage, "run /slave-ota-self-test and pass before flashing", sizeof(gSlaveOtaLastMessage) - 1);
                    gSlaveOtaLastMessage[sizeof(gSlaveOtaLastMessage) - 1] = '\0';
                    if (DEBUG_ENABLED) {
                        Serial.println("Slave OTA blocked: self-test not passed in this boot session");
                    }
                    return;
                }

                if (DEBUG_ENABLED) {
                    Serial.println("Slave OTA upload started");
                }
                gSlaveOtaInProgress = true;
                gSlaveOtaLastResult = false;
                gSlaveOtaSeq = 0;
                gSlaveOtaTotalBytes = upload.totalSize;
                gSlaveOtaSentBytes = 0;
                strncpy(gSlaveOtaStatus, "starting", sizeof(gSlaveOtaStatus) - 1);
                gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
                strncpy(gSlaveOtaLastMessage, "upload in progress", sizeof(gSlaveOtaLastMessage) - 1);
                gSlaveOtaLastMessage[sizeof(gSlaveOtaLastMessage) - 1] = '\0';
                pinMode(SLAVE_EN_CONTROL_PIN, OUTPUT);
                pinMode(SLAVE_BOOT_CONTROL_PIN, OUTPUT);
                digitalWrite(SLAVE_EN_CONTROL_PIN, HIGH);
                digitalWrite(SLAVE_BOOT_CONTROL_PIN, HIGH);
                delay(5);
                digitalWrite(SLAVE_BOOT_CONTROL_PIN, LOW);
                digitalWrite(SLAVE_EN_CONTROL_PIN, LOW);
                delay(50);
                digitalWrite(SLAVE_EN_CONTROL_PIN, HIGH);
                delay(50);
                digitalWrite(SLAVE_BOOT_CONTROL_PIN, HIGH);
                delay(100);

                if (!sendSlaveOtaStart(upload.totalSize)) {
                    gSlaveOtaInProgress = false;
                    gSlaveOtaLastResult = false;
                    strncpy(gSlaveOtaStatus, "start-failed", sizeof(gSlaveOtaStatus) - 1);
                    gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
                    strncpy(gSlaveOtaLastMessage, "slave did not ACK OTA start", sizeof(gSlaveOtaLastMessage) - 1);
                    gSlaveOtaLastMessage[sizeof(gSlaveOtaLastMessage) - 1] = '\0';
                    if (DEBUG_ENABLED) {
                        Serial.println("Slave OTA start failed");
                    }
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (!gSlaveOtaInProgress) {
                    return;
                }

                size_t offset = 0;
                while (offset < upload.currentSize) {
                    const size_t chunk = min((size_t)OTA_MAX_PAYLOAD - 2, upload.currentSize - offset);
                    if (!sendSlaveOtaDataChunk(upload.buf + offset, (uint16_t)chunk)) {
                        gSlaveOtaInProgress = false;
                        gSlaveOtaLastResult = false;
                        strncpy(gSlaveOtaStatus, "data-failed", sizeof(gSlaveOtaStatus) - 1);
                        gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
                        strncpy(gSlaveOtaLastMessage, "slave did not ACK data chunk", sizeof(gSlaveOtaLastMessage) - 1);
                        gSlaveOtaLastMessage[sizeof(gSlaveOtaLastMessage) - 1] = '\0';
                        if (DEBUG_ENABLED) {
                            Serial.println("Slave OTA data transfer failed");
                        }
                        break;
                    }
                    offset += chunk;
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                if (gSlaveOtaInProgress) {
                    bool endOk = sendSlaveOtaEnd();
                    gSlaveOtaLastResult = endOk;
                    gSlaveOtaInProgress = false;
                    gSlaveOtaSentBytes = gSlaveOtaTotalBytes;
                    if (!endOk) {
                        strncpy(gSlaveOtaStatus, "finalize-failed", sizeof(gSlaveOtaStatus) - 1);
                        gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
                        strncpy(gSlaveOtaLastMessage, "slave rejected OTA end/finalization", sizeof(gSlaveOtaLastMessage) - 1);
                        gSlaveOtaLastMessage[sizeof(gSlaveOtaLastMessage) - 1] = '\0';
                        if (DEBUG_ENABLED) {
                            Serial.println("Slave OTA finalize failed");
                        }
                    } else {
                        strncpy(gSlaveOtaLastMessage, "success", sizeof(gSlaveOtaLastMessage) - 1);
                        gSlaveOtaLastMessage[sizeof(gSlaveOtaLastMessage) - 1] = '\0';
                        if (DEBUG_ENABLED) {
                            Serial.println("Slave OTA transfer complete");
                        }
                    }
                }
            } else if (upload.status == UPLOAD_FILE_ABORTED) {
                gSlaveOtaInProgress = false;
                gSlaveOtaLastResult = false;
                strncpy(gSlaveOtaStatus, "aborted", sizeof(gSlaveOtaStatus) - 1);
                gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
                strncpy(gSlaveOtaLastMessage, "upload aborted by client", sizeof(gSlaveOtaLastMessage) - 1);
                gSlaveOtaLastMessage[sizeof(gSlaveOtaLastMessage) - 1] = '\0';
                if (DEBUG_ENABLED) {
                    Serial.println("Slave OTA upload aborted");
                }
            }
        }
    );

    auto redirectToPortal = []() {
        String target = "http://" + WiFi.softAPIP().toString() + "/";
        gWebServer.sendHeader("Location", target, true);
        gWebServer.send(302, "text/plain", "Redirecting to portal");
    };

    gWebServer.on("/generate_204", HTTP_GET, redirectToPortal);
    gWebServer.on("/hotspot-detect.html", HTTP_GET, redirectToPortal);
    gWebServer.on("/graph", HTTP_GET, []() {
        String body = "{";
        body += "\"power_w\":" + String(gLastReceivedSumWatt, 2) + ",";
        float maxVal = 1.0f;
        for (int i = 0; i < GRAPH_SAMPLES; i++) {
            float v = graphHistory[i];
            if (v != CURRENT_DEFAULT && v > maxVal) maxVal = v;
        }
        body += "\"max_w\":" + String(maxVal, 2) + ",";
        body += "\"samples\":" + String(GRAPH_SAMPLES) + ",";
        body += "\"interval_ms\":" + String(GRAPH_INTERVAL_MS) + ",";
        body += "\"history\":[";
        for (int i = 0; i < GRAPH_SAMPLES; i++) {
            if (i > 0) body += ",";
            int idx = ((int)graphHead + i) % GRAPH_SAMPLES;
            float v = graphHistory[idx];
            body += String(v == CURRENT_DEFAULT ? -999 : v, 2);
        }
        body += "]}";
        gWebServer.send(200, "application/json", body);
    });

    gWebServer.on("/connecttest.txt", HTTP_GET, redirectToPortal);
    gWebServer.on("/ncsi.txt", HTTP_GET, redirectToPortal);

    gWebServer.onNotFound([]() {
        if (gAccessPointMode) {
            String target = "http://" + WiFi.softAPIP().toString() + "/";
            gWebServer.sendHeader("Location", target, true);
            gWebServer.send(302, "text/plain", "Portal redirect");
            return;
        }
        gWebServer.send(404, "text/plain", "Not found");
    });

    gWebServer.begin();
    gWebServerStarted = true;
}

static void ensureNetworkServices() {
    if (gAccessPointMode && !gCaptiveDnsStarted) {
        gDnsServer.start(53, "*", WiFi.softAPIP());
        gCaptiveDnsStarted = true;
    }
    if (!gAccessPointMode && gCaptiveDnsStarted) {
        gDnsServer.stop();
        gCaptiveDnsStarted = false;
    }
}

static void serviceNetworkServices() {
    if (gWebServerStarted) {
        gWebServer.handleClient();
    }
    if (gCaptiveDnsStarted) {
        gDnsServer.processNextRequest();
    }
}

static uint16_t computeOtaFrameCrc(uint8_t type, uint8_t len, const uint8_t* payload) {
    uint8_t buffer[2 + OTA_MAX_PAYLOAD];
    buffer[0] = type;
    buffer[1] = len;
    if (len > 0 && payload != nullptr) {
        memcpy(buffer + 2, payload, len);
    }
    return UARTProtocol::calculateCRC16(buffer, (size_t)(2 + len));
}

static size_t buildOtaFrame(uint8_t type, const uint8_t* payload, uint8_t payloadLen, uint8_t* out, size_t outSize) {
    const size_t frameSize = (size_t)payloadLen + 6;
    if (payloadLen > OTA_MAX_PAYLOAD || out == nullptr || outSize < frameSize) {
        return 0;
    }

    out[0] = OTA_SYNC_BYTE;
    out[1] = type;
    out[2] = payloadLen;
    if (payloadLen > 0 && payload != nullptr) {
        memcpy(out + 3, payload, payloadLen);
    }
    const uint16_t crc = computeOtaFrameCrc(type, payloadLen, payload);
    out[3 + payloadLen] = (uint8_t)(crc & 0xFF);
    out[4 + payloadLen] = (uint8_t)((crc >> 8) & 0xFF);
    out[5 + payloadLen] = OTA_END_BYTE;
    return frameSize;
}

static bool readNextOtaFrame(uint8_t& typeOut, uint8_t* payloadOut, uint8_t& payloadLenOut, unsigned long timeoutMs) {
    bool inFrame = false;
    uint8_t frameType = 0;
    bool frameTypeRead = false;
    uint8_t frameLen = 0;
    bool frameLenRead = false;
    uint8_t framePayload[OTA_MAX_PAYLOAD];
    uint8_t payloadIndex = 0;
    uint8_t crcLow = 0;
    uint8_t crcHigh = 0;
    bool crcLowRead = false;
    bool crcHighRead = false;

    const unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        while (slaveSerial.available() > 0) {
            uint8_t b = (uint8_t)slaveSerial.read();

            if (!inFrame) {
                if (b == OTA_SYNC_BYTE) {
                    inFrame = true;
                    frameType = 0;
                    frameTypeRead = false;
                    frameLen = 0;
                    frameLenRead = false;
                    payloadIndex = 0;
                    crcLowRead = false;
                    crcHighRead = false;
                }
                continue;
            }

            if (!frameTypeRead) {
                frameType = b;
                frameTypeRead = true;
                continue;
            }

            if (!frameLenRead) {
                frameLen = b;
                frameLenRead = true;
                if (frameLen > OTA_MAX_PAYLOAD) {
                    inFrame = false;
                }
                continue;
            }

            if (payloadIndex < frameLen) {
                framePayload[payloadIndex++] = b;
                continue;
            }

            if (!crcLowRead) {
                crcLow = b;
                crcLowRead = true;
                continue;
            }

            if (!crcHighRead) {
                crcHigh = b;
                crcHighRead = true;
                continue;
            }

            if (b != OTA_END_BYTE) {
                inFrame = false;
                continue;
            }

            const uint16_t receivedCrc = (uint16_t)((uint16_t)crcHigh << 8) | crcLow;
            const uint16_t expectedCrc = computeOtaFrameCrc(frameType, frameLen, framePayload);
            if (receivedCrc != expectedCrc) {
                inFrame = false;
                continue;
            }

            typeOut = frameType;
            payloadLenOut = frameLen;
            if (frameLen > 0 && payloadOut != nullptr) {
                memcpy(payloadOut, framePayload, frameLen);
            }
            return true;
        }
        delay(1);
    }
    return false;
}

static bool waitForSlaveOtaAck(uint8_t expectedType, unsigned long timeoutMs) {
    uint8_t responseType = 0;
    uint8_t payloadLen = 0;
    uint8_t payload[OTA_MAX_PAYLOAD];
    if (!readNextOtaFrame(responseType, payload, payloadLen, timeoutMs)) {
        return false;
    }

    if (responseType == OTA_TYPE_NACK) {
        return false;
    }
    if (responseType != OTA_TYPE_ACK || payloadLen < 1) {
        return false;
    }
    return payload[0] == expectedType;
}

static bool sendSlaveOtaStart(uint32_t imageSize) {
    uint8_t payload[4];
    payload[0] = (uint8_t)(imageSize & 0xFF);
    payload[1] = (uint8_t)((imageSize >> 8) & 0xFF);
    payload[2] = (uint8_t)((imageSize >> 16) & 0xFF);
    payload[3] = (uint8_t)((imageSize >> 24) & 0xFF);

    uint8_t frame[OTA_MAX_PAYLOAD + 6];
    const size_t frameLen = buildOtaFrame(OTA_TYPE_START, payload, sizeof(payload), frame, sizeof(frame));
    if (frameLen == 0) {
        strncpy(gSlaveOtaStatus, "start-frame-error", sizeof(gSlaveOtaStatus) - 1);
        gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
        return false;
    }
    slaveSerial.write(frame, frameLen);
    const bool ok = waitForSlaveOtaAck(OTA_TYPE_START, OTA_ACK_TIMEOUT_MS);
    if (ok) {
        strncpy(gSlaveOtaStatus, "streaming", sizeof(gSlaveOtaStatus) - 1);
    } else {
        strncpy(gSlaveOtaStatus, "start-ack-timeout", sizeof(gSlaveOtaStatus) - 1);
    }
    gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
    return ok;
}

static bool sendSlaveOtaDataChunk(const uint8_t* chunk, uint16_t chunkLen) {
    if (chunk == nullptr || chunkLen == 0 || chunkLen > (OTA_MAX_PAYLOAD - 2)) {
        strncpy(gSlaveOtaStatus, "invalid-data-chunk", sizeof(gSlaveOtaStatus) - 1);
        gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
        return false;
    }

    uint8_t payload[OTA_MAX_PAYLOAD];
    payload[0] = (uint8_t)(gSlaveOtaSeq & 0xFF);
    payload[1] = (uint8_t)((gSlaveOtaSeq >> 8) & 0xFF);
    memcpy(payload + 2, chunk, chunkLen);

    uint8_t frame[OTA_MAX_PAYLOAD + 6];
    const uint8_t payloadLen = (uint8_t)(chunkLen + 2);
    const size_t frameLen = buildOtaFrame(OTA_TYPE_DATA, payload, payloadLen, frame, sizeof(frame));
    if (frameLen == 0) {
        strncpy(gSlaveOtaStatus, "data-frame-error", sizeof(gSlaveOtaStatus) - 1);
        gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
        return false;
    }

    slaveSerial.write(frame, frameLen);
    if (!waitForSlaveOtaAck(OTA_TYPE_DATA, OTA_ACK_TIMEOUT_MS)) {
        strncpy(gSlaveOtaStatus, "data-ack-timeout", sizeof(gSlaveOtaStatus) - 1);
        gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
        return false;
    }

    gSlaveOtaSeq++;
    gSlaveOtaSentBytes += chunkLen;
    return true;
}

static bool sendSlaveOtaEnd() {
    uint8_t frame[OTA_MAX_PAYLOAD + 6];
    const size_t frameLen = buildOtaFrame(OTA_TYPE_END, nullptr, 0, frame, sizeof(frame));
    if (frameLen == 0) {
        strncpy(gSlaveOtaStatus, "end-frame-error", sizeof(gSlaveOtaStatus) - 1);
        gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
        return false;
    }
    slaveSerial.write(frame, frameLen);
    const bool ok = waitForSlaveOtaAck(OTA_TYPE_END, OTA_ACK_TIMEOUT_MS * 2);
    if (ok) {
        strncpy(gSlaveOtaStatus, "complete", sizeof(gSlaveOtaStatus) - 1);
    } else {
        strncpy(gSlaveOtaStatus, "end-ack-timeout", sizeof(gSlaveOtaStatus) - 1);
    }
    gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
    return ok;
}

static bool runSlaveOtaSelfTest(String& detail) {
    #if DEVICE_MODE != DEVICE_MODE_MASTER
    detail = "Self-test is only available in master mode";
    return false;
    #else
    if (gSlaveOtaInProgress) {
        detail = "Slave OTA in progress";
        return false;
    }

    gSlaveOtaInProgress = true;
    pinMode(SLAVE_EN_CONTROL_PIN, OUTPUT);
    pinMode(SLAVE_BOOT_CONTROL_PIN, OUTPUT);
    digitalWrite(SLAVE_EN_CONTROL_PIN, HIGH);
    digitalWrite(SLAVE_BOOT_CONTROL_PIN, HIGH);
    delay(5);
    strncpy(gSlaveOtaStatus, "self-test", sizeof(gSlaveOtaStatus) - 1);
    gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';

    auto waitForAliveByte = [&](unsigned long timeoutMs) {
        unsigned long start = millis();
        while (millis() - start < timeoutMs) {
            while (slaveSerial.available() > 0) {
                int b = slaveSerial.read();
                if (b == UART_ACK_BYTE || b == UART_HEARTBEAT_BYTE) {
                    return true;
                }
            }
            delay(1);
        }
        return false;
    };

    auto pulseSlaveReset = [&]() {
        digitalWrite(SLAVE_EN_CONTROL_PIN, LOW);
        delay(60);
        digitalWrite(SLAVE_EN_CONTROL_PIN, HIGH);
    };

    while (slaveSerial.available() > 0) {
        (void)slaveSerial.read();
    }

    digitalWrite(SLAVE_BOOT_CONTROL_PIN, HIGH);
    delay(5);
    pulseSlaveReset();
    const bool aliveAfterNormalReset = waitForAliveByte(1500);

    digitalWrite(SLAVE_BOOT_CONTROL_PIN, LOW);
    delay(5);
    pulseSlaveReset();
    delay(120);

    digitalWrite(SLAVE_BOOT_CONTROL_PIN, HIGH);
    delay(5);
    pulseSlaveReset();
    const bool aliveAfterBootCycle = waitForAliveByte(1500);

    const bool ok = aliveAfterNormalReset && aliveAfterBootCycle;
    if (ok) {
        detail = "EN/BOOT toggle OK and slave UART responsive";
    } else if (!aliveAfterNormalReset) {
        detail = "No UART heartbeat after normal reset";
    } else {
        detail = "No UART heartbeat after BOOT low/high cycle";
    }

    gSlaveOtaInProgress = false;
    strncpy(gSlaveOtaStatus, "idle", sizeof(gSlaveOtaStatus) - 1);
    gSlaveOtaStatus[sizeof(gSlaveOtaStatus) - 1] = '\0';
    return ok;
    #endif
}

static bool processIncomingOtaFrame(uint8_t type, const uint8_t* payload, uint8_t payloadLen) {
    auto sendAck = [&](uint8_t ackForType) {
        uint8_t ackPayload[1] = {ackForType};
        uint8_t frame[OTA_MAX_PAYLOAD + 6];
        const size_t len = buildOtaFrame(OTA_TYPE_ACK, ackPayload, sizeof(ackPayload), frame, sizeof(frame));
        if (len > 0) {
            slaveSerial.write(frame, len);
        }
    };

    auto sendNack = [&](uint8_t nackForType) {
        uint8_t nackPayload[1] = {nackForType};
        uint8_t frame[OTA_MAX_PAYLOAD + 6];
        const size_t len = buildOtaFrame(OTA_TYPE_NACK, nackPayload, sizeof(nackPayload), frame, sizeof(frame));
        if (len > 0) {
            slaveSerial.write(frame, len);
        }
    };

    if (type == OTA_TYPE_START) {
        if (payloadLen != 4) {
            sendNack(type);
            return true;
        }

        const uint32_t totalSize =
            (uint32_t)payload[0] |
            ((uint32_t)payload[1] << 8) |
            ((uint32_t)payload[2] << 16) |
            ((uint32_t)payload[3] << 24);

        gSlaveOtaReceiving = false;
        gSlaveOtaExpectedSeq = 0;
        gSlaveOtaExpectedBytes = totalSize;
        gSlaveOtaReceivedBytes = 0;

        if (Update.begin(totalSize)) {
            gSlaveOtaReceiving = true;
            sendAck(type);
        } else {
            sendNack(type);
        }
        return true;
    }

    if (type == OTA_TYPE_DATA) {
        if (!gSlaveOtaReceiving || payloadLen < 2) {
            sendNack(type);
            return true;
        }

        const uint16_t seq = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
        if (seq != gSlaveOtaExpectedSeq) {
            sendNack(type);
            return true;
        }

        const uint8_t* data = payload + 2;
        const size_t dataLen = payloadLen - 2;
        if (Update.write((uint8_t*)data, dataLen) != dataLen) {
            sendNack(type);
            return true;
        }

        gSlaveOtaExpectedSeq++;
        gSlaveOtaReceivedBytes += (uint32_t)dataLen;
        sendAck(type);
        return true;
    }

    if (type == OTA_TYPE_END) {
        if (!gSlaveOtaReceiving) {
            sendNack(type);
            return true;
        }

        const bool sizeOk = (gSlaveOtaReceivedBytes == gSlaveOtaExpectedBytes);
        const bool endOk = sizeOk && Update.end(true);
        gSlaveOtaReceiving = false;
        if (!endOk) {
            sendNack(type);
            return true;
        }

        sendAck(type);
        delay(200);
        ESP.restart();
        return true;
    }

    return false;
}

static bool tryParseSlaveOtaByte(uint8_t b) {
    static bool inFrame = false;
    static uint8_t frameType = 0;
    static bool frameTypeRead = false;
    static uint8_t frameLen = 0;
    static bool frameLenRead = false;
    static uint8_t payload[OTA_MAX_PAYLOAD];
    static uint8_t payloadIndex = 0;
    static uint8_t crcLow = 0;
    static bool crcLowRead = false;
    static uint8_t crcHigh = 0;
    static bool crcHighRead = false;

    if (!inFrame) {
        if (b != OTA_SYNC_BYTE) {
            return false;
        }
        inFrame = true;
        frameType = 0;
        frameTypeRead = false;
        frameLen = 0;
        frameLenRead = false;
        payloadIndex = 0;
        crcLowRead = false;
        crcHighRead = false;
        return true;
    }

    if (!frameTypeRead) {
        frameType = b;
        frameTypeRead = true;
        return true;
    }

    if (!frameLenRead) {
        frameLen = b;
        frameLenRead = true;
        if (frameLen > OTA_MAX_PAYLOAD) {
            inFrame = false;
        }
        return true;
    }

    if (payloadIndex < frameLen) {
        payload[payloadIndex++] = b;
        return true;
    }

    if (!crcLowRead) {
        crcLow = b;
        crcLowRead = true;
        return true;
    }

    if (!crcHighRead) {
        crcHigh = b;
        crcHighRead = true;
        return true;
    }

    bool handled = true;
    if (b == OTA_END_BYTE) {
        const uint16_t rxCrc = (uint16_t)((uint16_t)crcHigh << 8) | crcLow;
        const uint16_t exCrc = computeOtaFrameCrc(frameType, frameLen, payload);
        if (rxCrc == exCrc) {
            handled = processIncomingOtaFrame(frameType, payload, frameLen);
        }
    }

    inFrame = false;
    return handled;
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

        // Only valid communication bytes (ACK/heartbeat) mean the link is alive
        if (b == UART_ACK_BYTE || b == UART_HEARTBEAT_BYTE) {
            slaveConnected = true;
            lastSuccessfulSlaveComm = now;
            if (b == UART_ACK_BYTE) slaveAckCount++;
            if (b == UART_HEARTBEAT_BYTE) slaveHeartbeatCount++;
        }
    }

    if (now - lastSuccessfulSlaveComm > SLAVE_TIMEOUT) {
        slaveConnected = false;
    }
    
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
    memcpy(message, payload, length);
    message[length] = '\0';

    strncpy(gLastMqttTopic, topic ? topic : "", sizeof(gLastMqttTopic) - 1);
    gLastMqttTopic[sizeof(gLastMqttTopic) - 1] = '\0';
    strncpy(gLastMqttPayload, message, sizeof(gLastMqttPayload) - 1);
    gLastMqttPayload[sizeof(gLastMqttPayload) - 1] = '\0';
    gLastMqttMessageMs = millis();

    float value = 0.0f;
    bool hasNumericValue = parseMqttNumericPayload(message, value);
    
    if (DEBUG_ENABLED) {
        Serial.print("📡 MQTT [");
        Serial.print(topic);
        Serial.print("] raw='");
        Serial.print(message);
        Serial.print("' parsed=");
        if (hasNumericValue) {
            Serial.println(value);
        } else {
            Serial.println("<none>");
        }
    }

    // Accept both slash and dotted topic separators (some broker bridges/UIs display dotted paths).
    bool isPhaseA = (strstr(topic, "PhaseA_Amp") != nullptr);
    bool isPhaseB = (strstr(topic, "PhaseB_Amp") != nullptr);
    bool isPhaseC = (strstr(topic, "PhaseC_Amp") != nullptr);
    bool isSumWatt = (strstr(topic, "SumPower_kW") != nullptr);

    if (!hasNumericValue && (isPhaseA || isPhaseB || isPhaseC || isSumWatt)) {
        if (DEBUG_ENABLED) {
            Serial.println("⚠ MQTT payload has no numeric value; ignoring update");
        }
        return;
    }

    // Check topic suffix to determine which phase
    if (isPhaseA) {
        gMQTT->setCurrentPhaseA(value);
        gSignalGen->setCurrentPhaseA(value);
    } else if (isPhaseB) {
        gMQTT->setCurrentPhaseB(value);
    } else if (isPhaseC) {
        gMQTT->setCurrentPhaseC(value);
    } else if (isSumWatt) {
        if (gIgnoreFirstSumWatt) {
            // Ignore first SumPower_kW after boot to avoid stale retained startup spikes.
            gIgnoreFirstSumWatt = false;
        } else {
            gLastReceivedSumWatt = value;
            gHasReceivedSumWatt = true;
            // Graph becomes active from the 2nd received MQTT SumPower_kW value.
            gGraphPReady = true;
        }
    }
}

static bool parseMqttNumericPayload(const char* payload, float& outValue) {
    if (payload == nullptr) return false;

    // Find first possible number start (supports plain values and JSON-like payloads).
    const char* p = payload;
    while (*p != '\0') {
        if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '+') {
            break;
        }
        p++;
    }
    if (*p == '\0') return false;

    char numberBuf[48];
    size_t i = 0;
    bool hasDigit = false;
    while (*p != '\0' && i < sizeof(numberBuf) - 1) {
        char c = *p;
        bool isDigit = (c >= '0' && c <= '9');
        bool isNumericChar = isDigit || c == '-' || c == '+' || c == '.' || c == ',' || c == 'e' || c == 'E';
        if (!isNumericChar) break;
        if (c == ',') c = '.';
        if (isDigit) hasDigit = true;
        numberBuf[i++] = c;
        p++;
    }
    numberBuf[i] = '\0';

    if (!hasDigit) return false;
    outValue = atof(numberBuf);
    return true;
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
    #if DEVICE_MODE == DEVICE_MODE_MASTER
    gDisplay->println("Mode: MASTER");
    #elif DEVICE_MODE == DEVICE_MODE_SLAVE
    gDisplay->println("Mode: SLAVE");
    #else
    gDisplay->println("Mode: STANDALONE");
    #endif
    gDisplay->display();
}

static void drawSumGraph(OledDisplay* disp) {
    const int DIGIT_W = 6;
    const int GX0 = 58 + DIGIT_W;
    const int GX1 = 127;
    const int GY0 = 0;
    const int GY1 = 63;
    const int axisX = GX0;
    const int plotX0 = GX0 + 2;
    const int plotWidth = GX1 - plotX0 + 1;

    float minV = 0.0f;
    float maxV = 0.0f;
    bool hasData = false;
    for (int i = 0; i < GRAPH_SAMPLES; i++) {
        float v = graphHistory[i];
        if (v == CURRENT_DEFAULT) continue;
        if (!hasData) {
            minV = v;
            maxV = v;
            hasData = true;
        } else {
            if (v < minV) minV = v;
            if (v > maxV) maxV = v;
        }
    }

    // Dynamic axis with min step 1 kW and max 3 labels.
    const int maxLabels = 3;
    float range = maxV - minV;
    if (!hasData || range < 1.0f) range = 1.0f;
    int yStep = (int)ceilf(range / (float)(maxLabels - 1));
    if (yStep < 1) yStep = 1;

    int yMin = (int)floorf(minV / (float)yStep) * yStep;
    int yMax = (int)ceilf(maxV / (float)yStep) * yStep;
    if (yMax <= yMin) yMax = yMin + yStep;

    auto mapY = [&](float v) -> int {
        float t = (v - (float)yMin) / (float)(yMax - yMin);
        int y = GY1 - (int)lroundf(t * (float)(GY1 - GY0));
        return constrain(y, GY0, GY1);
    };

    const int axisY = (yMin <= 0 && 0 <= yMax) ? mapY(0.0f) : mapY((float)yMin);

    int labels[3];
    int labelCount = 0;
    labels[labelCount++] = yMin;
    if (yMin < 0 && yMax > 0) {
        labels[labelCount++] = 0;
    } else {
        int mid = (int)lroundf(((yMin + yMax) * 0.5f) / (float)yStep) * yStep;
        if (mid <= yMin) mid = yMin + yStep;
        if (mid >= yMax) mid = yMax - yStep;
        if (mid > yMin && mid < yMax) labels[labelCount++] = mid;
    }
    labels[labelCount++] = yMax;
    if (labelCount > 3) labelCount = 3;

    auto drawDottedHLine = [&](int y, bool emphasize) {
        if (y < GY0 || y > GY1) return;
        for (int x = plotX0; x <= GX1; x += 4) {
            disp->drawPixel(x, y, OLED_WHITE);
            if (emphasize && x + 1 <= GX1) disp->drawPixel(x + 1, y, OLED_WHITE);
        }
    };

    auto drawYLabel = [&](int y, int value) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", value);
        int len = strlen(buf);
        int tx = axisX - 2 - (len * 6);
        if (tx < 0) tx = 0;
        int ty = y - 3;
        if (ty < 0) ty = 0;
        if (ty > GY1 - 7) ty = GY1 - 7;
        disp->setCursor(tx, ty);
        disp->print(buf);
    };

    for (int i = 0; i < labelCount; i++) {
        const int lv = labels[i];
        const int y = mapY((float)lv);
        drawDottedHLine(y, lv == 0);
        drawYLabel(y, lv);
    }

    disp->drawFastVLine(axisX, GY0, GY1 - GY0 + 1, OLED_WHITE);
    disp->drawFastHLine(axisX, axisY, GX1 - axisX + 1, OLED_WHITE);

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
        int y = mapY(v);

        if (prevY >= 0) {
            int lo = min(y, prevY);
            int hi = max(y, prevY);
            for (int py = lo; py <= hi; py++) disp->drawPixel(x, py, OLED_WHITE);
        }
        disp->drawPixel(x, y, OLED_WHITE);
        prevY = y;
    }
}

static void drawBootInfoDisplay() {
    if (!OLED_ENABLED || !gDisplayReady) return;

    const unsigned long uptimeS = millis() / 1000UL;
    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    const bool hasWifiError = gWifiConnectError[0] != '\0';
    char projectVersion[20];
    getProjectVersion(projectVersion, sizeof(projectVersion));

    bool mqttOn = false;
    #if DEVICE_MODE == DEVICE_MODE_MASTER || DEVICE_MODE == DEVICE_MODE_STANDALONE
    mqttOn = (gMQTT != nullptr) && gMQTT->isConnected();
    #endif

    gDisplay->clearDisplay();
    gDisplay->setTextColor(OLED_WHITE);
    gDisplay->setTextSize(1);

    if (hasWifiError) {
        // Show WiFi error prominently
        gDisplay->setCursor(0, 0);
        gDisplay->println("! WiFi Error !");
        gDisplay->setCursor(0, 10);
        gDisplay->print("SSID: ");
        char ssidBuf[17];
        strncpy(ssidBuf, gRuntimeConfig.wifiSsid, 16);
        ssidBuf[16] = '\0';
        gDisplay->println(ssidBuf);
        gDisplay->setCursor(0, 20);
        gDisplay->println(gWifiConnectError);
        gDisplay->setCursor(0, 32);
        gDisplay->print("AP: ");
        gDisplay->println(WIFI_AP_SSID);
        gDisplay->setCursor(0, 42);
        gDisplay->print("IP: ");
        gDisplay->println(WiFi.softAPIP());
        gDisplay->setCursor(0, 52);
        char buf[20];
        snprintf(buf, sizeof(buf), "v%s", projectVersion);
        gDisplay->println(buf);
    } else {
        // Normal boot info
        const char* wifiState = gAccessPointMode ? "AP" : (wifiConnected ? "STA OK" : "STA WAIT");
        char ssid[17];
        const char* ssidSrc = gAccessPointMode ? WIFI_AP_SSID : gRuntimeConfig.wifiSsid;
        if (ssidSrc == nullptr || strlen(ssidSrc) == 0) {
            strncpy(ssid, "-", sizeof(ssid) - 1);
        } else {
            strncpy(ssid, ssidSrc, sizeof(ssid) - 1);
        }
        ssid[sizeof(ssid) - 1] = '\0';

        gDisplay->setCursor(0, 0);
        char header[24];
        snprintf(header, sizeof(header), "Boot v%s", projectVersion);
        gDisplay->println(header);
        gDisplay->setCursor(0, 8);
        gDisplay->print("Uptime: ");
        gDisplay->print(uptimeS);
        gDisplay->println("s");
        gDisplay->setCursor(0, 16);
        gDisplay->print("WiFi: ");
        gDisplay->println(wifiState);
        gDisplay->setCursor(0, 24);
        gDisplay->print("SSID: ");
        gDisplay->println(ssid);
        gDisplay->setCursor(0, 32);
        gDisplay->print("IP: ");
        if (gAccessPointMode) {
            gDisplay->println(WiFi.softAPIP());
        } else {
            gDisplay->println(WiFi.localIP());
        }
        gDisplay->setCursor(0, 40);
        gDisplay->print("MQTT: ");
        gDisplay->println(mqttOn ? "OK" : "WAIT");
        gDisplay->setCursor(0, 48);
        gDisplay->print("Slave: ");
        gDisplay->println(slaveConnected ? "OK" : "WAIT");
        gDisplay->setCursor(0, 56);
        gDisplay->println("live view after 30s");
    }
    gDisplay->display();
}

static void drawOtaRunningDisplay() {
    if (!OLED_ENABLED || !gDisplayReady) return;

    float masterPct = 0.0f;
    if (gMasterOtaTotalBytes > 0) {
        masterPct = ((float)gMasterOtaReceivedBytes * 100.0f) / (float)gMasterOtaTotalBytes;
        if (masterPct > 100.0f) masterPct = 100.0f;
    }

    float slavePct = 0.0f;
    if (gSlaveOtaTotalBytes > 0) {
        slavePct = ((float)gSlaveOtaSentBytes * 100.0f) / (float)gSlaveOtaTotalBytes;
        if (slavePct > 100.0f) slavePct = 100.0f;
    }

    gDisplay->clearDisplay();
    gDisplay->setTextColor(OLED_WHITE);
    gDisplay->setTextSize(1);

    gDisplay->setCursor(0, 0);
    gDisplay->println("OTA RUNNING");

    char mLine[32];
    char sLine[32];
    snprintf(mLine, sizeof(mLine), "M:%5.1f%% %s", masterPct, gMasterOtaStatus);
    snprintf(sLine, sizeof(sLine), "S:%5.1f%% %s", slavePct, gSlaveOtaStatus);

    gDisplay->setCursor(0, 10);
    gDisplay->println(mLine);
    gDisplay->setCursor(0, 20);
    gDisplay->println(sLine);

    char mBytes[32];
    char sBytes[32];
    snprintf(mBytes, sizeof(mBytes), "m:%lu/%lu", (unsigned long)gMasterOtaReceivedBytes, (unsigned long)gMasterOtaTotalBytes);
    snprintf(sBytes, sizeof(sBytes), "s:%lu/%lu", (unsigned long)gSlaveOtaSentBytes, (unsigned long)gSlaveOtaTotalBytes);

    gDisplay->setCursor(0, 32);
    gDisplay->println(mBytes);
    gDisplay->setCursor(0, 42);
    gDisplay->println(sBytes);

    gDisplay->setCursor(0, 54);
    gDisplay->println("please wait...");
    gDisplay->display();
}

static void drawApModeDisplay() {
    if (!OLED_ENABLED || !gDisplayReady) return;

    gDisplay->clearDisplay();
    gDisplay->setTextColor(OLED_WHITE);
    gDisplay->setTextSize(1);

    gDisplay->setCursor(0, 0);
    gDisplay->println("AP mode");

    gDisplay->setCursor(0, 10);
    gDisplay->print("SSID: ");
    gDisplay->println(WIFI_AP_SSID);

    gDisplay->setCursor(0, 20);
    gDisplay->print("PWD: ");
    gDisplay->println(WIFI_AP_PASSWORD);

    if (hasStationWiFiConfig()) {
        unsigned long now = millis();
        const unsigned long retryIntervalMs = getApRetryIntervalMs();
        unsigned long leftSec = 0;
        if (!gApAsyncConnectInProgress) {
            unsigned long elapsed = now - gApAsyncLastActionMs;
            unsigned long leftMs = (elapsed >= retryIntervalMs) ? 0 : (retryIntervalMs - elapsed);
            leftSec = leftMs / 1000UL;
        }

        char ssidBuf[11];
        strncpy(ssidBuf, gRuntimeConfig.wifiSsid, sizeof(ssidBuf) - 1);
        ssidBuf[sizeof(ssidBuf) - 1] = '\0';

        char line1[32];
        snprintf(line1, sizeof(line1), "Try SSID:\"%s\"", ssidBuf);
        gDisplay->setCursor(0, 34);
        gDisplay->println(line1);

        char line2[24];
        if (gApAsyncConnectInProgress) {
            snprintf(line2, sizeof(line2), "Attempt: yes");
        } else {
            snprintf(line2, sizeof(line2), "Attempt: %u", gApAsyncAttemptCount);
        }
        gDisplay->setCursor(0, 44);
        gDisplay->println(line2);

        gDisplay->setCursor(0, 54);
        gDisplay->print("next in ");
        gDisplay->print((unsigned long)leftSec);
        gDisplay->print(" sec");
    } else {
        gDisplay->setCursor(0, 42);
        gDisplay->println("No STA SSID cfg");
    }

    gDisplay->display();
}

void updateDisplay() {
    if (!OLED_ENABLED || !gDisplayReady) return;

    if (gMasterOtaInProgress || gSlaveOtaInProgress) {
        drawOtaRunningDisplay();
        return;
    }

    if (gAccessPointMode) {
        drawApModeDisplay();
        return;
    }

    if (millis() - gBootStartMs < BOOT_INFO_DURATION_MS) {
        drawBootInfoDisplay();
        return;
    }

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

    // Keep the OLED summary compact: live phase values, aggregate power, and
    // comms state fit on one screen while the graph uses the remaining space.
    char va[7], vb[7], vc[7], vs[7], vp[8], vmq[2], vsl[2];
    if (phaseA == CURRENT_DEFAULT) strcpy(va, "  ---"); else snprintf(va, sizeof(va), "%5.1f", phaseA);
    if (phaseB == CURRENT_DEFAULT) strcpy(vb, "  ---"); else snprintf(vb, sizeof(vb), "%5.1f", phaseB);
    if (phaseC == CURRENT_DEFAULT) strcpy(vc, "  ---"); else snprintf(vc, sizeof(vc), "%5.1f", phaseC);
    if (!hasAny)                   strcpy(vs, "  ---"); else snprintf(vs, sizeof(vs), "%5.1f", sum);
    if (!gHasReceivedSumWatt) strcpy(vp, "  ---"); else snprintf(vp, sizeof(vp), "%5.1f", gLastReceivedSumWatt);
    strcpy(vmq, mqttOn ? "+" : "-");
    strcpy(vsl, slaveOn ? "+" : "-");

    gDisplay->clearDisplay();
    gDisplay->setTextColor(OLED_WHITE);
    gDisplay->setTextSize(1);

    const int yA = 0;
    const int yB = 8;
    const int yC = 16;
    const int yS = 24;
    const int yP = 32;
    const int yMQTT = 40;
    const int ySLAVE = 48;

    gDisplay->setCursor(0, yA);
    gDisplay->print("A:");
    gDisplay->setCursor(12, yA);
    gDisplay->print(va);
    gDisplay->setCursor(42, yA);
    gDisplay->print("A");
    gDisplay->setCursor(0, yB);
    gDisplay->print("B:");
    gDisplay->setCursor(12, yB);
    gDisplay->print(vb);
    gDisplay->setCursor(42, yB);
    gDisplay->print("A");
    gDisplay->setCursor(0, yC);
    gDisplay->print("C:");
    gDisplay->setCursor(12, yC);
    gDisplay->print(vc);
    gDisplay->setCursor(42, yC);
    gDisplay->print("A");
    gDisplay->setCursor(0, yS);
    gDisplay->print("S:");
    gDisplay->setCursor(12, yS);
    gDisplay->print(vs);
    gDisplay->setCursor(42, yS);
    gDisplay->print("A");

    gDisplay->setCursor(0, yP);
    gDisplay->print("P:");
    gDisplay->setCursor(12, yP);
    gDisplay->print(vp);
    gDisplay->setCursor(42, yP);
    gDisplay->print("kW");

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
