#include "config.h"

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>
#include "PubSubClient.h"

#define STATLED 13
#define RELAY 12
#define BUTTON 0

#define SPIFFS_ALIGNED_OBJECT_INDEX_TABLES 1

ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

String macAddress = "";
int relayState = 0;
unsigned long lastMQTTReconnectAttempt = 0;
unsigned long lastWiFiReconnectAttempt = 0;
Ticker btn_timer;
unsigned long btnCount = 0;
String MQTT_UPDATE_TOPIC_FULL = "";
String MQTT_DEVICE_TOPIC_FULL = "";
String MQTT_STATE_TOPIC = "";
boolean requireRestart = false;

bool publishToMQTT(String topic, String payload, bool retain = true);

void setRelay(boolean state) {
  digitalWrite(RELAY, state);
  relayState = state;
  publishToMQTT(MQTT_STATE_TOPIC, (relayState ? "on" : "off"), true);
}

void serveJSON() {
  String json = "{\"buildDate\": \"" + String(BUILD_DATE) + "\","
              + "\"deviceName\": \"" + String(DEVICE_NAME) + "\","
              + "\"deviceDescription\": \"" + String(DEVICE_DESCRIPTION) + "\","
              + "\"mac\": \"" + macAddress + "\","
              + "\"sketchSize\": \"" + String(ESP.getSketchSize()) + "\","
              + "\"freeSketchSize\": \"" + String(ESP.getFreeSketchSpace()) + "\","
              + "\"flashChipSize\": \"" + String(ESP.getFlashChipSize()) + "\","
              + "\"realFlashSize\": \"" + String(ESP.getFlashChipRealSize()) + "\","
              + "\"updateTopic\": \"" + MQTT_UPDATE_TOPIC_FULL + "\","
              + "\"relayState\": \"" + (relayState ? "on" : "off") + "\","
              + "\"mqttState\": \"" + (client.connected() ? "" : "dis") + "connected\""
              + "}";
  server.send(200, "application/json", json);
}

void mqttDisConnect() {
  if (client.connected()) {
    client.disconnect();
    delay(10);
  }
  serveJSON();
}
void mqttReConnect() {
  if (client.connected()) {
    client.disconnect();
    delay(50);
  }
  connectToMQTT();
  lastMQTTReconnectAttempt = 0;
  delay(10);
  serveJSON();
}

boolean connectToMQTT() {
  Serial.print("Trying to connect MQTT broker...");
  MQTT::Connect con(DEVICE_NAME);
  con.set_clean_session();
  con.set_will(MQTT_DEVICE_TOPIC_FULL, "");
  con.set_keepalive(30);
  if (MQTT_USER) {
    con.set_auth(MQTT_USER, MQTT_PASSWORD);
  }
  if (client.connect(con)) {
    Serial.println(" success");
    client.subscribe(MQTT_TOPIC);
    client.subscribe(MQTT_UPDATE_TOPIC_FULL);
    String deviceData = "{\"name\": \"" + String(DEVICE_NAME) + "\", \"deviceDescription\": \"" + String(DEVICE_DESCRIPTION) + "\", \"ip\": \"" + WiFi.localIP().toString() + "\", \"mac\": \"" + macAddress + "\", \"buildDate\": \"" + String(BUILD_DATE) + "\"}";
    publishToMQTT(MQTT_DEVICE_TOPIC_FULL, deviceData, true);
    publishToMQTT(MQTT_STATE_TOPIC, (relayState ? "on" : "off"), true);
  } else {
    Serial.println(" failed");
  }
  return client.connected();
}

