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
    bool isConnected();
    void reconnect();
    void loop();
    
    // Subscription management
    void subscribe();
    
    // Data getters (current values in Amperes)
    float getCurrentPhaseA();
    float getCurrentPhaseB();
    float getCurrentPhaseC();
    
    // Data setters (for Master-Slave mode)
    void setCurrentPhaseB(float value);
    void setCurrentPhaseC(float value);
    
    // Callback setup
    void setCallback(void (*callback)(char*, uint8_t*, unsigned int));
    
    // Publisher
    void publishStatus(const char* status);
    
private:
    PubSubClient client;
    unsigned long lastReconnectAttempt = 0;
    
    // Current values (RMS)
    float currentA = 0.0;
    float currentB = 0.0;
    float currentC = 0.0;
    
    friend void mqttMessageCallback(char* topic, uint8_t* payload, unsigned int length);
};

// Global MQTT handler (declared in main.cpp)
extern MQTTHandler* gMQTT;

#endif // MQTT_HANDLER_H
