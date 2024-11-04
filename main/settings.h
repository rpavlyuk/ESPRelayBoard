#ifndef SETTINGS_H
#define SETTINGS_H

#include "common.h"
#include "mqtt.h"


/**
 * General variable settings
 */
#define DEVICE_SERIAL_LENGTH    32
#define DEVICE_ID_LENGTH        12
#define MQTT_SERVER_LENGTH       128
#define MQTT_PROTOCOL_LENGTH     10
#define MQTT_USER_LENGTH         64
#define MQTT_PASSWORD_LENGTH     64
#define MQTT_PREFIX_LENGTH       128
#define HA_PREFIX_LENGTH         128
#define CA_CERT_TYPE_LENGTH      6

#define HA_UPDATE_INTERVAL_MIN  60000           // Once a minute
#define HA_UPDATE_INTERVAL_MAX  86400000        // Once a day (24 hr)

#define CHANNEL_COUNT_MIN  0
#define CHANNEL_COUNT_MAX  15

#define CONTACT_SENSORS_COUNT_MIN  0
#define CONTACT_SENSORS_COUNT_MAX  8

#define RELAY_REFRESH_INTERVAL_MIN  1
#define RELAY_REFRESH_INTERVAL_MAX  10000

#define RELAY_GPIO_PIN_MIN  0
#define RELAY_GPIO_PIN_MAX  39

#define OTA_UPDATE_URL_LENGTH  256

/**
 * General constants
 */
#define S_NAMESPACE           "settings"
#define S_DEVICE_FAMILY       "switch"
#define WIFI_NAMESPACE        "nvs.net80211"

/**
 * Settings keys
 * 
 * NOTE: MAX key size is 15 characters
 * 
 * DO NOT CHANGE, unless you definitelly know why you need this
 */


#define S_KEY_DEVICE_ID         "device_id"
#define S_KEY_DEVICE_SERIAL     "device_serial"

#define S_KEY_MQTT_CONNECT      "mqtt_connect"       
#define S_KEY_MQTT_SERVER       "mqtt_server"
#define S_KEY_MQTT_PORT         "mqtt_port"
#define S_KEY_MQTT_PROTOCOL     "mqtt_protocol"
#define S_KEY_MQTT_USER         "mqtt_user"
#define S_KEY_MQTT_PASSWORD     "mqtt_password"
#define S_KEY_MQTT_PREFIX       "mqtt_prefix"

#define S_KEY_HA_PREFIX                 "ha_prefix"
#define S_KEY_HA_UPDATE_INTERVAL        "ha_upd_intervl"

#define S_KEY_CH_PREFIX                 "relay_ch_"
#define S_KEY_SN_PREFIX                 "relay_sn_"
#define S_KEY_CHANNEL_COUNT             "relay_ch_count"
#define S_KEY_CONTACT_SENSORS_COUNT     "relay_sn_count"
#define S_KEY_RELAY_REFRESH_INTERVAL    "relay_refr_int"

#define S_KEY_OTA_UPDATE_URL            "ota_update_url"

/**
 * Settings default values
 */
#define S_DEFAULT_DEVICE_ID         ""
#define S_DEFAULT_DEVICE_SERIAL     ""

#define S_DEFAULT_MQTT_CONNECT      MQTT_CONN_MODE_DISABLE
#define S_DEFAULT_MQTT_SERVER       "127.0.0.1"
#define S_DEFAULT_MQTT_PORT         1883
#define S_DEFAULT_MQTT_PROTOCOL     "mqtt"
#define S_DEFAULT_MQTT_USER         ""
#define S_DEFAULT_MQTT_PASSWORD     ""
#define S_DEFAULT_MQTT_PREFIX       "relay_board"

#define S_DEFAULT_HA_PREFIX             "homeassistant"
#define S_DEFAULT_HA_UPDATE_INTERVAL    600000              // Update Home Assistant definitions every 10 minutes

#define S_DEFAULT_RELAY_GPIO_PIN                 4  /* Must be within SAFE_GPIO_PINS[] (relay.c) list*/

#define S_DEFAULT_CHANNEL_COUNT                  2
#define S_DEFAULT_CONTACT_SENSORS_COUNT          0
#define S_DEFAULT_RELAY_REFRESH_INTERVAL         1000       // ms

#define S_DEFAULT_OTA_UPDATE_URL                 "http://localhost:8080/ota/relayboard.bin"

/**
 * Paths
 */

#define CA_CERT_PATH_MQTTS  "/spiffs/ca-mqtts.crt"
#define CA_CERT_PATH_HTTPS  "/spiffs/ca-https.crt"

/** 
 * Initialize types
 */

/* Structure to pass the OTA URL to the task */
typedef struct {
    char ota_url[OTA_UPDATE_URL_LENGTH];  // Adjust the size if needed
} ota_update_param_t;

/**
 * Initialization variables
 */
extern int device_ready;

/**
 * Routines
 */ 

/**
 * @brief Initialize settings:
 *  - fill-in missing settings with default values
 * 
 */
esp_err_t settings_init();

/**
 * @brief: Initialize basic (platform) settings for the device
 */
esp_err_t base_settings_init();

/**
 * @brief: Device specific settings init
 */
esp_err_t device_settings_init();

/**
 * @brief Generate serial number for the device
 * 
 */
void generate_serial_number(char *serial_number);

/**
 * @brief: Init the filesystem / partition
 */
void init_filesystem();

/**
 * @brief: Call OTA update
 */
esp_err_t perform_ota_update(const char *url);

/**
 * @brief: Check if the OTA partitions are valid
 */
esp_err_t check_ota_partitions(void);

/**
 * @brief: OTA update task
 */
void ota_update_task(void *param);

/**
 * @brief: Reset factory settings
 */
esp_err_t reset_factory_settings();

/**
 * @brief: Reset device settings
 */
esp_err_t reset_device_settings();

/**
 * @brief: Reset WiFi settings
 */
esp_err_t reset_wifi_settings();

#endif