void receiveFromMQTT(const MQTT::Publish& pub) {
  Serial.print("Message arrived [");
  Serial.print(pub.topic());
  Serial.print("] Size: " + String(pub.payload_len()) + " B");

  Serial.println();

  digitalWrite(STATLED, false);
  if (pub.topic() == MQTT_TOPIC) {
	  if (pub.payload_string() == "on") {
		  setRelay(true);
	  } else {
		  setRelay(false);
	  }
  } else if (pub.topic() == MQTT_UPDATE_TOPIC_FULL) {
    ESP.wdtFeed();
    uint32_t size = pub.payload_len();
    if (size == 0) {
      Serial.println("Error, sketch size is 0 B");
    } else {
      Serial.println("Receiving firmware update of " + String(size) + " bytes...");
      Serial.setDebugOutput(false);
      ESP.wdtFeed();
      if (ESP.updateSketch(*pub.payload_stream(), size, false, false)) {
        ESP.wdtFeed();
        Serial.println("Clearing retained message.");
        publishToMQTT(MQTT_UPDATE_TOPIC_FULL, "", true);
        ESP.wdtFeed();
        Serial.println("Update success");
        requireRestart = true;
      }
    }
  }
  digitalWrite(STATLED, true);
}

bool publishToMQTT(String topic, String payload, bool retain) {
  if (!client.connected()) {
    connectToMQTT();
  }
  if (retain) {
    client.publish(MQTT::Publish(topic, payload).set_retain());
  } else {
    client.publish(topic, payload);
  }
  client.loop();
  return true;
}

boolean connectToWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to WiFi network ");
  Serial.println(sta_ssid);
  WiFi.begin(sta_ssid, sta_password);

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    ESP.wdtFeed();
    delay(500);
    Serial.print(".");
    i++;
    if (i >= 20) break;
  }
  Serial.println("");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.print("Unable to connect\n");
    return false;
  }
}

void handleButton() {
  if (!digitalRead(BUTTON)) {
    btnCount++;
  } else {
    if (btnCount > 1 && btnCount <= 40) {
      setRelay(!relayState);
    } else if (btnCount > 40){
      requireRestart = true;
    } 
    btnCount=0;
  } 
}

void setup() {
  macAddress = WiFi.macAddress();
  ESP.wdtDisable();
  pinMode(STATLED, OUTPUT);
  digitalWrite(STATLED, false);
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, false);
  pinMode(BUTTON, INPUT);

  Serial.begin(115200);
  delay(100);

  Serial.println("");
  Serial.println("Build date: " + String(BUILD_DATE));
  Serial.println("MAC address: " + macAddress + "\n");

  connectToWiFi();

  server.on("/", serveJSON);
  server.on("/mqttreconnect", mqttReConnect);
  server.on("/mqttdisconnect", mqttDisConnect);
  server.begin();

  MQTT_DEVICE_TOPIC_FULL = MQTT_DEVICE_TOPIC + String(DEVICE_NAME);
  MQTT_UPDATE_TOPIC_FULL = MQTT_DEVICE_TOPIC_FULL + String("/update");
  MQTT_STATE_TOPIC = MQTT_TOPIC + String("/state");
  client.set_server(MQTT_BROKER_ADDRESS, MQTT_BROKER_PORT);
  client.set_callback(receiveFromMQTT);
  lastMQTTReconnectAttempt = 0;

  btn_timer.attach(0.05, handleButton);
  Serial.println("MQTT update topic: " + MQTT_UPDATE_TOPIC_FULL);
  digitalWrite(STATLED, true);
  ESP.wdtEnable(5000);
}

void loop() {
  ESP.wdtFeed();
  if (requireRestart) {
    client.loop();
    requireRestart = false;
    Serial.println("Reboot...");
    client.disconnect();
    ESP.wdtDisable();
    delay(200);
    ESP.restart();
    while (1) {
      ESP.wdtFeed();
      delay(200);
      digitalWrite(STATLED, !digitalRead(STATLED));
   }
  }
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
    if (client.connected()) {
      client.loop();
    } else {
      unsigned long now = millis();
      if (now - lastMQTTReconnectAttempt >= 5000) {
        if (connectToMQTT()) {
          lastMQTTReconnectAttempt = 0;
        } else {
          lastMQTTReconnectAttempt = millis();
        }
      }
    }
  } else {
    unsigned long now = millis();
    if (now - lastWiFiReconnectAttempt >= 2000) {
      if (connectToWiFi()) {
        lastWiFiReconnectAttempt = 0;
      } else {
        lastWiFiReconnectAttempt = millis();
      }
    }
  }
}

