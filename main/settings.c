#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_spiffs.h"  // Include for SPIFFS
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_http_client.h"

#include "esp_ota_ops.h"
#include "esp_https_ota.h"

#include "settings.h"
#include "non_volatile_storage.h"
#include "relay.h"

int device_ready = 0;

/** 
 * @brief: Initialize settings:
 *  - fill-in missing settings with default values
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 * 
 * @note: This function should be called before any other settings-related functions
 */
esp_err_t base_settings_init() {

    bool is_dynamically_allocated = false;

    // Parameter: MQTT connection mode
    mqtt_connection_mode_t mqtt_connection_mode;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_MQTT_CONNECT, mqtt_connection_mode);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_CONNECT);
        mqtt_connection_mode = S_DEFAULT_MQTT_CONNECT;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, mqtt_connection_mode) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_MQTT_CONNECT, mqtt_connection_mode);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_MQTT_CONNECT, mqtt_connection_mode);
            return ESP_FAIL;
        }
    }  

    // Parameter: MQTT server
    char *mqtt_server = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_MQTT_SERVER, &mqtt_server) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_MQTT_SERVER, mqtt_server);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_SERVER);
        mqtt_server = S_DEFAULT_MQTT_SERVER;
        if (nvs_write_string(S_NAMESPACE, S_KEY_MQTT_SERVER, mqtt_server) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_MQTT_SERVER, mqtt_server);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_MQTT_SERVER, mqtt_server);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(mqtt_server); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: MQTT port
    uint16_t mqtt_port;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, &mqtt_port) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_MQTT_PORT, mqtt_port);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_PORT);
        mqtt_port = S_DEFAULT_MQTT_PORT;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, mqtt_port) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_MQTT_PORT, mqtt_port);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_MQTT_PORT, mqtt_port);
            return ESP_FAIL;
        }
    }

    // Parameter: MQTT protocol
    char *mqtt_protocol = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, &mqtt_protocol) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_MQTT_PROTOCOL, mqtt_protocol);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_PROTOCOL);
        mqtt_protocol = S_DEFAULT_MQTT_PROTOCOL;
        if (nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, mqtt_protocol) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_MQTT_PROTOCOL, mqtt_protocol);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_MQTT_PROTOCOL, mqtt_protocol);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(mqtt_protocol); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: MQTT user
    char *mqtt_user = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_MQTT_USER, &mqtt_user) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_MQTT_USER, mqtt_user);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_USER);
        mqtt_user = S_DEFAULT_MQTT_USER;
        if (nvs_write_string(S_NAMESPACE, S_KEY_MQTT_USER, mqtt_user) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_MQTT_USER, mqtt_user);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_MQTT_USER, mqtt_user);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(mqtt_user); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: MQTT password
    char *mqtt_password = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, &mqtt_password) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_MQTT_PASSWORD, mqtt_password);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_PASSWORD);
        mqtt_password = S_DEFAULT_MQTT_PASSWORD;
        if (nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, mqtt_password) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_MQTT_PASSWORD, mqtt_password);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_MQTT_PASSWORD, mqtt_password);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(mqtt_password); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: MQTT prefix
    char *mqtt_prefix = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_MQTT_PREFIX, mqtt_prefix);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_MQTT_PREFIX);
        mqtt_prefix = S_DEFAULT_MQTT_PREFIX;
        if (nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, mqtt_prefix) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_MQTT_PREFIX, mqtt_prefix);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_MQTT_PREFIX, mqtt_prefix);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(mqtt_prefix); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: HomeAssistant MQTT Integration prefix
    char *ha_prefix = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_HA_PREFIX, &ha_prefix) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_HA_PREFIX, ha_prefix);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_HA_PREFIX);
        ha_prefix = S_DEFAULT_HA_PREFIX;
        if (nvs_write_string(S_NAMESPACE, S_KEY_HA_PREFIX, ha_prefix) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_HA_PREFIX, ha_prefix);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_HA_PREFIX, ha_prefix);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(ha_prefix); // for string (char*) params only
        is_dynamically_allocated = false;
    }

    // Parameter: Device ID (actually, MAC)
    char *device_id = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_DEVICE_ID, device_id);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_DEVICE_ID);

        char new_device_id[DEVICE_ID_LENGTH+1];
        uint8_t mac[6];  // Array to hold the MAC address
        ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));  // Use esp_read_mac to get the MAC address

        // Extract the last 3 bytes (6 characters in hexadecimal) of the MAC address
        snprintf(new_device_id, sizeof(new_device_id), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        if (nvs_write_string(S_NAMESPACE, S_KEY_DEVICE_ID, new_device_id) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_DEVICE_ID, new_device_id);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_DEVICE_ID, new_device_id);
            return ESP_FAIL;
        }
    }
    // free(device_id); // for string (char*) params only

    // Parameter: Device Serial (to be used for API)
    char *device_serial = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_DEVICE_SERIAL, device_serial);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_DEVICE_SERIAL);
        char new_device_serial[DEVICE_SERIAL_LENGTH+1];
        generate_serial_number(new_device_serial);       
        if (nvs_write_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, new_device_serial) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_DEVICE_SERIAL, new_device_serial);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_DEVICE_SERIAL, new_device_serial);
            return ESP_FAIL;
        }
    }

    // Parameter: Update Home Assistant definitions every X minutes
    uint32_t ha_upd_intervl;
    if (nvs_read_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, &ha_upd_intervl) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %li", S_KEY_HA_UPDATE_INTERVAL, ha_upd_intervl);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_HA_UPDATE_INTERVAL);
        ha_upd_intervl = S_DEFAULT_HA_UPDATE_INTERVAL;
        if (nvs_write_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, ha_upd_intervl) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %li", S_KEY_HA_UPDATE_INTERVAL, ha_upd_intervl);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %li", S_KEY_HA_UPDATE_INTERVAL, ha_upd_intervl);
            return ESP_FAIL;
        }
    }

    // Parameter: OTA Update url
    char *ota_update_url = NULL;
    if (nvs_read_string(S_NAMESPACE, S_KEY_OTA_UPDATE_URL, &ota_update_url) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %s", S_KEY_OTA_UPDATE_URL, ota_update_url);
        is_dynamically_allocated = true;
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_OTA_UPDATE_URL);
        ota_update_url = S_DEFAULT_OTA_UPDATE_URL;
        if (nvs_write_string(S_NAMESPACE, S_KEY_OTA_UPDATE_URL, ota_update_url) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %s", S_KEY_OTA_UPDATE_URL, ota_update_url);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %s", S_KEY_OTA_UPDATE_URL, ota_update_url);
            return ESP_FAIL;
        }
    }
    if (is_dynamically_allocated) {
        free(ha_prefix); // for string (char*) params only
        is_dynamically_allocated = false;
    }
    
    return ESP_OK;

}

