#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Ticker.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <ezTime.h>

#include "eePromTools.h"
#include "valve.h"

#define ONE_WIRE_BUS 13
#define BUTTON 0

#define SSIDADDR 0       // max ssid 32 chars + one for termination \0
#define WPAADDR 33       // max wpa2 pass 63 chars + one for termination \0
#define MQTTSADDR 97     // max broker address 115 chars + one for termination \0
#define MQTTPORTADDR 213 // max port length (chars) 5 + one for termination \0
#define MQTTTADDR 219    // publish topic
#define MQTTUADDR 320    // username
#define MQTTPADDR 409    // password
#define TEMPADDR 490
#define HISTADDR 500
#define EPROMENDADDR 511

Valve valve;

String ssid = "";     // ssid
String wpa = "";      // wifipassword
String mqtts = "";    // mqtt broker address
String mqttport = ""; // mqtt port
String mqttt = "";    // mqtt NamNoc's Hub topic
String mqttu = "";    // mqtt username
String mqttp = "";    // mqtt password
String ip = "";
String payloadtosend = "";
String timeStamp = "0";

bool sub = 0;       // Successfull subscription to mqtt topic.
bool connblink = 1; // Blinking bool
bool softAP = 1;    // Soft AP status
String mac;         // Stores device's MAC ADDRESS
char macchar[18];   // Stores device's MAC ADDRESS
int alive = 0;      // Connection with hub status
double offline_temp = 21.0;
double offline_hist = 0.2;
double temps[3] = {0, 0, 0};
int tempIndex = 0;
double lastThreeAvgTemp = 0; // Avg of last three measurments
int resetCount = 0;

// Ticker for operations performed every minute.
Ticker minut;
bool minutb = 0;
void minutcb()
{
  minutb = 1;
}

// Ticker for initial jobs like initial connection to WiFi
Ticker initial;
bool initb = 0;
void initcb()
{
  initb = WiFi.status() == WL_CONNECTED;
}

// Ticker for stopping motor if detection of rotation end failes.

// Ticker for blinking ;)
Ticker blinking;
void blinkingcb()
{
  digitalWrite(LED_BUILTIN, connblink);
  connblink = !connblink;
}

// Some initialisation stuff
WiFiClient wlan;
PubSubClient mqtt(wlan);
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Clearing EEPROM
void factoryReset()
{
  blinking.detach();
  digitalWrite(LED_BUILTIN, LOW);
  while (!digitalRead(BUTTON))
  {
    delay(100);
  }
  EEPROM.begin(512);
  for (int i = 0; i < EPROMENDADDR; ++i)
  {
    EEPROM.write(i, '\0');
  }
  EEPROM.end();
  delay(5000);
  ESP.restart();
}

// Generating string to be sent via mqtt
String generatePayloadString()
{
  return "{\"id\":\"" + mac + "\", \"ip\":\"" + ip + "\", \"temp\":" + String(lastThreeAvgTemp) + ", \"timestamp\":" + timeStamp + ", \"opened\":" + valve.isOpened() + "}";
}

void handleHeartbeatMessage(String message)
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

// Callback for mqtt subscribed topic messages
void callback(char *topic, byte *payload, unsigned int length)
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

  payloadtosend = generatePayloadString();
  mqtt.publish(mqttt.c_str(), payloadtosend.c_str());
}

// Callback for web requests for mainpage - there is configuration form and href to firmware upgrade option.
void handle_home_page()
{
  String html = "<h3>Settings</h3><form action=\"/settings\" method=\"post\" enctype=\"multipart/form-data\"><label for=\"ssid\">SSID: (currently set - \"" + ssid + "\")</label><br><input type=\"text\" name=\"ssid\"><br><label for=\"wpa\">WiFi Password:</label><br><input type=\"password\" name=\"wpa\"><br><label for=\"mqttserver\">MQTT Broaker address: (currently set - \"" + mqtts + "\")</label><br><input type=\"text\" name=\"mqttserver\"><br><label for=\"mqttport\">MQTT Broaker port (1883 if default, currently set - \"" + mqttport + "\"):</label><br><input type=\"number\" name=\"mqttport\"><br><label for=\"mqtttopic\">MQTT NamNoc's Hub topic: (NamNoc if default, currently set - \"" + mqttt + "\")</label><br><input type=\"text\" name=\"mqtttopic\"><br><label for=\"mqttusername\">MQTT Username: (optional - empty is default, currently set - \"" + mqttu + "\")</label><br><input type=\"text\" name=\"mqttusername\"><br><label for=\"mqttpassword\">MQTT Password: (optional - empty is default)</label><br><input type=\"password\" name=\"mqttpassword\"><br><label for=\"temp\">Temperature when offline (is being synced with temperature set on NamNoc Hub, currently set " + offline_temp + ")</label><br><input type=\"number\" step=\"0.01\" name=\"temp\"><br><label for=\"hist\">Hysteresis when offline (is being synced with hysteresis set on NamNoc Hub, currently set " + offline_hist + ")</label><br><input type=\"number\" step=\"0.01\" name=\"hist\"><br><br><input type=\"submit\" value=\"Submit\"></form><h3>Firmware upgrade</h3><a href=\"/update\">Firmware upgrade</a>";
  server.send(200, "text/html", html.c_str());
}

