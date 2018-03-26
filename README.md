# Sonoff Socket S20 firmware

Firmware for [Sonoff Socket S20](https://www.banggood.com/SONOFF-S20-10A-2200W-Wifi-Wireless-Remote-Control-Socket-Smart-Timer-Plug-Smart-Home-Power-Socket-Support-Alexa-p-1142285.html?p=5O07141883558201507E)
This firmware supports MQTT and Home Assistant

#### Configuration

You can see config.example.h, copy it to config.h and fill out the variables:
```c
#define DEVICE_NAME "esp8266-sonoff-socket-1"

#define MQTT_BROKER_ADDRESS ""
#define MQTT_BROKER_PORT 1883
#define MQTT_STATE_TOPIC ""
#define MQTT_CONTROL_TOPIC ""
#define MQTT_DEVICE_TOPIC ""

const char *sta_ssid = "";
const char *sta_password = "";
```

#### Firmware updates OTA

Example command:
```bash
mosquitto_pub -h {MQTT_BROKER_ADDRESS} -t '{MQTT_DEVICE_TOPIC}{DEVICE_ID}/update' -r -f ./sonoff_socket.ino.bin
```

#### Home Assistant

configuration.yaml:
```
switch:
  - platform: mqtt
    name: "Éjjeli lámpa"
    state_topic: "smarthome/bedroom/nightlamp/state"
    command_topic: "smarthome/bedroom/nightlamp"
    payload_on: "on"
    payload_off: "off"
    qos: 1
    retain: true
group:
  haloszoba:
    name: Hálószoba
    control: hidden
    entities:
      - switch.ejjeli_lampa
```
customize.yml
```
  switch.ejjeli_lampa:
    icon: mdi:lamp
```



For more information check out my [page](https://gyengus.hu/2017/10/sonoff-smart-socket-es-home-assistant/).