/**
 * @brief: Device specific settings init
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
esp_err_t device_settings_init() {


    // Parameter: Relay refresh interval
    uint16_t relay_refr_int;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_RELAY_REFRESH_INTERVAL, &relay_refr_int) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_RELAY_REFRESH_INTERVAL, relay_refr_int);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_RELAY_REFRESH_INTERVAL);
        relay_refr_int = S_DEFAULT_RELAY_REFRESH_INTERVAL;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_RELAY_REFRESH_INTERVAL, relay_refr_int) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_RELAY_REFRESH_INTERVAL, relay_refr_int);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_RELAY_REFRESH_INTERVAL, relay_refr_int);
            return ESP_FAIL;
        }
    }

    // Parameter: Relay channels (actuators) count
    uint16_t channel_count;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_CHANNEL_COUNT, &channel_count) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_CHANNEL_COUNT, channel_count);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_CHANNEL_COUNT);
        channel_count = S_DEFAULT_CHANNEL_COUNT;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_CHANNEL_COUNT, channel_count) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_CHANNEL_COUNT, channel_count);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_CHANNEL_COUNT, channel_count);
            return ESP_FAIL;
        }
    }

    // Parameter: Relay channels (sensors) count
    uint16_t relay_sn_count;
    if (nvs_read_uint16(S_NAMESPACE, S_KEY_CONTACT_SENSORS_COUNT, &relay_sn_count) == ESP_OK) {
        ESP_LOGI(TAG, "Found parameter %s in NVS: %i", S_KEY_CONTACT_SENSORS_COUNT, relay_sn_count);
    } else {
        ESP_LOGW(TAG, "Unable to find parameter %s in NVS. Initiating...", S_KEY_CONTACT_SENSORS_COUNT);
        relay_sn_count = S_DEFAULT_CONTACT_SENSORS_COUNT;
        if (nvs_write_uint16(S_NAMESPACE, S_KEY_CONTACT_SENSORS_COUNT, relay_sn_count) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully created key %s with value %i", S_KEY_CONTACT_SENSORS_COUNT, relay_sn_count);
        } else {
            ESP_LOGE(TAG, "Failed creating key %s with value %i", S_KEY_CONTACT_SENSORS_COUNT, relay_sn_count);
            return ESP_FAIL;
        }
    }

    // not, let's iterate via all relays stored in the memory and try load/initiate them
    ESP_LOGI(TAG, "Settings: Initiating relays");
    for (int i_channel = 0; i_channel < channel_count; i_channel++) {
        relay_unit_t *relay;
        // Allocate memory for the relay pointer
        relay = malloc(sizeof(relay_unit_t));
        if (relay == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for relay");
            return ESP_FAIL; // Handle error accordingly
        }
        char *relay_nvs_key = get_relay_nvs_key(i_channel);
        if (relay_nvs_key == NULL) {
            ESP_LOGE(TAG, "Failed to get NVS key for channel %d", i_channel);
            return ESP_FAIL; // Handle error accordingly
        }
        if(load_relay_actuator_from_nvs(relay_nvs_key, relay) == ESP_OK) {
            ESP_LOGI(TAG, "Found relay channel %i stored in NVS at %s. PIN %i", i_channel, relay_nvs_key, relay->gpio_pin);
            // cleanup
            if (INIT_RELAY_ON_LOAD) {
                ESP_ERROR_CHECK(relay_gpio_deinit(relay));
            }
        } else {
            ESP_LOGW(TAG, "Unable to find relay channel %i stored in NVS at %s. Initiating...", i_channel, relay_nvs_key);

            // Initialize the relay using the get_actuator_relay function
            int gpio_pin = get_next_available_safe_gpio_pin();
            if (gpio_pin < 0) {
                ESP_LOGE(TAG, "No safe GPIO pins left! Cannot assign one to the relay unit. Aborting!");
                return ESP_FAIL;
            }
            *relay = get_actuator_relay(i_channel, gpio_pin);

            // Save the relay configuration to NVS
            esp_err_t err = save_relay_to_nvs(relay_nvs_key, relay);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save relay configuration to NVS");
                free(relay_nvs_key);
                free(relay);
                return ESP_FAIL;
            }

            // cleanup
            if (INIT_RELAY_ON_GET) {
                ESP_ERROR_CHECK(relay_gpio_deinit(relay));
            }

        }
        
        // set relay state to GPIO
        esp_err_t err = relay_set_state(relay, relay->state, false);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Was unable to set the relay state. Error: %s", esp_err_to_name(err));
        }

        // Free the memory allocated for the NVS key
        free(relay_nvs_key);
        free(relay);
    }

    // not, let's iterate via all sensors stored in the memory and try load/initiate them
    ESP_LOGI(TAG, "Settings: Initiating contact sensors");
    for (int i_channel = 0; i_channel < relay_sn_count; i_channel++) {
        relay_unit_t *relay;
        // Allocate memory for the relay pointer
        relay = malloc(sizeof(relay_unit_t));
        if (relay == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for contact sensor");
            return ESP_FAIL; // Handle error accordingly
        }
        char *relay_nvs_key = get_contact_sensor_nvs_key(i_channel);
        if (relay_nvs_key == NULL) {
            ESP_LOGE(TAG, "Failed to get NVS key for channel %d", i_channel);
            return ESP_FAIL; // Handle error accordingly
        }
        if(load_relay_sensor_from_nvs(relay_nvs_key, relay) == ESP_OK) {
            ESP_LOGI(TAG, "Found sensor contact channel %i stored in NVS at %s. PIN %i", i_channel, relay_nvs_key, relay->gpio_pin);
            // cleanup
            if (INIT_SENSORS_ON_LOAD) {
                ESP_ERROR_CHECK(relay_gpio_deinit(relay));
            }
        } else {
            ESP_LOGW(TAG, "Unable to find sensor contact channel %i stored in NVS at %s. Initiating...", i_channel, relay_nvs_key);

            // Initialize the relay using the get_actuator_relay function
            int gpio_pin = get_next_available_safe_gpio_pin();
            if (gpio_pin < 0) {
                ESP_LOGE(TAG, "No safe GPIO pins left! Cannot assign one to the relay unit. Aborting!");
                return ESP_FAIL;
            }
            *relay = get_sensor_relay(i_channel, gpio_pin);

            // Save the relay configuration to NVS
            esp_err_t err = save_relay_to_nvs(relay_nvs_key, relay);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save contact sensor configuration to NVS");
                free(relay_nvs_key);
                free(relay);
                return ESP_FAIL;
            }
            
            // cleanup
            if (INIT_SENSORS_ON_GET) {
                ESP_ERROR_CHECK(relay_gpio_deinit(relay));
            }
        }

        // Free the memory allocated for the NVS key
        free(relay_nvs_key);
        free(relay);
    }

    return ESP_OK;
}

/**
 * @brief: Initialize device settings
 */
