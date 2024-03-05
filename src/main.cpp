#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>

#include "eePromTools.h"
#include "valve.h"
#include "temperatureSensor.h"
#include "mqttClient.h"

#define BUTTON 0

#define SSIDADDR 0 // max ssid 32 chars + one for termination \0
#define WPAADDR 33 // max wpa2 pass 63 chars + one for termination \0
#define EPROMENDADDR 511

MqttClient mqtt;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
Valve valve;
TemperatureSensor tempSensor;

String ssid = "";
String wpa = "";
String mac;

bool sub = 0;       // Successfull subscription to mqtt topic.
bool connblink = 1; // Blinking bool
bool softAP = 1;    // Soft AP status
int alive = 0;      // Connection with hub status

double offline_temp = 21.0;
double offline_hist = 0.2;
double lastThreeAvgTemp = 0; // Avg of last three measurments

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

// Ticker for blinking ;)
Ticker blinking;
void blinkingcb()
{
  digitalWrite(LED_BUILTIN, connblink);
  connblink = !connblink;
}

// Clearing EEPROM
void handleFactoryReset()
{
  int resetCount = 0;
  while (!digitalRead(BUTTON)) //When button is pressed
  {
    ++resetCount;
    if (resetCount == 50)
    {
      blinking.detach();
      digitalWrite(LED_BUILTIN, LOW);
      while (!digitalRead(BUTTON)) //Wait for button release
      {
        delay(100);
      }
      clearEeprom();
      delay(5000);
      ESP.restart();
    }
    delay(100);
  }
}

// Callback for web requests for mainpage - there is configuration form and href to firmware upgrade option.
void handle_home_page()
{
  String html = "<h3>Settings</h3><form action=\"/settings\" method=\"post\" enctype=\"multipart/form-data\"><label for=\"ssid\">SSID: (currently set - \"" + ssid + "\")</label><br><input type=\"text\" name=\"ssid\"><br><label for=\"wpa\">WiFi Password:</label><br><input type=\"password\" name=\"wpa\"><br><label for=\"mqttserver\">MQTT Broaker address: (currently set - \"" + mqtt.getServer() + "\")</label><br><input type=\"text\" name=\"mqttserver\"><br><label for=\"mqttport\">MQTT Broaker port (1883 if default, currently set - \"" + mqtt.getPort() + "\"):</label><br><input type=\"number\" name=\"mqttport\"><br><label for=\"mqtttopic\">MQTT NamNoc's Hub topic: (NamNoc if default, currently set - \"" + mqtt.getTopic() + "\")</label><br><input type=\"text\" name=\"mqtttopic\"><br><label for=\"mqttusername\">MQTT Username: (optional - empty is default, currently set - \"" + mqtt.getUser() + "\")</label><br><input type=\"text\" name=\"mqttusername\"><br><label for=\"mqttpassword\">MQTT Password: (optional - empty is default)</label><br><input type=\"password\" name=\"mqttpassword\"><br><label for=\"temp\">Temperature when offline (is being synced with temperature set on NamNoc Hub, currently set " + offline_temp + ")</label><br><input type=\"number\" step=\"0.01\" name=\"temp\"><br><label for=\"hist\">Hysteresis when offline (is being synced with hysteresis set on NamNoc Hub, currently set " + offline_hist + ")</label><br><input type=\"number\" step=\"0.01\" name=\"hist\"><br><br><input type=\"submit\" value=\"Submit\"></form><h3>Firmware upgrade</h3><a href=\"/update\">Firmware upgrade</a>";
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
  offline_temp = eepromRead(TEMPADDR, HISTADDR).toDouble();
  offline_hist = eepromRead(HISTADDR, EPROMENDADDR).toDouble();
}

void setup()
{
  // More initialization stuff like, but not limited to, reading parameters from EEPROM, connecting to AP, setting MQTT Broker parameters and so on.
  pinMode(BUTTON, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  blinking.attach(0.2, blinkingcb);

  delay(500);
  initializeVars();
  tempSensor.init();
  WiFi.begin(ssid.c_str(), wpa.c_str());
  mac = WiFi.macAddress();
  WiFi.softAP(mac);

  mqtt.init();
  httpUpdater.setup(&server);
  MDNS.addService("http", "tcp", 80);

  server.on("/", handle_home_page);
  server.on("/settings", handle_settings);
  server.begin();

  initial.attach(0.5, initcb);
  minut.attach(60, minutcb);
}

void loop()
{
  handleFactoryReset();
  mqtt.loop();
  valve.handlEvents();
  server.handleClient();

  if (initb) // When connected to wifi sucessfully
  {
    initial.detach();
    initb = 0;
    minutb = 1;
    mqtt.connect();
    sub = mqtt.subscribe();
  }

  // Tasks performed every minute - measuring temperature and publishing MQTT message, also controling connection with hub and decision making when conection lost
  if (minutb)
  {
    minutb = 0;

    lastThreeAvgTemp = tempSensor.getTemperature();
    mqtt.publish();

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
        mqtt.connect();
        sub = mqtt.subscribe();
      }
      else if (!sub)
        sub = mqtt.subscribe();

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
}