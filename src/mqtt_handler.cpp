#include "mqtt_handler.h"

#include <cmath>
#include <cstring>

static const unsigned long DATAPOINT_RECHECK_INTERVAL_MS = 30000UL;

static void extractShortMqttName(const char* path, char* out, size_t outSize) {
    if (!out || outSize == 0) return;

    if (!path) {
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

    size_t n = (size_t)(end - start);
    if (n >= outSize) n = outSize - 1;
    memcpy(out, start, n);
    out[n] = '\0';
}

MQTTHandler::MQTTHandler(Client& espClient) : client(espClient) {}

void MQTTHandler::begin(const char* clientId) {
    client.setBufferSize(1024);
    if (clientId && clientId[0] != '\0') {
        strncpy(mqttClientId, clientId, sizeof(mqttClientId) - 1);
        mqttClientId[sizeof(mqttClientId) - 1] = '\0';
    }
}

bool MQTTHandler::connect(const char* server, uint16_t port, const char* user, const char* pass) {
    setConnectionConfig(server, port, user, pass);
    client.setServer(mqttServer, mqttPort);
    if (user && pass) {
        return client.connect(mqttClientId, user, pass);
    } else {
        return client.connect(mqttClientId);
    }
}

void MQTTHandler::setConnectionConfig(const char* server, uint16_t port, const char* user, const char* pass) {
    if (server && server[0] != '\0') {
        strncpy(mqttServer, server, sizeof(mqttServer) - 1);
        mqttServer[sizeof(mqttServer) - 1] = '\0';
    }
    if (port > 0) {
        mqttPort = port;
    }

    if (user) {
        strncpy(mqttUser, user, sizeof(mqttUser) - 1);
        mqttUser[sizeof(mqttUser) - 1] = '\0';
    }
    if (pass) {
        strncpy(mqttPass, pass, sizeof(mqttPass) - 1);
        mqttPass[sizeof(mqttPass) - 1] = '\0';
    }
}

bool MQTTHandler::isConnected() {
    return client.connected();
}

int MQTTHandler::getConnectionState() {
    return client.state();
}

const char* MQTTHandler::getConnectionStateText() {
    switch (client.state()) {
        case MQTT_CONNECTION_TIMEOUT:
            return "timeout";
        case MQTT_CONNECTION_LOST:
            return "connection-lost";
        case MQTT_CONNECT_FAILED:
            return "connect-failed";
        case MQTT_DISCONNECTED:
            return "disconnected";
        case MQTT_CONNECTED:
            return "connected";
        case MQTT_CONNECT_BAD_PROTOCOL:
            return "bad-protocol";
        case MQTT_CONNECT_BAD_CLIENT_ID:
            return "bad-client-id";
        case MQTT_CONNECT_UNAVAILABLE:
            return "server-unavailable";
        case MQTT_CONNECT_BAD_CREDENTIALS:
            return "bad-credentials";
        case MQTT_CONNECT_UNAUTHORIZED:
            return "unauthorized";
        default:
            return "unknown";
    }
}

bool MQTTHandler::isServerReachable() {
    const int state = client.state();
    return client.connected() ||
           state == MQTT_CONNECT_BAD_CREDENTIALS ||
           state == MQTT_CONNECT_UNAUTHORIZED ||
           state == MQTT_CONNECT_BAD_CLIENT_ID ||
           state == MQTT_CONNECT_BAD_PROTOCOL;
}

bool MQTTHandler::isLoginOk() {
    if (mqttUser[0] == '\0') {
        return client.connected();
    }

    return client.connected();
}

bool MQTTHandler::isLoginConfigured() {
    return mqttUser[0] != '\0';
}

void MQTTHandler::reconnect() {
    unsigned long now = millis();
    
    if (!isConnected() && now - lastReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
        lastReconnectAttempt = now;
        
        client.setServer(mqttServer, mqttPort);
        bool ok = false;
        if (mqttUser[0] != '\0') {
            ok = client.connect(mqttClientId, mqttUser, mqttPass);
        } else {
            ok = client.connect(mqttClientId);
        }

        if (ok) {
            subscribe();
            publishStatus("online");
            initializeDatapoints(mqttPath);
        }
    }
}

void MQTTHandler::setMqttPath(const char* path) {
    extractShortMqttName(path, mqttPath, sizeof(mqttPath));
}

void MQTTHandler::subscribe() {
    char topic[128];
    
    if (DEBUG_ENABLED) {
        Serial.println("MQTT Subscribing to topics:");
    }
    
    // Subscribe to both canonical and compatibility topic variants so the
    // firmware works with brokers/UIs that normalize away the leading slash.
    // Canonical topics with leading slash
    snprintf(topic, sizeof(topic), "/%s/PhaseA_Amp", mqttPath);
    if (DEBUG_ENABLED) Serial.print("  [Canon] ");
    if (DEBUG_ENABLED) Serial.println(topic);
    client.subscribe(topic);
    
    snprintf(topic, sizeof(topic), "/%s/PhaseB_Amp", mqttPath);
    if (DEBUG_ENABLED) Serial.print("  [Canon] ");
    if (DEBUG_ENABLED) Serial.println(topic);
    client.subscribe(topic);
    
    snprintf(topic, sizeof(topic), "/%s/PhaseC_Amp", mqttPath);
    if (DEBUG_ENABLED) Serial.print("  [Canon] ");
    if (DEBUG_ENABLED) Serial.println(topic);
    client.subscribe(topic);

    snprintf(topic, sizeof(topic), "/%s/SumPower_kW", mqttPath);
    if (DEBUG_ENABLED) Serial.print("  [Canon] ");
    if (DEBUG_ENABLED) Serial.println(topic);
    client.subscribe(topic);

    // Compatibility topics without leading slash
    snprintf(topic, sizeof(topic), "%s/PhaseA_Amp", mqttPath);
    if (DEBUG_ENABLED) Serial.print("  [Compat] ");
    if (DEBUG_ENABLED) Serial.println(topic);
    client.subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/PhaseB_Amp", mqttPath);
    if (DEBUG_ENABLED) Serial.print("  [Compat] ");
    if (DEBUG_ENABLED) Serial.println(topic);
    client.subscribe(topic);
    
    snprintf(topic, sizeof(topic), "%s/PhaseC_Amp", mqttPath);
    if (DEBUG_ENABLED) Serial.print("  [Compat] ");
    if (DEBUG_ENABLED) Serial.println(topic);
    client.subscribe(topic);

    snprintf(topic, sizeof(topic), "%s/SumPower_kW", mqttPath);
    if (DEBUG_ENABLED) Serial.print("  [Compat] ");
    if (DEBUG_ENABLED) Serial.println(topic);
    client.subscribe(topic);
}

void MQTTHandler::loop() {
    client.loop();
}

float MQTTHandler::getCurrentPhaseA() {
    return currentA;
}

float MQTTHandler::getCurrentPhaseB() {
    return currentB;
}

float MQTTHandler::getCurrentPhaseC() {
    return currentC;
}

void MQTTHandler::setCurrentPhaseA(float value) {
    currentA = constrain(value, -(float)CURRENT_MAX, (float)CURRENT_MAX);
}

void MQTTHandler::setCurrentPhaseB(float value) {
    currentB = constrain(value, -(float)CURRENT_MAX, (float)CURRENT_MAX);
}

void MQTTHandler::setCurrentPhaseC(float value) {
    currentC = constrain(value, -(float)CURRENT_MAX, (float)CURRENT_MAX);
}

void MQTTHandler::setCallback(void (*callback)(char*, uint8_t*, unsigned int)) {
    _userCallback = callback;
    client.setCallback(callback);
}

bool MQTTHandler::publishStatus(const char* status) {
    if (!client.connected()) {
        return false;
    }

    StaticJsonDocument<128> doc;
    doc["status"] = status ? status : "unknown";
    doc["timestamp"] = millis();
    
    char buffer[256];
    serializeJson(doc, buffer);

    char topicCanonical[128];
    char topicCompat[128];
    snprintf(topicCanonical, sizeof(topicCanonical), "/%s/Status", mqttPath);
    snprintf(topicCompat, sizeof(topicCompat), "%s/Status", mqttPath);

    bool okCanonical = client.publish(topicCanonical, (uint8_t*)buffer, strlen(buffer), true);
    bool okCompat = client.publish(topicCompat, (uint8_t*)buffer, strlen(buffer), true);
    return okCanonical || okCompat;
}

bool MQTTHandler::publishSumWatt(float value, bool retained) {
    if (!client.connected()) {
        return false;
    }

    char topic[128];
    snprintf(topic, sizeof(topic), "/%s/SumPower_kW", mqttPath);

    char payload[24];
    snprintf(payload, sizeof(payload), "%.1f", value);
    return client.publish(topic, (uint8_t*)payload, strlen(payload), retained);
}

void MQTTHandler::periodicDatapointCheck(const char* mqttPath) {
    if (!client.connected()) return;
    unsigned long now = millis();
    if (now - lastDatapointCheck >= DATAPOINT_RECHECK_INTERVAL_MS) {
        initializeDatapoints(mqttPath);
    }
}

void MQTTHandler::initializeDatapoints(const char* mqttPath) {
    if (!client.connected() || !mqttPath || mqttPath[0] == '\0') return;

    lastDatapointCheck = millis();

    char normalizedPath[64];
    extractShortMqttName(mqttPath, normalizedPath, sizeof(normalizedPath));

    char topicA[128], topicB[128], topicC[128], topicStatus[128], topicSumWatt[128];
    char topicACompat[128], topicBCompat[128], topicCCompat[128], topicStatusCompat[128], topicSumWattCompat[128];
    snprintf(topicA,             sizeof(topicA),             "/%s/PhaseA_Amp",  normalizedPath);
    snprintf(topicB,             sizeof(topicB),             "/%s/PhaseB_Amp",  normalizedPath);
    snprintf(topicC,             sizeof(topicC),             "/%s/PhaseC_Amp",  normalizedPath);
    snprintf(topicStatus,        sizeof(topicStatus),        "/%s/Status",      normalizedPath);
    snprintf(topicSumWatt,       sizeof(topicSumWatt),       "/%s/SumPower_kW", normalizedPath);
    snprintf(topicACompat,       sizeof(topicACompat),       "%s/PhaseA_Amp",   normalizedPath);
    snprintf(topicBCompat,       sizeof(topicBCompat),       "%s/PhaseB_Amp",   normalizedPath);
    snprintf(topicCCompat,       sizeof(topicCCompat),       "%s/PhaseC_Amp",   normalizedPath);
    snprintf(topicStatusCompat,  sizeof(topicStatusCompat),  "%s/Status",       normalizedPath);
    snprintf(topicSumWattCompat, sizeof(topicSumWattCompat), "%s/SumPower_kW",  normalizedPath);

    if (DEBUG_ENABLED) {
        Serial.print("MQTT initializeDatapoints path: ");
        Serial.println(normalizedPath);
    }

    // Unconditionally publish retained initial values so datapoints always exist
    // on the broker. The external system will overwrite with live values quickly.
    char payload[32];
    auto sanitizeCurrent = [](float v) {
        return (isfinite(v) && v != CURRENT_DEFAULT) ? v : 0.0f;
    };

    snprintf(payload, sizeof(payload), "%.2f", sanitizeCurrent(currentA));
    bool okA = client.publish(topicA, (uint8_t*)payload, strlen(payload), true);
    bool okACompat = client.publish(topicACompat, (uint8_t*)payload, strlen(payload), true);

    snprintf(payload, sizeof(payload), "%.2f", sanitizeCurrent(currentB));
    bool okB = client.publish(topicB, (uint8_t*)payload, strlen(payload), true);
    bool okBCompat = client.publish(topicBCompat, (uint8_t*)payload, strlen(payload), true);

    snprintf(payload, sizeof(payload), "%.2f", sanitizeCurrent(currentC));
    bool okC = client.publish(topicC, (uint8_t*)payload, strlen(payload), true);
    bool okCCompat = client.publish(topicCCompat, (uint8_t*)payload, strlen(payload), true);

    bool okStatus  = client.publish(topicStatus,  (uint8_t*)"initialized", 11, true);
    bool okStatusCompat = client.publish(topicStatusCompat, (uint8_t*)"initialized", 11, true);
    bool okSumWatt = client.publish(topicSumWatt, (uint8_t*)"0.0",          3, true);
    bool okSumWattCompat = client.publish(topicSumWattCompat, (uint8_t*)"0.0", 3, true);

    if (DEBUG_ENABLED) {
        Serial.print("  publish canon A/B/C/Status/SumW: ");
        Serial.print(okA      ? "ok" : "FAIL");
        Serial.print("/");
        Serial.print(okB      ? "ok" : "FAIL");
        Serial.print("/");
        Serial.print(okC      ? "ok" : "FAIL");
        Serial.print("/");
        Serial.print(okStatus ? "ok" : "FAIL");
        Serial.print("/");
        Serial.println(okSumWatt ? "ok" : "FAIL");

        Serial.print("  publish compat A/B/C/Status/SumW: ");
        Serial.print(okACompat      ? "ok" : "FAIL");
        Serial.print("/");
        Serial.print(okBCompat      ? "ok" : "FAIL");
        Serial.print("/");
        Serial.print(okCCompat      ? "ok" : "FAIL");
        Serial.print("/");
        Serial.print(okStatusCompat ? "ok" : "FAIL");
        Serial.print("/");
        Serial.println(okSumWattCompat ? "ok" : "FAIL");
    }
}