esp_err_t settings_init() {

    // reset device readiness
    device_ready = 0;

    // call for basic platform settings (serial, ID, MQTT, HA, etc)
    if (base_settings_init() != ESP_OK) {
        ESP_LOGE(TAG, "Unable to init base settings. Unable to continue.");
        return ESP_FAIL;
    }

    // call for device specific settings
    if (device_settings_init() != ESP_OK) {
        ESP_LOGE(TAG, "Unable to init device settings. Unable to continue.");
        return ESP_FAIL;
    }

    // device ready
    device_ready = 1;
    return ESP_OK;
}

/**
 * @brief Generate serial number for the device
 * 
 * @param serial_number: Pointer to the buffer where the serial number will be stored
 */
void generate_serial_number(char *serial_number) {
    const char alphanum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int i;

    for (i = 0; i < DEVICE_SERIAL_LENGTH; ++i) {
        int index = esp_random() % (sizeof(alphanum) - 1);  // esp_random generates a random 32-bit number
        serial_number[i] = alphanum[index];
    }

    serial_number[DEVICE_SERIAL_LENGTH] = '\0';  // Null-terminate the string
}

/**
 * @brief: Init the filesystem / partition
 * 
 * @note: This function should be called before any other settings-related functions
 */
void init_filesystem() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format filesystem");
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information");
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}

