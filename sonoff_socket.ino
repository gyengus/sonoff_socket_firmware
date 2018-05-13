#include "config.h"

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <Ticker.h>
#include "PubSubClient.h"

#define STATLED 13
#define RELAY 12
#define BUTTON 0

#define SPIFFS_ALIGNED_OBJECT_INDEX_TABLES 1
#define DEBUG_HTTP_UPDATE Serial.printf

ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

int relayState = 0;
long lastMQTTReconnectAttempt = 0;
long lastWiFiReconnectAttempt = 0;
Ticker btn_timer;
unsigned long btnCount = 0;
String MQTT_UPDATE_TOPIC_FULL = "";
boolean requireRestart = false;

void setRelay(boolean state) {
  digitalWrite(RELAY, state);
  relayState = state;
  publishToMQTT();
}

void handleRoot() {
  String body = "<!doctype html><html lang='hu'><head><title>Sonoff Socket</title><meta charset='UTF-8'/><style>body{margin: 0px;padding: 20px;font: 14px Verdana,Arial,sans-serif;}#container{display: block;border: 1px solid #dddddd;border-radius: 3px;box-shadow: 0 3px 5px 0 rgba(50,50,50,.25);width: 400px;max-width: 500px;margin: 0px auto;background: #ffffff;padding: 10px;}#sensor_name{font-size: 1.2em;text-align: center;font-weight: bold;margin-bottom: 10px;border-bottom: 1px solid #dddddd;}.left{float: left;width: 180px;}.right{float: right;text-align: right;width: 210px;}.clearboth{clear: both;height: 5px;}</style></head><body><div id='container'><div id='sensor_name'>" + String(DEVICE_NAME) + "</div>"
              + "<div class='left'>Serial number</div><div class='right'>" + String(ESP.getChipId()) + "</div>"
              + "<div class='clearboth'></div>"
              + "<div class='left'>Sketch size</div><div class='right'>" + String(ESP.getSketchSize()) + " B</div>"
              + "<div class='clearboth'></div>"
              + "<div class='left'>Free size</div><div class='right'>" + String(ESP.getFreeSketchSpace()) + " B</div>"
              + "<div class='clearboth'></div>"
              + "<div class='left'>Flash size</div><div class='right'>" + String(ESP.getFlashChipSize()) + " B</div>"
              + "<div class='clearboth'></div>"
              + "<div class='left'>Real flash size</div><div class='right'>" + String(ESP.getFlashChipRealSize()) + " B</div>"
              + "<div class='clearboth'></div>"
              + "<div class='left'>MQTT firmware update topic</div><div class='right'>" + MQTT_UPDATE_TOPIC_FULL + "</div>"
              + "<div class='clearboth'></div>"
              + "<div class='left'>Konnektor</div><div class='right'>" + (relayState ? "bekapcsolva" : "kikapcsolva") + "</div>"
              + "<div class='clearboth'></div>"
              + "<div class='left'>MQTT</div><div class='right'>" + (client.connected() ? "OK" : "nincs kapcsolat") + "</div>"
              + "<div class='clearboth'></div>"
              + "</div></body></html>";
  server.send(200, "text/html", body);
}

boolean connectToMQTT() {
  String clientId = DEVICE_NAME;
  if (client.connect(clientId.c_str())) {
    client.subscribe(MQTT_CONTROL_TOPIC);
    client.subscribe(MQTT_UPDATE_TOPIC_FULL);
    publishToMQTT();
  }
  return client.connected();
}

void receiveFromMQTT(const MQTT::Publish& pub) {
  Serial.print("Message arrived [");
  Serial.print(pub.topic());
  Serial.print("] Size: " + String(pub.payload_len()) + " B");
  
  Serial.println();

  digitalWrite(STATLED, false);
  if (pub.topic() == MQTT_CONTROL_TOPIC) {
	  if (pub.payload_string() == "on") {
		  setRelay(true);
	  } else {
		  setRelay(false);
	  }
  } else if (pub.topic() == MQTT_UPDATE_TOPIC_FULL) {
    ESP.wdtFeed();
    uint32_t size = pub.payload_len();
    if (size == 0) {
      Serial.println("Error, sketch size is 0");
    } else {
      Serial.println("Receiving OTA of " + String(size) + " bytes...");
      Serial.setDebugOutput(true);
      ESP.wdtFeed();
      if (ESP.updateSketch(*pub.payload_stream(), size, false, false)) {
        ESP.wdtFeed();
        Serial.println("Clearing retained message.");
        client.publish(MQTT::Publish(pub.topic(), "").set_retain());
        Serial.println("Update success");
        requireRestart = true;
      }
    }
  }
  digitalWrite(STATLED, true);
}

void publishToMQTT() {
  if (client.connected()) {
    client.publish(MQTT::Publish(MQTT_STATE_TOPIC, (relayState ? "on" : "off")).set_retain());
  }
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
  ESP.wdtDisable();
  pinMode(STATLED, OUTPUT);
  digitalWrite(STATLED, false);
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, false);
  pinMode(BUTTON, INPUT);

  Serial.begin(115200);
  delay(100);

  Serial.println("");

  connectToWiFi();

  server.on("/", handleRoot);
  server.begin();

  ESPhttpUpdate.rebootOnUpdate(false);

  MQTT_UPDATE_TOPIC_FULL = MQTT_DEVICE_TOPIC + String(ESP.getChipId()) + String("/update");
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
    requireRestart = false;
    Serial.println("Reboot...");
    client.disconnect();
    ESP.wdtDisable();
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
      long now = millis();
      if (now - lastMQTTReconnectAttempt > 5000) {
        if (connectToMQTT()) {
          lastMQTTReconnectAttempt = 0;
        } else {
          lastMQTTReconnectAttempt = millis();
        }
      }
    }
  } else {
    long now = millis();
    if (now - lastWiFiReconnectAttempt > 5000) {
      if (connectToWiFi()) {
        lastWiFiReconnectAttempt = 0;
      } else {
        lastWiFiReconnectAttempt = millis();
      }
    }
  }
}

