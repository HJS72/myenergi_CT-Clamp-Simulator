#include "mqtt_handler.h"

MQTTHandler::MQTTHandler(Client& espClient) : client(espClient) {}

void MQTTHandler::begin(const char* clientId) {
    client.setBufferSize(1024);
}

bool MQTTHandler::connect(const char* server, uint16_t port, const char* user, const char* pass) {
    client.setServer(server, port);
    if (user && pass) {
        return client.connect("myenergi_CT-Clamp-Simulator", user, pass);
    } else {
        return client.connect("myenergi_CT-Clamp-Simulator");
    }
}

bool MQTTHandler::isConnected() {
    return client.connected();
}

void MQTTHandler::reconnect() {
    unsigned long now = millis();
    
    if (!isConnected() && now - lastReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
        lastReconnectAttempt = now;
        
        if (connect(MQTT_SERVER, MQTT_PORT)) {
            subscribe();
            publishStatus("online");
        }
    }
}

void MQTTHandler::subscribe() {
    client.subscribe(MQTT_TOPIC_PHASE_A);
    client.subscribe(MQTT_TOPIC_PHASE_B);
    client.subscribe(MQTT_TOPIC_PHASE_C);
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
    client.setCallback(callback);
}

void MQTTHandler::publishStatus(const char* status) {
    StaticJsonDocument<128> doc;
    doc["status"] = status;
    doc["timestamp"] = millis();
    
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish("myenergi/harvi/status", buffer);
}