/**
 * @brief Perform an OTA update from the given URL.
 *
 * This function initiates an OTA update process using the URL provided.
 * It configures the OTA client, downloads the firmware, and flashes it to the device.
 * Upon successful completion, the device will automatically reboot.
 *
 * @param url The URL of the firmware binary to download.
 * @return ESP_OK if the OTA update was successful, or an error code otherwise.
 */
esp_err_t perform_ota_update(const char *url) {
    ESP_LOGI(TAG, "Starting OTA update from URL: %s", url);
    
    char *ca_cert = NULL;
    
    // Load the CA root certificate
    if (load_ca_certificate(&ca_cert) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load CA certificate. Proceeding without it.");
    }

    // Configure HTTP client for OTA update
    esp_http_client_config_t ota_https_client_config = {
        .url = url,
        .timeout_ms = 5000,  // Timeout for OTA update
        .cert_pem = ca_cert  // Set CA certificate if loaded, otherwise NULL
    };

    // Configure OTA update
    esp_https_ota_config_t ota_config = {
        .http_config = &ota_https_client_config,
    };

    // Start the OTA update process
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful!");
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
    }

    // Free the CA certificate memory if it was dynamically allocated
    if (ca_cert) {
        free(ca_cert);
    }

    return ret;
}

/**
 * @file: settings.c
 * @brief Check the OTA partitions for the current running partition and its state.
 * 
 * This function checks the OTA partitions to determine the current running partition
 * and its state. It prints the running partition label and the OTA state to the console.
 * 
 * @return ESP_OK if the OTA partitions were checked successfully, or an error code otherwise.
 */
esp_err_t check_ota_partitions(void) {

    ESP_LOGI(TAG, "Checking OTA partitions");

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_LOGE(TAG, "Running partition not found!");
        return ESP_ERR_OTA_BASE;
    } else {
        ESP_LOGI(TAG, "Running from partition: %s", running->label);
    }

    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        ESP_LOGI(TAG, "OTA partition state: %d", ota_state);
    } else {
        ESP_LOGE(TAG, "Failed to get OTA state");
    }

    return ESP_OK;
}

/**
 * @brief: OTA update task
 * 
 * @param param: Pointer to the OTA update parameters
 * 
 * @note: This function should be called as a FreeRTOS task
 */
void ota_update_task(void *param) {
    ota_update_param_t *update_param = (ota_update_param_t *)param;
    esp_err_t ret = perform_ota_update(update_param->ota_url);
    
    // Check the result and handle any post-update logic
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update completed successfully.");
        ESP_LOGI(TAG, "Rebooting the device...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA update failed with error code: %s", esp_err_to_name(ret));
    }

    free(update_param);  // Free the allocated memory for parameters
    vTaskDelete(NULL);   // Delete the OTA update task
}