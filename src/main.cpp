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
 
#define ONE_WIRE_BUS 13
#define OPEN 5
#define CLOSE 4
#define DETECT 12

#define SSIDADDR 0 //max ssid 32 chars + one for termination \0
#define WPAADDR 33 //max wpa2 pass 63 chars + one for termination \0
#define MQTTSADDR 97 //max broaker address 115 chars + one for termination \0
#define MQTTPORTADDR 213 //max port length (chars) 5 + one for termination \0
#define MQTTTADDR 219 //publish topic
#define MQTTUADDR 320 //username
#define MQTTPADDR 409 //password
#define EPROMENDADDR 511


String ssid = ""; //ssid
String wpa = ""; //wifipassword
String mqtts = ""; //mqtt broaker address
String mqttport = "";//mqtt port
String mqttt = "";//mqtt NamNoc's Hub topic
String mqttu = "";//mqtt username
String mqttp = "";//mqtt password
String ip = "";

double offline_temp = 21.0;
double offline_hist = 0.2;

String payloadtosend = "";


bool sub = 0; //Successfull subscription to mqtt topic. 
bool connblink = 1; //Blinking bool
bool softAP = 1; //Soft AP status
bool opened = 1;
String mac; //Stores devices MAC ADDRESS
char macchar[18]; //Stores devices MAC ADDRESS

//Ticker for operations performed every ten secounds - Before first connection to WiFi those operations are performed much more frequently!!!
Ticker tensec;
bool tensecb = 0;
void tenseccb()
{
  tensecb = 1;
}

//Ticker for operations performed every minute.
Ticker minut;
bool minutb = 0;
void minutcb()
{
  minutb = 1;
}

//Ticker for initial jobs like initial connection to WiFi
Ticker initial;
bool initb = 0;
void initcb()
{
  initb = WiFi.status() == WL_CONNECTED;
}

//Ticker for stopping motor if detection of rotation end failes.
Ticker stop;
bool stopb = 0;
void stopcb()
{
  stopb = 1;
}

//Ticker for blinking ;)
Ticker blinking;
void blinkingcb()
{
    digitalWrite(LED_BUILTIN, connblink);
    connblink = !connblink;
}

//Some initialisation staff
WiFiClient wlan;
PubSubClient mqtt(wlan);
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//Callback for mqtt subscribed topic message
void callback(char* topic, byte* payload, unsigned int length) {
  String debugmess; //debug
  payload[length] = '\0';
  String payload_string((char*)payload);
  if(payload_string == "open")
    {
      digitalWrite(CLOSE, LOW);
      digitalWrite(OPEN, HIGH);
      opened = 1;
      stop.attach(4, stopcb);
    } else if(payload_string == "close")
    {
      digitalWrite(CLOSE, HIGH);
      digitalWrite(OPEN, LOW);
      opened = 0;
      stop.attach(5, stopcb);
    } else if (payload_string.length() > 9)
    {
        if(payload_string.substring(0,9) == "heartbeat")
        {
            int pos = payload_string.indexOf(";");
            offline_temp = payload_string.substring(10, pos).toDouble();
            offline_hist = payload_string.substring(pos+1).toDouble();
            debugmess = String(offline_temp) + " " + String(offline_hist); //debug
        }
    }
    payloadtosend = "{\"id\":\""+ mac +"\", \"ip\":\""+ ip +"\", \"temp\":" + String(sensors.getTempCByIndex(0)) + ", \"opened\":" + opened + "}";
    mqtt.publish(mqttt.c_str(), payloadtosend.c_str());
    mqtt.publish("debug", debugmess.c_str()); //debug
    delay(500);
}

