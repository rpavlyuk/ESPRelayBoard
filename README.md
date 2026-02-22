[![Stand With Ukraine](https://raw.githubusercontent.com/vshymanskyy/StandWithUkraine/main/badges/StandWithUkraine.svg)](https://stand-with-ukraine.pp.ua)
[![Made in Ukraine](https://img.shields.io/badge/Made_in-Ukraine-ffd700.svg?labelColor=0057b7)](https://stand-with-ukraine.pp.ua)

# ESP32 Relay Board Firmware

## Purpose
This project provides firmware for an ESP32-based relay boards which you can find in great number at Aliexpress, Amazon or Botland. Alternatively, you can use this firmware if you want to contol any kind of relay's (typically those from SRD-**VDC-SL-C family) or just any digital electronics component that is controlled by HIGH/LOW logical levels.

The device supports two types of units:
* **Relay**: The board controls an actuator which is controlled by HIGH/LOW logical states
* **[Dry Contact Sensor](https://www.electrical4u.com/dry-contacts/)**: The board monitors contact state between selected GPIO(s) and GND, providing information either contact is closed or open

## Table of Contents
1. [Purpose](#purpose)
2. [Features](#features)
3. [Prerequisites: What you need to start](#prerequisites)
4. [Tested Relay Boards / Configurations](#tested-relay-boards--configurations)
5. [Wiring Sample](#wiring)
6. [Getting started: Building and Flashing](#building-and-flashing)
7. [Updating the firmware](#updating-the-firmware)
8. [Initiation: Making device to work](#initiation)
	- [First boot](#first-boot)
	- [WiFi Setup](#wifi-setup)
	- [Device Setup](#device-setup)
9. [WEB Setup](#web-setup)
10. [Units (Relays) Configuration](#units-configuration)
11. [Testing the Setup](#testing-the-setup)
	 - [Relay / Actuator](#relay--actuator)
	 - [Contact Sensor](#contact-sensor)
12. [Home Assistant Integration](#home-assistant-integration)
13. [OTA Firmware Update](#ota-firmware-update)
14. [WEB API](#web-api)
15. [Known issues, problems and TODOs](#known-issues-problems-and-todos)
16. [License and Credits](#license-and-credits)

## Features
- **Open Source**: You see what flash into your device and thus you know what it does from a security standpoint. And what it doesn't.
- **WiFi Connectivity**: Supports connecting to WiFi for remote monitoring and control.
- **Soft Configuration**: GPIO pins that relays are connected to and other settings are defined via user interface and are stored in NVS (EEPROM).
- **Relay State Memory**: Relay (actuators) states are written to NVS (EEPROM) memory and will be restored after power loss.
- **MQTT Support**: Publishes relay and contact sensors states via MQTT for further integration with various platforms.
- **Home Assistant Integration**: Automatically detects and configures devices in Home Assistant using auto-discovery via MQTT.
- **Web Interface**: Provides a web-based user interface for configuration, control and monitoring.
- **Web API**: Simple JSON API is in place should you want to integrate the device into your custom infractucture projects.
- **OTA (over the air) Firmware Update**: Trigger firmware update via WEB interface from a provided URL.
- **Remote/Network Logging**: The device supports remote logging using the Syslog protocol (RFC 3164 / RFC 5424) over UDP or TCP.
- **MemGuard**: Automatically reboot device if it runs out of free memory to prevent device stall.

## Prerequisites
To get started, you will need:
- **Hardware**:
  - ESP32, ESP32-C6 or ESP32-S3 microcontroller (all have been tested) with at least 4Mb flash.
  - Relay. E.g., SRD-05VDC-SL-C-based mechanical relay module.
  - **OR** ESP32-based relay board with minimum 4Mb flash in a single-board composition. Most of such boards use `ESP32` variant of ESP32. 
- **Software**:
  - ESP-IDF framework (version 5.5 or higher recommended installed and configured).
  - Operating system: `Linux` (tested, recommended), `macOS` (tested, recommended), `Windows` (tested)

## Tested Relay Boards / Configurations
* [LilyGO 4 Ch T-Relay](https://www.lilygo.cc/products/t-relay)
  * Assign pins to the relay channels according to LilyGO specs: `{21, 19, 18, 05}`
  * Use `ESP32` as target device.
* [ESP32_Relay x2](https://aliexpress.com/item/1005005926554704.html) from AliExpress
  * Use pins: 16 (channel 0) and 17 (channel 1). No inversion.
  * Use `ESP32` as target device.
* [ESP32-S3-DevKitM-1](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/hw-reference/esp32s3/user-guide-devkitm-1.html) + [2Ch Relay Board](https://aliexpress.com/item/1005006402415669.html)
  * Wire as shown in *Wiring* section below
  * Use `ESP32-S3` as target device.
* [ElectroDragon Wifi IoT SPDT Relay Board V2](https://www.electrodragon.com/product/wifi-iot-relay-board-spdt-based-esp8266/)
  * Assign pins to the relay channels according to [ElectroDragon specs](https://w2.electrodragon.com/Board-dat/NWI/NWI1119-DAT/NWI1119-DAT.md): `{6, 7}`
  * You may use GPIO05 as sensor (located on the board)
  * Use `ESP32-C3` as target device.

## Wiring
No need to do any wiring if you're using fully built ESP32-based relay board. It's just already done so you can skip this block.

However, if you're using a custom built combination of ESP32 board and the relay module, then you can wire them as following (example for 2 relay module):
```
+------------+-----------+
| Module Pin | ESP32 Pin |
+------------+-----------+
| GND        | GND       |
+------------+-----------+
| VCC        | +5V       |
+------------+-----------+
| IN1        | IO04      |
+------------+-----------+
| IN2        | IO05      |
+------------+-----------+
```
This is just a default (sample) wiring and do not worry if you'd like to use other pins: you can configure them via WEB interface
> [!NOTE]
> But remember one thing: not every pin from ESP32 board/module can be used to control relays in this project, because some of those are reserved for system purposes and can make the device very unstable. Make sure that pins you'd like to use are in this list:
```
{4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 39}
```

## Building and Flashing
1. **Setup the ESP-IDF Environment**:
   - Follow the official [ESP-IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#ide) to configure your development environment. The installation approach will be different depending on the OS you use.
   - Further on we will assume, that you're using Unix-based OS (Linux, macOS) and you've installed ESP-IDF into `$HOME/esp/esp-idf` folder. Note, that the project has been tested to be built under `Windows`, but the setup process and the commands might differ slightly.
2. **Clone this Repository**:
   ```bash
   git clone https://github.com/rpavlyuk/ESPRelayBoard
   cd ESPRelayBoard
   ```
3. **Initiate ESP-IDF**:
  This will map the source folder to ESP-IDF framework and will enable all needed global variables (macOS, Linux).
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```
   **OR** For Windows, start `ESP IDF vX.X Powershell` from Programs menu and change directory to project's root. E.g.:
   ```
   cd D:\Work\esp\projects\ESPRelayBoard
   ```
4. **Prepare the configuration**:
   The project already contains some pre-built SDK configuration files (```sdkconfig```) depending on the board type. You will find them in the project's root as ```sdkconfig.*```. Just copy the one which corresponds to you board, for example:
   ```bash
	cp -a sdkconfig.esp32-s3 sdkconfig
   ```
   See more details on how to play with ```sdkconfig``` in the section **Flash the Firmware** below.
4. **Build the firmware**:
   ```bash
   idf.py build
   ```
   You will get `idf.py: command not found` if you didn't initiate ESP-IDF in the previous step.
   You may also get build errors if you've selected other board type then `ESP32-C6`, `ESP32` or `ESP32-S3`.
5. **Determine the port**:
   A connected ESP32 device is opening USB-2-Serial interface, which your system might see as `/dev/tty.usbmodem1101` (macos example) or `/dev/ttyUSB0` (Linux example). Use `ls -al /dev` command to see the exact one.
   Also, please, refer [this article](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/establish-serial-connection.html) to get much more comprehensive help and instructions on how to determine serial port depending on your OS.
6. *(recommended for the initial setup of fresh ESP32 module)* **Erase the Flash**
   Remove all data from the ESP32 board (incl. WiFi connection settings and NVS storage).
   ```
   idf.py -p /dev/ttyUSB0 erase-flash
   ```
    Replace `/dev/ttyUSB0` with the appropriate port for your system.
7. **Flash the Firmware**:
   ```bash
   idf.py -p /dev/ttyUSB0 build flash monitor
   ```
   You will get the device console log as the result of the command.

> [!NOTE]
> Every time you change the board type (e.g., from ESP32-C6 to ESP32-S3, the framework is re-creating `sdkconfig` file which makes some very important settings gone. Thus, you (might) need to change/set some settings **if you've changed the device board**:
* Open SDK settings menu by running:
  ```bash
  idf.py menuconfig
  ```
* Set the following parameters:
  * *Partition table -> Custom partition table CSV* as the option
  * *Partition table -> Custom partition CSV file* set to `partition_table/partition.csv`
  * *Serial flasher config -> Flash size* to 4Mb or 8Mb (depends on the board you use)
  * *Serial flasher config -> Detect flash size when flashing bootloader* to enabled
  * *Component config -> HTTP Server -> Max HTTP Request Header Length* to `8192`
  * *Component config → ESP System Settings -> Event loop task stack size* to `4096`
  * *Component config → ESP System Settings -> Main task stack size* to `4096`
  * *Component config → ESP System Settings -> Minimal allowed size for shared stack* to `2048`
* **Alternatively**, you can use some of the existing configurations that I've prepared for some of most popular ESP32 target devices. Those are `sdkconfig.<device>` files in project root. You simply copy them as `sdkconfig` and that will load all needed settings, no need to go to `menuconfig` as described in the section above. For example, to enable `ESP32-S3` as device, you can:
```bash
cp -a sdkconfig.esp32-s3 sdkconfig
```
> [!NOTE]
> This will overwrite any settings you've made via `idf.py menuconfig`
* You will need to do a full re-build after you either make changes via `menuconfig` or by copying `sdkconfig.*` file:
```bash
idf.py fullclean build
```
## Updating the Firmware
If you already have ESP32 device flashed with previous version of `ESPRelayBoard` and you want to update it to the latest one -- just follow those simple steps (assuming that ESP-IDF is already configured and initiated as stated in section above).
* Pull the latest version from Github:
```
git pull
```
* Cleanup the code (recommended):
```
idf.py fullclean
```
* Flash the new version (assuming your device is accessible as `/dev/ttyUSB0`):
```
idf.py -p /dev/ttyUSB0 build flash monitor
```
Your settings will be preserved after the update. However, there's a possibility that those settings will become incompatible due to the significant changes in the code. In that case (if you see that device is struggling to boot) you should do `idf.py erase-flash` as described in the section above.

It is recommeded however to use [OTA Firmware Update](#ota-firmware-update) after version 1.4 and higher.

## Initiation
### First boot
Right upon initial flashing (or after flash was erased) the device will boot in Wi-Fi setup mode.
### WiFi Setup
* On the first boot (or after flash erase), the WiFi will start in access point mode with the SSID `PROV_AP_XXXXXX`. The exact named will be different depending on the device hardware ID (which is built in). The password is SSID name plus `1234`. For example, `PROV_AP_XXXXXX1234`
* Use the *ESP SoftAP Prov* ([iOS](https://apps.apple.com/us/app/esp-softap-provisioning/id1474040630), [Android](https://play.google.com/store/apps/details?id=com.espressif.provsoftap&hl=en)) mobile app to connect to the device and configure WiFi settings. Read more [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/provisioning.html#provisioning-tools) if you want to know more about the SoftAP provisioning.

### Device Setup
* Device will initiate itself with default settings once the WiFi was provisioned. All further configuration, including **relays / units management**, will/can be made via WEB interface.
> [!NOTE]
> Currently only DHCP mode is supported.

## WEB Setup
* Device starts the WEB interfance available as `http://<WIFI-IP>/`. The device implements no authorization (yet) so be sure to use it in safe environment.
* You can start controlling the relays by visiting **Status** page (assuming you connected them to default pins).
* Basic configuration parameters on **Configuration** page:
  * `MQTT Mode`: defines how MQTT works
    - `Disabled`: MQTT is disabled
    - `Connect Once`: MQTT is enabled. Connection will be established, but no reconnect attempts will be made once it is dropped.
    - `Auto-Connect`: MQTT is enabled. Connection will operate in keep-alive mode, reconnecting on failures.
  * `MQTT Server`, `MQTT Port`, `MQTT Protocol`, `MQTT User`, `MQTT Password`: MQTT connection string parameters.
  * `MQTT Prefix`: top level path in the MQTT tree. The path will look like: `<MQTT_prefix>/<device_id>/...`
  * `HomeAssistant Device integration MQTT Prefix`: HomeAssistant MQTT device auto-discovery prefix. Usually, it is set to `homeassistant`
  * `HomeAssistant Device update interval (ms)`: how often to update device definitions at HomeAssistant.
* System Update:
  * `OTA Update URL`: A URL pointing to `.bin` file with the firmware which you want to update the system to. See section *OTA Firmware Update* below for details. The UI client will also try to check if there's new version at the provided URL but looking for `build_info.json` file in the same directory as firmware file.
  * `OTA Update Reset Config`: Reset device configuration (except Wi-Fi) once OTA is performed. Useful
* Relay Parameters:
  * `Channels count (actuators)`: number of relays (actuators) you'd like to control (or your board has)
  * `Contact sensors count`: number of contact sensors you'd like to activate  and monitor
  * `Refresh interval (ms)`: relay/sensor reading update interval. Used in WEB interface.
* Network logging parameters:
  * `Logging Type`: what logging mechaism to use.
    * `Disabled`: Default setting. No network logging is enabled.
    * `UDP`, `TCP`: The device supports remote logging using the Syslog protocol (RFC 3164 / RFC 5424) over UDP or TCP (recommended). You may capture the logs using [RSyslog](https://www.rsyslog.com) or other compatible tools like [Graylog](https://graylog.org). **NOTE:** SSL/TLS encryption is not supported, so ensure safe environment when sending logs outside the secure perimeter.
    * `MQTT`: Logging to a specified MQTT topic. *Currently not implemented*
  * `Logging Host`: IP or hostname to send logs to. Must be within the same LAN when using UDP broadcast address.
  * `Logging Port`: UDP/TCP port on the target host.
  * `Keep Stdout Logging`: keep sending messages to STDOUT (console) even when network logging is enabled.
* Memguard parameters:
  * `Memory Guard Mode`: Either disable memguard at all OR set corresponding system reaction: warning only or full reboot.
  * `Memory Guard Threshold (bytes)`: Free heap memory in bytes which is considered as too low, so the action must be taken. Default is 65535 (64k). Setting the threashhold too high (90k+) may put the device in endless reboot loop. However, there's a prevention machanism: `memguard` actions are only taken 3 minutes since boot, so you have time to change the threashhold or disable `memguard` at all.
* `CA / Root Certificates`: PEM formatted SSL root/intermediate certificate. Used by MQTTS connection and HTTPS firmware OTA update. You can separately configure the certificate for MQTT and for OTA update via HTTPS.

Changing number of sensors or relays requires a restart of the device. Use button `Reboot Device` at the bottom of configuration page.

## Units Configuration
All units (relays and sensors) can be configured on **Relays** page.

All units have the following properties:
* `Channel`*(read-only)*: An id of the control channel the relay or sensor is associated to. Relays and sensors have separate channel group.
* `GPIO Pin`: GPIO pin that is controlling the relay (for actuator) or for which the contact state is monitored (sensor).
* `Inverted`: Inverting the state of control or sensing signals correspondngly.
* `Enabled`*(reserved, currently not in use and is ignored)*: Unit is enabled.

Setting values are being saved by `Update` button for each corresponding unit. Pin number be within the so called *safe list* and should not overlap with other pins already in use.

## Testing the Setup
### Relay / Actuator
* Connect the module to corresponding GPIO pin (if using an external module) or configure the `GPIO Pin` for corresping relay channel.
* Use page **Status** to change the relay state, observe the relay changing its state
* See console log output if any issues occur.
### [Dry Contact Sensor](https://www.electrical4u.com/dry-contacts/)
* Add at least one contact sensor on **Configuration** page and reboot the device
* Note a `GPIO Pin` for the contact sensor you'd like to test OR change it to the one you plan to use
* Connect `GND` to the mentioned above pin
* Observe the change of the state on **Status** page for corresponding contact sensor
* See console log output if any issues occur

Consider using state inversion (`Inverted` parameter) for both sensors and relay if needed.

## Home Assistant Integration
The device will automatically enable itself in Home Assistant if:
* both HA and the device are connected to the same MQTT server
* `HomeAssistant Device integration MQTT Prefix` is set correctly
* auto-discovery is enable in Home Assistant (should be enabled by default)

You will see device shown as `<device_id>` (e.g., *DAF3124C798E* by *ESP Relay Board*) in the device list as soon as HA picks the auto-discovery records up.

## OTA Firmware Update
The device allows updating the firmware from a provided URL pointing to the firmware image. The file is generate once you run a successful build using `idf.py build` command and is placed in `./build/` folder as `ESPRelayBoard.bin`. 

You can either point `OTA Update URL` to one of the `.bin` files placed in *Releases* section at Github or you may have your own update location. Typically, the update flow in this case will look like this:
* Run a build:
```
idf.py build
```
* Place the file `./build/ESPRelayBoard.bin` to a WEB server that is accessible by the device. If you use HTTPS then make sure you've set correct `CA / Root Certificate` at `HTTPS` tab.
* (optional) Place file `./build/storage.bin` in the same WEB server directory as the firmware one. This will update SPIFFS where templates and other files are stored.
* Adjust / provide `OTA Update URL` if needed.
* It is recommended to set `OTA Update Reset Config` to `Enable` if you're doing major version upgrade.
* Click `Update Device` button on WEB UI. Device will update itself (takes 2-5 mins depending on the connection speed) and reboot with new hardware.
* The device will revert to the previous (running) firmware if the OTA process crashes or is not complete successfully.
> [!NOTE]
> OTA firmware update is generally quite robust. If you flash your device with incompatible firmware or use wrong URL and thus incidentally "brick" it, no problem -- just re-flash it using USB dongle as was described in section *Building and Flashing* above. You might need to `erase-flash` and reconfigure the device again.

## WEB API
The device is exposing a simple JSON API to control relays and get units information..
1. **Get all units (relays and sensors):**
 * Endpoint: `/api/relays`
 * Method: GET
 * Request payload (example): N/A
 * Response payload (example):
 ```
{
	"data":	[{
			"relay_key":	"relay_ch_0",
			"channel":	0,
			"state":	false,
			"inverted":	true,
			"gpio_pin":	4,
			"enabled":	true,
			"type":	0
		}, {
			"relay_key":	"relay_ch_1",
			"channel":	1,
			"state":	false,
			"inverted":	true,
			"gpio_pin":	5,
			"enabled":	true,
			"type":	0
		}, {
			"relay_key":	"relay_sn_0",
			"channel":	0,
			"state":	false,
			"inverted":	true,
			"gpio_pin":	12,
			"enabled":	true,
			"type":	1
		}, {
			"relay_key":	"relay_sn_1",
			"channel":	1,
			"state":	true,
			"inverted":	true,
			"gpio_pin":	13,
			"enabled":	true,
			"type":	1
		}],
	"status":	{
		"count":	4,
		"code":	0,
		"text":	"ok"
	}
}
 ```
2. **Update unit information:**
 * Endpoint: `/api/relay/update`
 * Method: POST
 * Request payload (example):
 ```
 {"data":{"device_serial":"0O0RSJ3Q2XF03F8U2Z4CVLWUAFOOQ0TO","relay_key":"relay_ch_0","relay_channel":0,"relay_gpio_pin":4,"relay_enabled":true,"relay_inverted":true}}
 ```
   Required parameters: `device_serial`, `relay_key`
 * Response payload (example):
 ```
 {
	"data":	{
	"relay_key":	"relay_ch_0",
	"channel":	0,
	"state":	false,
	"inverted":	true,
	"gpio_pin":	4,
	"enabled":	true,
	"type":	0
},
	"status":	{
		"error":	"OK",
		"code":	0
	}
}
 ```
3. **Get device status:**
 * Endpoint: `/api/status`
 * Method: GET
 * Request payload (example): N/A
 * Response payload (example):
 ```
{
	"status":	{
		"free_heap":	206328,
		"min_free_heap":	144852,
		"time_since_boot":	6127315474
	}
}
 ```
4. **Get device settings (all):**
 * Endpoint: `/api/setting/get/all`
 * Method: GET
 * Request params (GET):
   * `device_id`
   * `device_serial`
 * Request payload (example): N/A
 * Response payload (example):
 ```
{
    "data": {
        "ota_update_url": {
            "type": 2,
            "max_size": 256,
            "value": "http://localhost:8080/ota/relayboard.bin",
            "size": 41
        },
        "ha_upd_intervl": {
            "type": 0,
            "max_size": 4,
            "value": 60000,
            "size": 4
        },
        "mqtt_connect": {
            "type": 1,
            "max_size": 2,
            "value": 2,
            "size": 2
        },
        "mqtt_server": {
            "type": 2,
            "max_size": 128,
            "value": "192.168.1.2",
            "size": 12
        },
        "mqtt_port": {
            "type": 1,
            "max_size": 2,
            "value": 1883,
            "size": 2
        },
        "mqtt_protocol": {
            "type": 2,
            "max_size": 10,
            "value": "mqtt",
            "size": 5
        },
        "mqtt_user": {
            "type": 2,
            "max_size": 64,
            "value": "",
            "size": 1
        },
        "mqtt_password": {
            "type": 2,
            "max_size": 64,
            "value": "",
            "size": 1
        },
        "mqtt_prefix": {
            "type": 2,
            "max_size": 128,
            "value": "relay_board",
            "size": 12
        },
        "ha_prefix": {
            "type": 2,
            "max_size": 128,
            "value": "homeassistant",
            "size": 14
        },
        "relay_refr_int": {
            "type": 1,
            "max_size": 2,
            "value": 1000,
            "size": 2
        },
        "relay_ch_count": {
            "type": 1,
            "max_size": 2,
            "value": 2,
            "size": 2
        },
        "relay_sn_count": {
            "type": 1,
            "max_size": 2,
            "value": 1,
            "size": 2
        },
        "net_log_type": {
            "type": 1,
            "max_size": 2,
            "value": 2,
            "size": 2
        },
        "net_log_host": {
            "type": 2,
            "max_size": 256,
            "value": "127.0.0.1",
            "size": 12
        },
        "net_log_port": {
            "type": 1,
            "max_size": 2,
            "value": 5114,
            "size": 2
        },
        "net_log_stdout": {
            "type": 1,
            "max_size": 2,
            "value": 1,
            "size": 2
        }
    },
    "total": 17
}
 ```
5. **Get device settings (selected one):**
 * Endpoint: `/api/setting/get`
 * Method: GET
 * Request params (GET):
   * `device_id`
   * `device_serial`
   * `key`: Setting key (see `main/settings.h` for keys reference)
 * Request payload (example): N/A
 * Response payload (example):
 ```
 {
  "mqtt_server": {
      "type": 2,
      "max_size": 128,
      "value": "192.168.1.5",
      "size": 12
  }
}
 ```
 6. **Set device setting(s):**
 * Endpoint: `/api/setting/update`
 * Method: POST
 * Request payload (example):
 ```
{
    "device_id": "9XXE6E0MMC5C",
    "device_serial": "VU7303USWVEP6ENQ3POTTFHVV7JH97QX",
    "data": {
        "mqtt_server":        "192.168.1.5",
        "mqtt_port":        1883
    },
    "action": 0
}
 ```
 * Response payload (example):
 ```
{
    "status": {
        "success": 2,
        "failed": 0,
        "total": 2
    },
    "details": {
        "mqtt_server": {
            "old_value": "192.168.1.2",
            "new_value": "192.168.1.5",
            "status": 0,
            "error_msg": "Updated setting 'mqtt_server' (was 192.168.1.2)"
        },
        "mqtt_port": {
            "old_value": "8883",
            "new_value": "1883",
            "status": 0,
            "error_msg": "Updated setting 'mqtt_port' (was 8883)"
        }
    }
}
 ```
7. **Forcing device reboot via API:**
 * Endpoint: `/api/setting/update`
 * Method: POST
 * Request payload (example):
 ```
{
    "device_id": "9XXE6E0MMC5C",
    "device_serial": "VU7303USWVEP6ENQ3POTTFHVV7JH97QX",
    "data": {
    },
    "action": 2
}
 ```
 * Response payload (example):
 ```
 {
    "status": {
        "success": 0,
        "failed": 0,
        "total": 0
    },
    "details": {}
}
 ```

## Known issues, problems and TODOs:
* Static IP support needed
* Device may have memory leaks when used very intensively (to be improved)


## License and Credits
* GPLv3 -- you're free to use and modify the code or its parts, and thus make your over better ESP32 relay board :)
* Uses:
  * [ESP32_NVS](https://github.com/VPavlusha/ESP32_NVS) library by [VPavlusha](https://github.com/VPavlusha)🇺🇦
  * [esp-idf-net-logging](https://github.com/nopnop2002/esp-idf-net-logging) library by [nopnop2002](https://github.com/nopnop2002)🇯🇵
* Consider putting a star if you like the project
* Find me at roman.pavlyuk@gmail.com
