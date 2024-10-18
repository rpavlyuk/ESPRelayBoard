[![Stand With Ukraine](https://raw.githubusercontent.com/vshymanskyy/StandWithUkraine/main/badges/StandWithUkraine.svg)](https://stand-with-ukraine.pp.ua)
[![Made in Ukraine](https://img.shields.io/badge/Made_in-Ukraine-ffd700.svg?labelColor=0057b7)](https://stand-with-ukraine.pp.ua)

# ESP32 Pressure Sensor Firmware

## Purpose
This project provides firmware for an ESP32-based relay boards which you can find in great number in Aliexpress, Amazon or Botland. Alternatively, you can use this firmware if you want to contol any kind of relay's (typically those from SRD-**VDC-SL-C family) or just any digital electronics component that is controlled by HIGH/LOW logical levels.

The device supports two types of units:
* **Relay**: The board controls an actuator which is controlled by HIGH/LOW logical states
* **Contact Sensor**: The board monitors contact state between selected GPIO(s) and GND, providing information either contact is closed or open

### Features
- **WiFi Connectivity**: Supports connecting to WiFi for remote monitoring and control.
- **MQTT Support**: Publishes relay and contact sensors states via MQTT for further integration with various platforms.
- **Home Assistant Integration**: Automatically detects and configures devices in Home Assistant using auto-discovery via MQTT.
- **Web Interface**: Provides a web-based user interface for configuration, control and monitoring.

## Prerequisites
To get started, you will need:
- **Hardware**:
  - ESP32-C6 or ESP32-S3 microcontroller (both have been tested).
  - Relay. E.g., SRD-05VDC-SL-C-based mechanical relay module.
  - **OR** ESP32-based relay board.
- **Software**:
  - ESP-IDF framework (version 4.4 or higher recommended installed and configured).

## Wiring
No need to do any wiring if you're using fully built ESP32-based relay board.

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
This is a default (sample) wiring and do not worry if you'd like to use other pins: you can configure them via WEB interface
> [!NOTE]
> But remember one thing: not every pin from ESP32 board/module can be used to control relays in this project, because some of those are reserved for system purposes and can make the device very unstable. Make sure that pins you'd like to use are in this list:
```
{4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 26, 27, 28, 29, 30, 31}
```

## Building and Flashing
1. **Setup the ESP-IDF Environment**:
   - Follow the official [ESP-IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to configure your development environment.
   - Further on we will assume, that you're using Unix-based OS (Linux, macos) and you've installed ESP-IDF into `$HOME/esp-idf` folder.
2. **Clone this Repository**:
   ```bash
   git clone https://github.com/rpavlyuk/ESPRelayBoard
   cd ESPRelayBoard
   ```
3. **Initiate ESP-IDF**:
  This will map the source folder to ESP-IDF framework and will enable all needed global variables.
   ```bash
   . $HOME/esp-idf/export.sh
   ```
4. **Build the firmware**:
   ```bash
   idf.py build
   ```
   You will get `idf.py: command not found` if you didn't initiate ESP-IDF in the previous step.
   You may also get build errors if you've selected other board type then `ESP32-C6` or `ESP32-S3`.
5. **Determine the port**:
   A connected ESP32 device is opening USB-2-Serial interface, which your system might see as `/dev/tty.usbmodem1101` (macos example) or `/dev/ttyUSB0` (Linux example). Use `ls -al /dev` command to see the exact one.
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


## Initiation
### First boot
Right upon flashing (after flash was erased) the device will boot in Wi-Fi setup mode.
### WiFi Setup
* On the first boot (or after flash erase), the WiFi will start in access point mode with the SSID `PROV_AP_XXXXXX`. The exact named will be different depending on the device hardware ID (which is built in). The password is SSID name plus `1234`. For example, `PROV_AP_XXXXXX1234`
* Use the *ESP SoftAP Prov* ([iOS](https://apps.apple.com/us/app/esp-softap-provisioning/id1474040630), [Android](https://play.google.com/store/apps/details?id=com.espressif.provsoftap&hl=en)) mobile app to connect to the device and configure WiFi settings. Read more [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/provisioning.html#provisioning-tools) if you want to know more about the SoftAP provisioning.

### Device Setup
* Device will initiate itself with default settings once the WiFi was provisioned. All further configuration, including **sensor calibration**, will/can be made via WEB interface.
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
  * `MQTT Server`, `MQTT Port`, `MQTT Protocol`, `MQTT User`, `MQTT Password`: MQTT connection string parameters. Protocol `mqtts` is supported but CA/root certificate management UI has not been implemented yet.
  * `MQTT Prefix`: top level path in the MQTT tree. The path will look like: `<MQTT_prefix>/<device_id>/...`
  * `HomeAssistant Device integration MQTT Prefix`: HomeAssistant MQTT device auto-discovery prefix. Usually, it is set to `homeassistant`
  * `HomeAssistant Device update interval (ms)`: how often to update device definitions at HomeAssistant.
* Relay Parameters:
  * `Channels count (actuators)`: number of relays (actuators) you'd like to control (or your board has)
  * `Contact sensors count`: number of contact sensors you'd like to activate  and monitor
  * `Refresh interval (ms)`: relay/sensor reading update interval. Used in WEB interface.

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

## WEB API
The device is exposing a simple read-only API URL to get the device status and sensor data.
1. Get all units (relays and sensors):
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
2. Update unit information:
 * Endpoint: `/api/relay/update`
 * Method: POST
 * Request payload (example):
 ```
 {"data":{"device_serial":"0O0RSJ3Q2XF03F8U2Z4CVLWUAFOOQ0TO","relay_key":"relay_ch_0","relay_channel":0,"relay_gpio_pin":4,"relay_enabled":true,"relay_inverted":true}}
 ```
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
3. Get device status:
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
