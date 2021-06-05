#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <WifiHandler.h>
#include <MqttHandler.h>
#include <OTAUpdateHandler.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "Adafruit_Si7021.h"

#ifndef WIFI_SSID
  #error "Missing WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
  #error "Missing WIFI_PASSWORD"
#endif

#ifndef VERSION
  #error "Missing VERSION"
#endif

const String CHIP_ID = String("ESP_") + String(ESP.getChipId());

const String dhtChannel = String("devices/") + CHIP_ID + String("/dht");
const String tempChannel = String("devices/") + CHIP_ID + String("/temp");
const String humChannel = String("devices/") + CHIP_ID + String("/hum");
const String voltageChannel = String("devices/") + CHIP_ID + String("/voltage");
const String debugChannel = String("devices/") + CHIP_ID + String("/debug");

// void ping();
void publishValues();
void startDeepSleep();
void ledTurnOn();
void ledTurnOff();
void onFooBar(char* payload);
void onOtaUpdate(char* payload);
void onMqttConnected();
void onMqttMessage(char* topic, char* message);

WifiHandler wifiHandler(WIFI_SSID, WIFI_PASSWORD);
MqttHandler mqttHandler("192.168.178.28", CHIP_ID);
OTAUpdateHandler updateHandler("192.168.178.28:9042", VERSION);

// Ticker pingTimer(ping, 60 * 1000);
Ticker deepSleepTimer(startDeepSleep, 15 * 1000); // start deepsleep 15s after boot

Adafruit_Si7021 sensor = Adafruit_Si7021();

ADC_MODE(ADC_VCC); // allows you to monitor the internal VCC level

void setup() {
  Serial.begin(9600);
  sensor.begin();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  wifiHandler.connect();
  mqttHandler.setup();
  mqttHandler.setOnConnectedCallback(onMqttConnected);
  mqttHandler.setOnMessageCallback(onMqttMessage);
  // pingTimer.start();

  // start OTA update immediately
  updateHandler.startUpdate();
}

void loop() {
  mqttHandler.loop();
  updateHandler.loop();
  // pingTimer.update();
  deepSleepTimer.update();
}

void ledTurnOn() {
  digitalWrite(LED_BUILTIN, LOW);
}

void ledTurnOff() {
  digitalWrite(LED_BUILTIN, HIGH);
}

/*void ping() {
  const String channel = String("devices/") + CHIP_ID + String("/version");
  mqttHandler.publish(channel.c_str(), VERSION);
}*/

void publishVoltageLevel() {
  float volts = ESP.getVcc();
  mqttHandler.publish(voltageChannel.c_str(), String(volts / 1024.00f).c_str());
  mqttHandler.publish(debugChannel.c_str(), String(volts).c_str());
}

String buildJsonDoc(float temperature, float humidity) {
  StaticJsonDocument<64> jsonDoc;
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;
  String output;
  serializeJson(jsonDoc, output);
  return output;
}

void publishValues() {
  float humidity = sensor.readHumidity();
  float temperature = sensor.readTemperature();

  mqttHandler.publish(tempChannel.c_str(), String(temperature).c_str());
  mqttHandler.publish(humChannel.c_str(), String(humidity).c_str());
  mqttHandler.publish(dhtChannel.c_str(), buildJsonDoc(temperature, humidity).c_str());
}

void startDeepSleep() {
  ESP.deepSleep(15 * 60e6); // sleep for 15min
  delay(100);
}

void onFooBar(char* payload) {
  if (strcmp(payload, "on") == 0) {
    ledTurnOn();
  } else if (strcmp(payload, "off") == 0) {
    ledTurnOff();
  }
}

void onOtaUpdate(char* payload) {
  updateHandler.startUpdate();
}

void onMqttConnected() {
  mqttHandler.subscribe("foo/+/baz");
  mqttHandler.subscribe("otaUpdate/all");

  publishVoltageLevel();
  publishValues();
  deepSleepTimer.start(); // start deep sleep timer to allow enough time for the values to be sent
}

void onMqttMessage(char* topic, char* message) {
  if (((std::string) topic).rfind("foo/", 0) == 0) {
    onFooBar(message);
  } else if (strcmp(topic, "otaUpdate/all") == 0) {
    onOtaUpdate(message);
  }
}