//Callback for web requests for mainpage - there is configuration form and href to firmware upgrade option.
void handle_home_page() {
  String html = "<h3>Settings</h3><form action=\"/settings\" method=\"post\" enctype=\"multipart/form-data\"><label for=\"ssid\">SSID: (currently set - \""+ ssid +"\")</label><br><input type=\"text\" name=\"ssid\"><br><label for=\"wpa\">WiFi Password:</label><br><input type=\"password\" name=\"wpa\"><br><label for=\"mqttserver\">MQTT Broaker address: (currently set - \""+ mqtts +"\")</label><br><input type=\"text\" name=\"mqttserver\"><br><label for=\"mqttport\">MQTT Broaker port (1883 if default, currently set - \""+ mqttport +"\"):</label><br><input type=\"text\" name=\"mqttport\"><br><label for=\"mqtttopic\">MQTT NamNoc's Hub topic: (NamNoc if default, currently set - \""+ mqttt +"\")</label><br><input type=\"text\" name=\"mqtttopic\"><br><label for=\"mqttusername\">MQTT Username: (optional - empty is default, currently set - \""+ mqttu +"\")</label><br><input type=\"text\" name=\"mqttusername\"><br><label for=\"mqttpassword\">MQTT Password: (optional - empty is default)</label><br><input type=\"password\" name=\"mqttpassword\"><br><input type=\"submit\" value=\"Submit\"></form><h3>Firmware upgrade</h3><a href=\"/update\">Firmware upgrade</a>";
  server.send(200, "text/html", html.c_str());
}

//Dealing with change of configuration - callback for web request of /settings - it writes values submitted from mainpage to eeprom. TODO: Add verification of users data!!! Not necessary right now!
void handle_settings() {
  EEPROM.begin(512);
  String ssidtemp = server.arg(0);
  String wpatemp = server.arg(1);
  String mqttstemp = server.arg(2);
  String mqttporttemp = server.arg(3);
  String mqttttemp = server.arg(4);
  String mqttutemp = server.arg(5);
  String mqttptemp = server.arg(6);
  if(server.args() == 7)
  {
    for(unsigned int i = 0; i < ssidtemp.length(); ++i)
    {
      EEPROM.write(SSIDADDR + i, ssidtemp[i]);
    }
    EEPROM.write(SSIDADDR + ssidtemp.length(), '\0');

    for(unsigned int i = 0; i < wpatemp.length(); ++i)
    {
      EEPROM.write(WPAADDR + i, wpatemp[i]);
    }
    EEPROM.write(WPAADDR + wpatemp.length(), '\0');

    for(unsigned int i = 0; i < mqttstemp.length(); ++i)
    {
      EEPROM.write(MQTTSADDR + i, mqttstemp[i]);
    }
    EEPROM.write(MQTTSADDR + mqttstemp.length(), '\0');

    for(unsigned int i = 0; i < mqttporttemp.length(); ++i)
    {
      EEPROM.write(MQTTPORTADDR + i, mqttporttemp[i]);
    }
    EEPROM.write(MQTTPORTADDR + mqttporttemp.length(), '\0');

    for(unsigned int i = 0; i < mqttttemp.length(); ++i)
    {
      EEPROM.write(MQTTTADDR + i, mqttttemp[i]);
    }
    EEPROM.write(MQTTTADDR + mqttttemp.length(), '\0');

    for(unsigned int i = 0; i < mqttutemp.length(); ++i)
    {
      EEPROM.write(MQTTUADDR + i, mqttutemp[i]);
    }
    EEPROM.write(MQTTUADDR + mqttutemp.length(), '\0');

    for(unsigned int i = 0; i < mqttptemp.length(); ++i)
    {
      EEPROM.write(MQTTPADDR + i, mqttptemp[i]);
    }
    EEPROM.write(MQTTPADDR + mqttptemp.length(), '\0');
    EEPROM.end();
    server.send(200, "text/html", "It worked! Rebooting now!");
    delay(5000);
    ESP.restart(); //Reset after (hopefully) successfull write to EEPROM
  }
  server.send(200, "text/html", "Something went wrong! Please <a href=\"/\">go back</a> and try again!"); //Let the user know he did something wrong. Or maybe it's our fault. But we don't need to tell him that :)
}


