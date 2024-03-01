
#include "mqttClient.h"
#include <Ticker.h>

#include "valve.h"

extern double lastThreeAvgTemp;
extern String mac;
extern String ip;
extern double offline_temp;
extern double offline_hist;
extern int alive;
extern Ticker blinking;
extern Valve valve;

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
    alive = 5;
    blinking.detach();
    digitalWrite(LED_BUILTIN, HIGH);

    int pos = payload_string.indexOf(";");
    String offlinetemp = payload_string.substring(10, pos);
    double temp = offlinetemp.toDouble();
    String offlinehist = payload_string.substring(pos + 1);
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
    return;
  }

  publish();
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
  mqtt.publish(topic.c_str(), ("{\"id\":\"" + mac + "\", \"ip\":\"" + ip + "\", \"temp\":" + String(lastThreeAvgTemp) + ", \"opened\":" + valve.isOpened() + "}").c_str());
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
