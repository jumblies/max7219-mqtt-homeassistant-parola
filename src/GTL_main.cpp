/*
20181104 MAX7219 Display hooked to homeassistant  
Revised to add reset command MQTT topic

TODO:  Quirky Display Code.  May need to implement a 24 hour reset timer to keep display working.  For now,
have it in MQTT code to allow resets */
// uint16_t day_timer = 0;
// Add 24 millis() code
// Dropping segments was caused by flaky USB power supply in the island

// Must set #define USE_GENERIC_HW 0 in the MD_Max82xx.h file
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
// SOFTWARE SPI SETUP
#define MAX_DEVICES 4 // For WEMOS D1 MINI JST HEADERS/CLK=D7, DIN=D3, CS=D6
#define CLK_PIN D7
#define DATA_PIN D3
#define CS_PIN D6
// SOFTWARE SPI OBJECT INSTANTIATION
MD_Parola P = MD_Parola(DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

#include "secrets.h"
WiFiClient espClient;
PubSubClient client(espClient);

uint8_t scrollSpeed = 100; // default frame delay value
textEffect_t scrollEffect = PA_SCROLL_LEFT;
textPosition_t scrollAlign = PA_LEFT;
uint16_t scrollPause = 50; // in milliseconds

// Global message buffers shared by Serial and Scrolling functions
#define BUF_SIZE 75
char curMessage[BUF_SIZE] = {""};
char newMessage[BUF_SIZE] = {"reset enabled"};
bool newMessageAvailable = true;

//Nonblocking MQTT PubSub Reconnet
long lastReconnectAttempt;


void callback(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';
  if (strcmp(topic, "MAX7219/brightness") == 0)
  {
    P.setIntensity(atoi((char *)payload)); //The required value is 1 - 15 therefor the int conversion I guess
    Serial.println((char *)payload);
  }
  else if (strcmp(topic, "MAX7219/reset") == 0)
  {
    Serial.println("Resetting");
    ESP.restart();
  }
  else
  {
    //need to copy char array to new message Holy shit it works
    strcpy(newMessage, (char *)payload);
    newMessageAvailable = true;
  }
}

boolean reconnect()
{//Reference for connect method 
// boolean connect(const char* id, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);

  if (client.connect("Max-Race","MAX7219/LWT", /*QOS*/ 0, /*RETAIN*/1, /*WillMSG*/"Online"));
  {
    // Once connected, Subscribe to topics
    client.subscribe(topic);
    client.subscribe(topic2);
    client.subscribe(topic3);
  }
  Serial.println("MQTT Connected");
  return client.connected();
}

// THIS HAS BEEN DEPRECATED IN FAVOR OF A NONBLOCKING MQTT LOOP
// void reconnect()
// {
//   // Loop until we're reconnected
//   while (!client.connected())
//   {
//     Serial.print("Attempting MQTT connection...");
//     // Create a random client ID
//     String clientId = "Max-Race";
//     // clientId += String(random(0xffff), HEX);
//     // Attempt to connect
//     if (client.connect(clientId.c_str()))
//     {
//       Serial.println("connected");
//       client.subscribe(topic);
//       client.subscribe(topic2);
//       client.subscribe(topic3);
//     }
//     else
//     {
//       Serial.print("failed, rc=");
//       Serial.print(client.state());
//       // Serial.println(" try again in 5 seconds");
//       // Wait 5 seconds before retrying
//       // delay(5000);
//     }
//   }

void setup()
{
  lastReconnectAttempt = 0;

  Serial.begin(115200);

  Serial.print(F("Setting static ip to : "));
  Serial.println(ip);
  WiFi.config(ip, gateway, subnet);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  //OTA setup
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() { delay(500); });
  ArduinoOTA.onError([](ota_error_t error) {
    (void)error;
    ESP.restart();
  });
  ArduinoOTA.begin();
  Serial.println(WiFi.localIP());

  // newMessage = strcpy(newMessage, char* (WiFi.localIP().toString().c_str));
  P.begin();
  P.displayText(curMessage, scrollAlign, scrollSpeed, scrollPause, scrollEffect, scrollEffect);
}

void loop()
{
  if (!client.connected())
  { Serial.println("Entering NonBlocking Reconnect");
    long now = millis();
    if (now - lastReconnectAttempt > 5000)
    {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect())
      {
        lastReconnectAttempt = 0;
      }
    }
  }
  else
  {
    client.loop();
  }
  if (P.displayAnimate())
  {
    if (newMessageAvailable)
    {
      strcpy(curMessage, newMessage);
      newMessageAvailable = false;
    }
    P.displayReset();
  }
  ArduinoOTA.handle();

}
