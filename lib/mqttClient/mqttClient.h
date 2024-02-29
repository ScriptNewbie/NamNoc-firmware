#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

#include "eePromTools.h"
#include "temperatureSensor.h"

#define MQTTSADDR 97     // max broker address 115 chars + one for termination \0
#define MQTTPORTADDR 213 // max port length (chars) 5 + one for termination \0
#define MQTTTADDR 219    // publish topic
#define MQTTUADDR 320    // username
#define MQTTPADDR 409    // password
#define TEMPADDR 490
#define HISTADDR 500

class MqttClient
{
private:
    PubSubClient mqtt;
    String server = "";    // mqtt broker address
    String port = ""; // mqtt port
    String topic = "";    
    String user = "";    // mqtt username
    String password = "";    // mqtt password
    void callback(char *topic, byte *payload, unsigned int length);
    void handleHeartbeatMessage(String payload);

public:
    MqttClient();
    void init();
    void publish();
    String getServer();
    String getPort();
    String getTopic();
    String getUser();
    void connect();
    bool subscribe();
    int state();
    void loop();
};