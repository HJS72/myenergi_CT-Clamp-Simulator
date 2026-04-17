#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

class MQTTHandler {
public:
    MQTTHandler(Client& espClient);
    
    // Connection management
    void begin(const char* clientId);
    bool connect(const char* server, uint16_t port, const char* user = nullptr, const char* pass = nullptr);
    void setConnectionConfig(const char* server, uint16_t port, const char* user = nullptr, const char* pass = nullptr);
    bool isConnected();
    void reconnect();
    void loop();
    int getConnectionState();
    const char* getConnectionStateText();
    bool isServerReachable();
    bool isLoginOk();
    bool isLoginConfigured();
    
    // Subscription management
    void setMqttPath(const char* path);
    void subscribe();
    
    // Data getters (current values in Amperes)
    float getCurrentPhaseA();
    float getCurrentPhaseB();
    float getCurrentPhaseC();
    
    // Data setters (for Master-Slave mode)
    void setCurrentPhaseA(float value);
    void setCurrentPhaseB(float value);
    void setCurrentPhaseC(float value);
    
    // Callback setup
    void setCallback(void (*callback)(char*, uint8_t*, unsigned int));
    
    // Publisher
    bool publishStatus(const char* status);
    bool publishSumWatt(float value, bool retained = true);
    void initializeDatapoints(const char* mqttPath);
    void periodicDatapointCheck(const char* mqttPath);
    
private:
    PubSubClient client;
    unsigned long lastReconnectAttempt = 0;
    unsigned long lastDatapointCheck = 0;
    char mqttServer[64] = MQTT_SERVER;
    uint16_t mqttPort = MQTT_PORT;
    char mqttUser[64] = "";
    char mqttPass[64] = "";
    char mqttClientId[64] = "myenergi_CT-Clamp-Simulator";
    
    // Current values (RMS) — CURRENT_DEFAULT means no MQTT value received yet
    float currentA = CURRENT_DEFAULT;
    float currentB = CURRENT_DEFAULT;
    float currentC = CURRENT_DEFAULT;

    // MQTT path for subscription
    char mqttPath[64] = "esp32CTSimulator";

    // Stored user callback (restored after datapoint init check)
    void (*_userCallback)(char*, uint8_t*, unsigned int) = nullptr;
    
    friend void mqttMessageCallback(char* topic, uint8_t* payload, unsigned int length);
};

// Global MQTT handler (declared in main.cpp)
extern MQTTHandler* gMQTT;

#endif // MQTT_HANDLER_H
