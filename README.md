[![Stand With Ukraine](https://raw.githubusercontent.com/vshymanskyy/StandWithUkraine/main/badges/StandWithUkraine.svg)](https://stand-with-ukraine.pp.ua)
[![Made in Ukraine](https://img.shields.io/badge/Made_in-Ukraine-ffd700.svg?labelColor=0057b7)](https://stand-with-ukraine.pp.ua)

# ESP32 Relay Board Firmware

## Purpose
This project provides firmware for an ESP32-based relay boards which you can find in great number in Aliexpress, Amazon or Botland. Alternatively, you can use this firmware if you want to contol any kind of relay's (typically those from SRD-**VDC-SL-C family) or just any digital electronics component that is controlled by HIGH/LOW logical levels.

The device supports two types of units:
* **Relay**: The board controls an actuator which is controlled by HIGH/LOW logical states
* **Contact Sensor**: The board monitors contact state between selected GPIO(s) and GND, providing information either contact is closed or open

## Table of Contents
1. [Purpose](#purpose)
2. [Features](#features)
3. [Prerequisites: What you need to start](#prerequisites)
4. [Tested Relay Boards / Configurations](#tested-relay-boards--configurations)
5. [Wiring Sample](#wiring)
6. [Getting started: Building and Flashing](#building-and-flashing)
7. [Initiation: Making device to work](#initiation)
	- [First boot](#first-boot)
	- [WiFi Setup](#wifi-setup)
	- [Device Setup](#device-setup)
8. [WEB Setup](#web-setup)
9. [Units (Relays) Configuration](#units-configuration)
10. [Testing the Setup](#testing-the-setup)
	 - [Relay / Actuator](#relay--actuator)
	 - [Contact Sensor](#contact-sensor)
11. [Home Assistant Integration](#home-assistant-integration)
12. [OTA Firmware Update](#ota-firmware-update)
13. [WEB API](#web-api)
14. [Known issues, problems and TODOs](#known-issues-problems-and-todos)
15. [License and Credits](#license-and-credits)

## Features
- **WiFi Connectivity**: Supports connecting to WiFi for remote monitoring and control.
- **Soft Configuration**: GPIO pins that relays are connected to and other settings are defined via user interface and are stored in NVS (EEPROM).
- **Relay State Memory**: Relay (actuators) states are written to NVS (EEPROM) memory and will be restored after power loss.
- **MQTT Support**: Publishes relay and contact sensors states via MQTT for further integration with various platforms.
- **Home Assistant Integration**: Automatically detects and configures devices in Home Assistant using auto-discovery via MQTT.
- **Web Interface**: Provides a web-based user interface for configuration, control and monitoring.
- **Web API**: Simple JSON API is in place should you want to integrate the device into your custom infractucture projects.
- **OTA (over the air) Firmware Update**: Trigger firmware update via WEB interface from a provided URL.

## Prerequisites
To get started, you will need:
- **Hardware**:
  - ESP32-C6 or ESP32-S3 microcontroller (both have been tested) with at least 4Mb flash.
  - Relay. E.g., SRD-05VDC-SL-C-based mechanical relay module.
  - **OR** ESP32-based relay board with 4Mb flash.
- **Software**:
  - ESP-IDF framework (version 4.4 or higher recommended installed and configured).
  - Operating system: `Linux` (tested, recommended), `macOS` (tested, recommended), `Windows` (tested)

## Tested Relay Boards / Configurations
* [LilyGO 4 Ch T-Relay](https://www.lilygo.cc/products/t-relay)
  * Assign pins to the relay channels according to LilyGO specs: `{21, 19, 18, 05}`
* [ESP32_Relay x2](https://aliexpress.com/item/1005005926554704.html) from AliExpress
  * Use pins: 16 (channel 0) and 17 (channel 1). No inversion.
* [ESP32-S3-DevKitM-1](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/hw-reference/esp32s3/user-guide-devkitm-1.html) + [2Ch Relay Board](https://aliexpress.com/item/1005006402415669.html)
  * Wire as shown in *Wiring* section below


## Wiring
No need to do any wiring if you're using fully built ESP32-based relay board. It's just already done :)

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
   - Further on we will assume, that you're using Unix-based OS (Linux, macOS) and you've installed ESP-IDF into `$HOME/esp-idf` folder. Note, that the project has been tested to be built under `Windows`, but the setup process and the commands might differ slightly.
2. **Clone this Repository**:
   ```bash
   git clone https://github.com/rpavlyuk/ESPRelayBoard
   cd ESPRelayBoard
   ```
3. **Initiate ESP-IDF**:
  This will map the source folder to ESP-IDF framework and will enable all needed global variables (macOS, Linux).
   ```bash
   . $HOME/esp-idf/export.sh
   ```
   **OR** For Windows, start `ESP IDF vX.X Powershell` from Programs menu and change directory to project's root. E.g.:
   ```
   cd D:\Work\esp\projects\ESPRelayBoard
   ```
4. **Build the firmware**:
   ```bash
   idf.py build
   ```
   You will get `idf.py: command not found` if you didn't initiate ESP-IDF in the previous step.
   You may also get build errors if you've selected other board type then `ESP32-C6` or `ESP32-S3`.
5. **Determine the port**:
   A connected ESP32 device is opening USB-2-Serial interface, which your system might see as `/dev/tty.usbmodem1101` (macos example) or `/dev/ttyUSB0` (Linux example). Use `ls -al /dev` command to see the exact one.
   Also, please, refer [this article](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/establish-serial-connection.html) to get much more comprehensive help and instructions on how to determine serial port depending on your OS.
6. *(recommended)* **Erase the Flash**
   Remove all data from the ESP32 board (incl. WiFi connection settings and NVS storage).
   ```
   idf.py -p /dev/ttyUSB0 erase-flash
   ```
    Replace `/dev/ttyUSB0` with the appropriate port for your system.
7. **Flash the Firmware**:
   ```bash
   idf.py -p /dev/ttyUSB0 build flash monitor
   ```

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
* Alternatively, you can use some of the existing configurations that I've prepared for some of ESP32 devices. Those are `sdkconfig.<device>` files in project root. You copy them as `sdkconfig` and that will load all needed settings already. For example, to enable `ESP32-S3` as device, you can:
```
cp -a sdkconfig.esp32-s3 sdkconfig
```
> [!NOTE]
> This will overwrite any settings you've made via `idf.py menuconfig`

## Initiation
### First boot
Right upon flashing (after flash was erased) the device will boot in Wi-Fi setup mode.
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
  * `OTA Update URL`: A URL pointing to `.bin` file with the firmware which you want to update the system to. See section *OTA Firmware Update* below for details.
* Relay Parameters:
  * `Channels count (actuators)`: number of relays (actuators) you'd like to control (or your board has)
  * `Contact sensors count`: number of contact sensors you'd like to activate  and monitor
  * `Refresh interval (ms)`: relay/sensor reading update interval. Used in WEB interface.
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
### Contact Sensor
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
> [!NOTE]
> CA certificates are stored in SPIFFS, so you might need to update them after the Storage OTA update is complete.
* Adjust / provide `OTA Update URL` if needed.
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


## Known issues, problems and TODOs:
* Static IP support needed
* Device may have memory leaks when used very intensively (to be improved)


## License and Credits
* GPLv3 -- you're free to use and modify the code, and make your over better ESP32 relay board :)
* Uses [ESP32_NVS](https://github.com/VPavlusha/ESP32_NVS) library by [VPavlusha](https://github.com/VPavlusha)
* Consider putting a star if you like the project
* Find me at roman.pavlyuk@gmail.com
