# Sonoff Socket S20 firmware

Firmware for [Sonoff Socket S20](https://www.banggood.com/SONOFF-S20-10A-2200W-Wifi-Wireless-Remote-Control-Socket-Smart-Timer-Plug-Smart-Home-Power-Socket-Support-Alexa-p-1142285.html?p=5O07141883558201507E)
This firmware supports MQTT and Home Assistant

#### Configuration

You can see buildConfig.example.json, copy it to buildConfig.json and fill out the variables:
```json
{
	"arduino": {
		"root": "/opt/arduino/",
		"cmd": "arduino-builder",
		"hardware": "hardware",
		"tools": "tools-builder",
		"libraries": "libraries",
		"src": "sonoff_socket.ino",
		"build-path": "build"
	},
	"mqtt": {
		"host": "192.168.x.x",
		"port": 1883,
		"clientId": "device-updater",
		"name": "username",
		"password": "secret password",
		"deviceTopicPrefix": "device/"
	},
	"wifi": {
		"ssid": "",
		"password": ""
	},
	"devices": {
		"sonoff-socket-s20": {
			"mqttHost": "192.168.x.x",
			"mqttPort": 1883,
			"mqttUser": "sonoff",
			"mqttPassword": "password",
			"topic": "",
			"fqbn": "esp8266com:esp8266:generic",
			"hardware": "hardware/esp8266com/esp8266/libraries",
			"tools": "hardware/esp8266com/esp8266/tools/",
			"libraries": "hardware/esp8266com/esp8266/libraries",
			"prefs": [
				"build.flash_ld=/opt/arduino/hardware/esp8266com/esp8266/tools/sdk/ld/eagle.flash.1m64.ld",
				"build.flash_freq=40",
				"build.flash_size=1M",
				"build.flash_mode=DOUT",
				"build.f_cpu=80000000"
			]
		}
	}
}

```
#### Requirements

- Arduino, tested with 1.8.5
- [Arduino core for ESP8266 WiFi chip](https://github.com/esp8266/Arduino)
- NodeJS
- Gulp

#### Install dependencies

```bash
npm install
```

#### Generate configuration, build and update firmware

Just run this command:
```bash
gulp --device=sonoff-socket-s20
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
customize.yml:
```
  switch.ejjeli_lampa:
    icon: mdi:lamp
```



For more information check out my [page](https://gyengus.hu/2017/10/sonoff-smart-socket-es-home-assistant/).