void setup() {
  //More initialization staff like, but not limited to, reading parameters from EEPROM, connecting to AP, setting MQTT Broker parameters and so on.
  pinMode(DETECT, INPUT_PULLUP);
  pinMode(CLOSE, OUTPUT);
  digitalWrite(CLOSE, LOW);
  pinMode(OPEN, OUTPUT);
  digitalWrite(OPEN, LOW);
  pinMode(LED_BUILTIN, OUTPUT);

  blinking.attach(0.2, blinkingcb);

  delay(500);
  char current;
  EEPROM.begin(512);
  for(int i = SSIDADDR; i < WPAADDR-1; ++i)
  {
    current = EEPROM.read(i);
    if(current == '\0') break;
    ssid += current;
  }

  for(int i = WPAADDR; i < MQTTSADDR-1; ++i)
  {
    current = EEPROM.read(i);
    if(current == '\0') break;
    wpa += current;
  }

for(int i = MQTTSADDR; i < MQTTPORTADDR-1; ++i)
  {
    current = EEPROM.read(i);
    if(current == '\0') break;
    mqtts += current;
  }

  for(int i = MQTTPORTADDR; i < MQTTTADDR-1; ++i)
  {
    current = EEPROM.read(i);
    if(current == '\0') break;
    mqttport += current;
  }

  for(int i = MQTTTADDR; i < MQTTUADDR-1; ++i)
  {
    current = EEPROM.read(i);
    if(current == '\0') break;
    mqttt += current;
  }

  for(int i = MQTTUADDR; i < MQTTPADDR-1; ++i)
  {
    current = EEPROM.read(i);
    if(current == '\0') break;
    mqttu += current;
  }

  for(int i = MQTTPADDR; i < EPROMENDADDR; ++i)
  {
    current = EEPROM.read(i);
    if(current == '\0') break;
    mqttp += current;
  }
 EEPROM.end();

  sensors.begin();
  WiFi.begin(ssid.c_str(), wpa.c_str());
  mac = WiFi.macAddress();
  mac.toCharArray(macchar, mac.length()+1);
  WiFi.softAP(mac);

  mqtt.setServer(mqtts.c_str(),mqttport.toInt());
  mqtt.setCallback(callback);
  httpUpdater.setup(&server);
  MDNS.addService("http", "tcp", 80);

  server.on("/", handle_home_page);
  server.on("/settings", handle_settings);
  server.begin();

  initial.attach(0.5, initcb);
  //Open valve
  digitalWrite(OPEN, HIGH);
  digitalWrite(CLOSE, LOW);
  stop.attach(4, stopcb);
  delay(500);
}

void loop() {
  mqtt.loop();

  if(!digitalRead(DETECT) || stopb) //Stop the motor if end of rotation is detected or emergency ticker ticked. 
  {
    stop.detach();
    digitalWrite(OPEN, LOW);
    digitalWrite(CLOSE, LOW);
    stopb = 0;

    mqtt.publish(mqttt.c_str(), "LoL");
  }

//If initial wifi connection is done start the ten second ticker.
  if(initb)
  {
    initial.detach();
    ip = WiFi.localIP().toString();
    initb = 0;
    tensecb = 1;
    tensec.attach(10, tenseccb);
    minut.attach(60, minutcb);
  }

//Operations performed every ten seconds - checking if mqtt connection is fine
  if(tensecb){
      tensecb = 0;
      if(mqtt.state())
      {
        if(!softAP){
          WiFi.softAP(mac);
          softAP = 1;
          blinking.attach(0.5, blinkingcb);
        }
        mqtt.connect(macchar, mqttu.c_str(), mqttp.c_str());
        sub = mqtt.subscribe(macchar);
        tensecb = 1;
      } else 
      {
        if(softAP) {
          WiFi.softAPdisconnect(1);
          blinking.detach();
          digitalWrite(LED_BUILTIN, HIGH);
          softAP = 0;
        }
      }
      if(!sub) 
      {
        sub = mqtt.subscribe(macchar);
      }
  }

  if(minutb){
    minutb = 0;
    sensors.requestTemperatures();
    payloadtosend = "{\"id\":\""+ mac +"\", \"ip\":\""+ ip +"\", \"temp\":" + String(sensors.getTempCByIndex(0)) + ", \"opened\":" + opened + "}";
    mqtt.publish(mqttt.c_str(), payloadtosend.c_str());
  }

  server.handleClient();
}