// Dealing with change of configuration - callback for web request of /settings - it writes values submitted from mainpage to eeprom. TODO: Add verification of users data!!! Not necessary right now!
void handle_settings()
{
  EEPROM.begin(512);
  String ssidtemp = server.arg(0);
  String wpatemp = server.arg(1);
  String mqttstemp = server.arg(2);
  String mqttporttemp = server.arg(3);
  String mqttttemp = server.arg(4);
  String mqttutemp = server.arg(5);
  String mqttptemp = server.arg(6);
  String temptemp = server.arg(7);
  String histtemp = server.arg(8);

  if (server.args() == 9)
  {
    eepromWrite(SSIDADDR, ssidtemp);
    eepromWrite(WPAADDR, wpatemp);
    eepromWrite(MQTTSADDR, mqttstemp);
    eepromWrite(MQTTPORTADDR, mqttporttemp);
    eepromWrite(MQTTTADDR, mqttttemp);
    eepromWrite(MQTTUADDR, mqttutemp);
    eepromWrite(MQTTPADDR, mqttptemp);
    eepromWrite(TEMPADDR, temptemp);
    eepromWrite(HISTADDR, histtemp);

    server.send(200, "text/html", "It worked! Rebooting now!");
    delay(5000);
    ESP.restart(); // Reset after (hopefully) successfull write to EEPROM
  }
  server.send(200, "text/html", "Something went wrong! Please <a href=\"/\">go back</a> and try again!"); // Let the user know he did something wrong.
}

void initializeVars()
{
  ssid = eepromRead(SSIDADDR, WPAADDR);
  wpa = eepromRead(WPAADDR, MQTTSADDR);
  mqtts = eepromRead(MQTTSADDR, MQTTPORTADDR);
  mqttport = eepromRead(MQTTPORTADDR, MQTTTADDR);
  mqttt = eepromRead(MQTTTADDR, MQTTUADDR);
  mqttu = eepromRead(MQTTUADDR, MQTTPADDR);
  mqttp = eepromRead(MQTTPADDR, TEMPADDR);
  offline_temp = eepromRead(TEMPADDR, HISTADDR).toDouble();
  offline_hist = eepromRead(HISTADDR, EPROMENDADDR).toDouble();

  temps[0] = offline_temp;
  temps[1] = offline_temp;
  temps[2] = offline_temp;
}

void setup()
{
  // More initialization stuff like, but not limited to, reading parameters from EEPROM, connecting to AP, setting MQTT Broker parameters and so on.
  pinMode(BUTTON, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  blinking.attach(0.2, blinkingcb);

  delay(500);
  initializeVars();

  sensors.begin();
  WiFi.begin(ssid.c_str(), wpa.c_str());
  mac = WiFi.macAddress();
  mac.toCharArray(macchar, mac.length() + 1);
  WiFi.softAP(mac);

  mqtt.setServer(mqtts.c_str(), mqttport.toInt());
  mqtt.setCallback(callback);
  httpUpdater.setup(&server);
  MDNS.addService("http", "tcp", 80);

  server.on("/", handle_home_page);
  server.on("/settings", handle_settings);
  server.begin();

  initial.attach(0.5, initcb);
  // Open valve

  minut.attach(60, minutcb);
}

void loop()
{
  mqtt.loop();

  valve.handlEvents();

  if (initb) // When connected to wifi sucessfully
  {
    initial.detach();
    ip = WiFi.localIP().toString();
    initb = 0;
    minutb = 1;
    mqtt.connect(macchar, mqttu.c_str(), mqttp.c_str());
    sub = mqtt.subscribe(macchar);
    waitForSync();
  }

  // Tasks performed every minute - measuring temperature and publishing MQTT message, also controling connection with hub and decision making when conection lost
  if (minutb)
  {
    minutb = 0;
    sensors.requestTemperatures();
    timeStamp = String(UTC.now());
    temps[tempIndex] = sensors.getTempCByIndex(0);
    lastThreeAvgTemp = 0;
    for (int i = 0; i < 3; ++i)
    {
      lastThreeAvgTemp += temps[i];
    }
    lastThreeAvgTemp = lastThreeAvgTemp / 3;
    payloadtosend = generatePayloadString();
    mqtt.publish(mqttt.c_str(), payloadtosend.c_str());
    tempIndex = (tempIndex + 1) % 3;
    if (alive > 0)
    {
      --alive;
      if (softAP)
      {
        WiFi.softAPdisconnect(1);
        blinking.detach();
        digitalWrite(LED_BUILTIN, HIGH);
        softAP = 0;
      }
    }
    else
    {
      if (!softAP)
      {
        WiFi.softAP(mac);
        softAP = 1;
      }
      blinking.attach(0.5, blinkingcb);
      if (mqtt.state() != 0)
      {
        blinking.attach(0.2, blinkingcb);
        mqtt.connect(macchar, mqttu.c_str(), mqttp.c_str());
        sub = mqtt.subscribe(macchar);
      }
      else if (!sub)
        sub = mqtt.subscribe(macchar);

      if (valve.isOpened() && lastThreeAvgTemp > (offline_temp + offline_hist))
      {
        valve.close();
      }
      else if (!valve.isOpened() && lastThreeAvgTemp < (offline_temp - offline_hist))
      {
        valve.open();
      }
    }
  }
  events();
  server.handleClient();

  // factory reset
  while (!digitalRead(BUTTON))
  {
    ++resetCount;
    if (resetCount == 50)
      factoryReset();
    delay(100);
  }
  resetCount = 0;
}