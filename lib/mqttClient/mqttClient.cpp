
#include "mqttClient.h"
#include <Ticker.h>
#include <WiFiClient.h>

#include "valve.h"

extern double lastThreeAvgTemp;
extern String mac;
extern String ip;
extern double offline_temp;
extern double offline_hist;
extern int alive;
extern Ticker blinking;
extern WiFiClient wlan;
extern Valve valve;

String generatePayloadString()
{
  return "{\"id\":\"" + mac + "\", \"ip\":\"" + ip + "\", \"temp\":" + String(lastThreeAvgTemp) + ", \"opened\":" + valve.isOpened() + "}";
}

void MqttClient::handleHeartbeatMessage(String message)
{
  alive = 5;
  blinking.detach();
  digitalWrite(LED_BUILTIN, HIGH);

  message = message.substring(10);
  int pos = message.indexOf(";");
  String offlinetemp = message.substring(0, pos);
  double temp = offlinetemp.toDouble();
  String offlinehist = message.substring(pos + 1);
  double hist = offlinehist.toDouble();

  if (temp != offline_temp)
  {
    offline_temp = temp;
    eepromWrite(TEMPADDR, offlinetemp);
  }

  if (hist != offline_hist)
  {
    offline_hist = hist;
    eepromWrite(HISTADDR, offlinehist);
  }
}

void MqttClient::callback(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';
  String payload_string((char *)payload);

  if (payload_string == "open")
  {
    valve.open();
  }
  else if (payload_string == "close")
  {
    valve.close();
  }
  else if (payload_string.substring(0, 9) == "heartbeat")
  {
    handleHeartbeatMessage(payload_string);
    return;
  }

  publish();
}

MqttClient::MqttClient()
{
    mqtt = PubSubClient(wlan);
}

void MqttClient::init()
{
    server = eepromRead(MQTTSADDR, MQTTPORTADDR);
    port = eepromRead(MQTTPORTADDR, MQTTTADDR);
    topic = eepromRead(MQTTTADDR, MQTTUADDR);
    user = eepromRead(MQTTUADDR, MQTTPADDR);
    password = eepromRead(MQTTPADDR, TEMPADDR);

    mqtt.setServer(server.c_str(), port.toInt());
    mqtt.setCallback(std::bind(&MqttClient::callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}


void MqttClient::publish()
{
    mqtt.publish(topic.c_str(), generatePayloadString().c_str());
}

String MqttClient::getServer()
{
    return server;
}

String MqttClient::getPort()
{
    return port;
}

String MqttClient::getTopic()
{
    return topic;
}

String MqttClient::getUser()
{
    return user;
}

void MqttClient::connect()
{
    mqtt.connect(mac.c_str(), user.c_str(), password.c_str());
}

bool MqttClient::subscribe()
{
    return mqtt.subscribe(mac.c_str());
}

int MqttClient::state()
{
    return mqtt.state();
}

void MqttClient::loop()
{
    mqtt.loop();
}


