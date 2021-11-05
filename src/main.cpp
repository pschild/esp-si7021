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

const String CHIP_ID = String(ESP.getChipId());
const String CLIENT_NAME = String("ESP_") + CHIP_ID;

const String dhtChannel = String("devices/") + CHIP_ID + String("/dht");
const String tempChannel = String("devices/") + CHIP_ID + String("/temperature");
const String humChannel = String("devices/") + CHIP_ID + String("/humidity");
const String voltageChannel = String("devices/") + CHIP_ID + String("/voltage");

void ping();
void publishValues();
void startDeepSleep();
void ledTurnOn();
void ledTurnOff();
void onHeaterChange(char* payload);
void onFooBar(char* payload);
void onOtaUpdate(char* payload);
void onMqttConnected();
void onMqttMessage(char* topic, char* message);

WifiHandler wifiHandler(WIFI_SSID, WIFI_PASSWORD);
MqttHandler mqttHandler("192.168.178.28", CLIENT_NAME);
OTAUpdateHandler updateHandler("192.168.178.28:9042", VERSION);

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

  // start OTA update immediately
  updateHandler.startUpdate();
}

void loop() {
  mqttHandler.loop();
  updateHandler.loop();
  deepSleepTimer.update();
}

void ledTurnOn() {
  digitalWrite(LED_BUILTIN, LOW);
}

void ledTurnOff() {
  digitalWrite(LED_BUILTIN, HIGH);
}

void ping() {
  const String channel = String("devices/") + CHIP_ID + String("/ping");
  mqttHandler.publish(channel.c_str(), VERSION);
}

String buildJsonDoc(float temperature, float humidity) {
  StaticJsonDocument<64> jsonDoc;
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;
  String output;
  serializeJson(jsonDoc, output);
  return output;
}

String buildJsonDocSingle(String key, float value) {
  StaticJsonDocument<64> jsonDoc;
  jsonDoc[key] = value;
  String output;
  serializeJson(jsonDoc, output);
  return output;
}

void publishVoltageLevel() {
  float volts = ESP.getVcc();
  mqttHandler.publish(voltageChannel.c_str(), buildJsonDocSingle("value", volts / 1024.00f).c_str());
}

void publishValues() {
  float humidity = sensor.readHumidity();
  float temperature = sensor.readTemperature();

  mqttHandler.publish(tempChannel.c_str(), buildJsonDocSingle("value", temperature).c_str());
  mqttHandler.publish(humChannel.c_str(), buildJsonDocSingle("value", humidity).c_str());
  mqttHandler.publish(dhtChannel.c_str(), buildJsonDoc(temperature, humidity).c_str());
}

void startDeepSleep() {
  // ESP.deepSleep(15 * 60e6); // sleep for 15min
  // delay(100);
}

void onHeaterChange(char* payload) {
  if (strcmp(payload, "on") == 0) {
    // sensor.setHeatLevel(SI_HEATLEVEL_HIGHEST);
    sensor.heater(true);
  } else if (strcmp(payload, "off") == 0) {
    sensor.heater(false);
  }
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

  const String otaDeviceChannel = String("ota/") + CHIP_ID;
  mqttHandler.subscribe(otaDeviceChannel.c_str());
  const String otaAllDevicesChannel = "ota/all";
  mqttHandler.subscribe(otaAllDevicesChannel.c_str());

  const String heaterChannel = String("devices/") + CHIP_ID + String("/heater");
  mqttHandler.subscribe(heaterChannel.c_str());

  ping();
  publishVoltageLevel();
  publishValues();
  // deepSleepTimer.start(); // start deep sleep timer to allow enough time for the values to be sent
}

void onMqttMessage(char* topic, char* message) {
  mqttHandler.publish(String("debug/onMqttMessage").c_str(), topic);
  if (String(topic).startsWith("foo/")) {
    onFooBar(message);
  } else if (String(topic).startsWith("ota/")) {
    onOtaUpdate(message);
  } else if (String(topic).equals(String("devices/") + CHIP_ID + String("/heater"))) {
    mqttHandler.publish(String("debug/heater").c_str(), message);
    onHeaterChange(message);
  } else if (String(topic).equals(String("devices/") + CHIP_ID + String("/readnow"))) {
    mqttHandler.publish(String("debug/readnow").c_str(), topic);
    publishVoltageLevel();
    publishValues();
  }
}
