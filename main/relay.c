#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "cJSON.h"

#include "non_volatile_storage.h"

#include "common.h"
#include "settings.h"
#include "relay.h"
#include "mqtt.h"
#include "status.h"

const int SAFE_GPIO_PINS[SAFE_GPIO_COUNT] = {4, 5, 6, 7, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 39};

static QueueHandle_t gpio_evt_queue = NULL;

/* Routines */

/**
 * @brief: checks if a GPIO is in the safe list
 * @param gpio_pin GPIO pin number
 */
bool is_gpio_safe(int gpio_pin) {
    for (int i = 0; i < SAFE_GPIO_COUNT; i++) {
        if (SAFE_GPIO_PINS[i] == gpio_pin) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks if a specific GPIO pin is in use by any relay.
 *
 * @param pin The GPIO pin number to check.
 * @return
 *     - true: The pin is in use by some relay.
 *     - false: The pin is not in use.
 */
bool is_gpio_pin_in_use(int pin) {
    relay_unit_t *relay_list = NULL;
    uint16_t total_count = 0;
    esp_err_t err = get_all_relay_units(&relay_list, &total_count);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get relay units.");
        return false;  // Assume pin is not in use in case of failure
    }

    if (total_count < 1) {
        ESP_LOGW(TAG, "No initialized relay units are found. Pin is not in use.");
        return false;
    }

    for (int i = 0; i < total_count; i++) {
        if (relay_list[i].gpio_pin == pin) {
            const char *type_str = (relay_list[i].type == RELAY_TYPE_ACTUATOR) ? "actuator" : "sensor";
            ESP_LOGI(TAG, "GPIO pin %d is already in use by relay channel %d (%s)", 
                     pin, relay_list[i].channel, type_str);
            // ESP_ERROR_CHECK(free_relays_array(relay_list, total_count));  // Free the relay list memory
            free(relay_list);
            return true;
        }
    }

    // Free the relay list memory
    free(relay_list);

    ESP_LOGI(TAG, "GPIO pin %d is not in use", pin);
    return false;
}

/**
 * @brief Finds the next available safe GPIO pin that is not in use by any relay.
 *
 * @return
 *     - GPIO pin number: The first available safe GPIO pin.
 *     - -1: All safe GPIO pins are in use.
 */
int get_next_available_safe_gpio_pin() {
    for (int i = 0; i < SAFE_GPIO_COUNT; i++) {
        if (!is_gpio_pin_in_use(SAFE_GPIO_PINS[i])) {
            ESP_LOGI(TAG, "Found available safe GPIO pin: %d", SAFE_GPIO_PINS[i]);
            return SAFE_GPIO_PINS[i];  // Return the first available safe pin
        }
    }

    ESP_LOGW(TAG, "No available safe GPIO pins found.");
    return -1;  // All safe pins are in use
}


/**
 * @brief Initialize a relay actuator
 *
 * @param channel Relay channel number
 * @param pin GPIO pin number for the relay
 * @return Initialized relay_unit_t structure
 */
relay_unit_t get_actuator_relay(int channel, int pin) {
    // Initialize relay_unit_t structure
    relay_unit_t relay;
    relay.channel = channel;
    relay.state = RELAY_STATE_OFF; // Default to OFF
    relay.inverted = false;        // Set to false by default, can be adjusted if needed
    relay.gpio_pin = pin;
    relay.enabled = true;          // Enable by default
    relay.gpio_initialized = false;
    relay.type = RELAY_TYPE_ACTUATOR;

    if(INIT_RELAY_ON_GET) {
        if(relay_gpio_init(&relay) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to init GPIO pin (%d) for actuator relay unit (%d)", relay.gpio_pin, relay.channel);
        }
    } else {
        relay.io_conf = (gpio_config_t){0};
    }

    ESP_LOGI(TAG, "Relay actuator initialized on channel %d, GPIO pin %d", channel, pin);
    return relay;
}

/**
 * @brief Initialize a relay contact sensor
 *
 * @param channel Relay channel number
 * @param pin GPIO pin number for the contact sensor
 * @return Initialized relay_unit_t structure
 */
relay_unit_t get_sensor_relay(int channel, int pin) {
    // Initialize relay_unit_t structure
    relay_unit_t relay;
    relay.channel = channel;
    relay.state = RELAY_STATE_OFF; // Default to OFF (no contact)
    relay.inverted = false;        // Set to false by default, can be adjusted if needed
    relay.gpio_pin = pin;
    relay.enabled = true;          // Enable by default
    relay.gpio_initialized = false;
    relay.type = RELAY_TYPE_SENSOR;

    if(INIT_SENSORS_ON_GET) {
        if(relay_gpio_init(&relay) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to init GPIO pin (%d) for conctact sensor relay unit (%d)", relay.gpio_pin, relay.channel);
        }
    } else {
        relay.io_conf = (gpio_config_t){0};
    } 

    ESP_LOGI(TAG, "Contact sensor initialized on channel %d, GPIO pin %d", channel, pin);
    return relay;
}

/**
 * @brief: Init relay unit's (either actuator or sensor) GPIO. It defines GPIO pin parameters according to the relay type (actuator or contact sensor), and calls gpio_config().
 * 
 * @param[in,out] relay Pointer to the relay unit for which GPIO pin shall be initialized
 * @return esp_err_t result of the operation
 */
esp_err_t relay_gpio_init(relay_unit_t *relay) {
    if (relay == NULL) {
        ESP_LOGE(TAG, "NULL value for relay unit");
        return ESP_ERR_INVALID_ARG;
    }

    // check gpio init flag
    if (relay->gpio_initialized) {
        ESP_LOGW(TAG, "GPIO pin %d seems to be already inialized. Flag set to TRUE. Potential risk of memory leak.", relay->gpio_pin);
    }

    gpio_config_t io_conf = {0};
    io_conf.pin_bit_mask = (1ULL << relay->gpio_pin);
    
    // Configure GPIO based on relay type (actuator or sensor)
    switch (relay->type) {
        case RELAY_TYPE_SENSOR:
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;     // Enable pull-up
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;  // Disable pull-down
            io_conf.intr_type = GPIO_INTR_ANYEDGE;  // Interrupt on any edge
            break;

        case RELAY_TYPE_ACTUATOR:
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.intr_type = GPIO_INTR_DISABLE;
            break;

        default:
            ESP_LOGE(TAG, "Invalid relay type: %d", relay->type);
            return ESP_ERR_INVALID_ARG;
    }

    // Apply GPIO configuration
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed for pin %d, error: %s", relay->gpio_pin, esp_err_to_name(err));
        return err;
    }

    // Assign io_conf to relay struct
    relay->io_conf = io_conf;

    // set flag to true
    relay->gpio_initialized = true;

    return ESP_OK;
}

/**
 * @brief: Register ISR interrupt handler to the pin of the sensor
 * 
 * @param[in,out] relay Pointer to the relay unit for which GPIO pin shall ISR be registered
 * @return esp_err_t result of the operation
 */
esp_err_t relay_sensor_register_isr(relay_unit_t *relay) {

    if (relay == NULL) {
        ESP_LOGE(TAG, "NULL value for relay unit");
        return ESP_ERR_INVALID_ARG;
    }

    // Check if GPIO is initialized
    if (!relay->gpio_initialized) {
        ESP_LOGE(TAG, "io_conf not initialized. Cannot continue.");
        return ESP_ERR_INVALID_ARG;
    }

    // Register ISR handler for sensor-type relays
    if (relay->type == RELAY_TYPE_SENSOR) {
        // Add ISR handler for the specific GPIO pin
        gpio_isr_handler_add(relay->gpio_pin, gpio_isr_handler, (void *)(relay->gpio_pin));
        ESP_LOGI(TAG, "ISR handler added for GPIO pin %d", relay->gpio_pin);
    } else {
        ESP_LOGE(TAG, "Relay unit is not sensor.");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}


/**
 * @brief GPIO interrupt service routine (ISR) handler.
 * 
 * This ISR is triggered when a GPIO interrupt occurs. It reads the GPIO pin state
 * and sends an event to the queue for further processing by a FreeRTOS task.
 *
 * @param[in] arg A pointer to the GPIO pin number that triggered the interrupt.
 */
void IRAM_ATTR gpio_isr_handler(void *arg) {
    int gpio_num = (int)arg;
    gpio_event_t evt;
    evt.gpio_num = gpio_num;
    evt.level = gpio_get_level(gpio_num);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (gpio_evt_queue != NULL) {
        if (xQueueSendFromISR(gpio_evt_queue, &evt, &xHigherPriorityTaskWoken) != pdPASS) {
            // Logging removed: Don't log inside ISR
        }
    }

    // Perform a context switch if necessary
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief FreeRTOS task to handle GPIO events.
 * 
 * This task processes GPIO events that are posted to the event queue by the ISR.
 * It includes optional debounce logic to filter out bouncing signals and updates 
 * the contact sensor state in NVS if necessary.
 *
 * @param[in] arg Unused task argument.
 */
void gpio_event_task(void *arg) {
    gpio_event_t evt;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &evt, portMAX_DELAY)) {
            ESP_LOGI(TAG, "GPIO[%d] intr, val: %d", evt.gpio_num, evt.level);

            // Debouncing logic
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
            int current_level = gpio_get_level(evt.gpio_num);
            if (current_level != evt.level) {
                ESP_LOGW(TAG, "Debounce detected, ignoring event");
                continue;
            }

            // Get the list of contact sensors from NVS
            relay_unit_t *sensors = NULL;
            uint16_t sensor_count = 0;
            esp_err_t err = get_contact_sensor_list(&sensors, &sensor_count);
            if (err != ESP_OK || sensors == NULL) {
                ESP_LOGE(TAG, "Failed to get contact sensor list from NVS");
                continue;
            }

            // Find the sensor associated with the GPIO pin
            relay_unit_t relay;
            for (int i = 0; i < sensor_count; i++) {
                if (sensors[i].gpio_pin == evt.gpio_num) {
                    relay = sensors[i];
                    break;
                }
            }

            // Update relay state (if necessary)
            if (relay.inverted) {
                relay.state = (current_level == 1) ? RELAY_STATE_OFF : RELAY_STATE_ON;
            } else {
                relay.state = (current_level == 1) ? RELAY_STATE_ON : RELAY_STATE_OFF;
            }


            // Get NVS key for the contact sensor and save state to NVS
            char *relay_nvs_key = get_contact_sensor_nvs_key(relay.channel);
            if (relay_nvs_key == NULL) {
                ESP_LOGE(TAG, "Failed to get NVS key for channel %d", relay.channel);
                free_relays_array(sensors, sensor_count);  // Free sensor list before exiting
                continue;
            }

            ESP_LOGI(TAG, ">>> Saving new relay contact state (%d) to NVS. Key (%s), channel (%d), pin (%d)", (int)relay.state, relay_nvs_key, relay.channel, relay.gpio_pin);
            err = save_relay_to_nvs(relay_nvs_key, &relay);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save contact sensor state to NVS");
                continue;
            }

            // publish to MQTT
            uint16_t mqtt_connection_mode;
            ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));
            if (_DEVICE_ENABLE_MQTT && mqtt_connection_mode && g_mqtt_ready) {
                mqtt_publish_relay_data(&relay);
                ESP_ERROR_CHECK(trigger_mqtt_publish(get_unit_nvs_key(&relay), relay.type));
            }

            free(relay_nvs_key);  // Free the dynamically allocated NVS key
            // cleanup
            free_relays_array(sensors, sensor_count);  // Free sensor list before exiting   
        }
    }
}

/**
 * @brief: Captures the level currently on the GPIO pin and saves it to NVS
 * 
 * @param[in,out] relay Pointer to the relay unit for which GPIO pin shall be refreshed
 * @return esp_err_t result of the operation
 */
esp_err_t relay_sensor_gpio_state_refresh(relay_unit_t *relay) {

    // is relay an instance ?
    if (relay == NULL) {
        ESP_LOGE(TAG, "NULL value for relay unit");
        return ESP_ERR_INVALID_ARG;
    }

    if (relay->type != RELAY_TYPE_SENSOR) {
        ESP_LOGE(TAG, "Relay unit is not sensor.");
        return ESP_ERR_INVALID_ARG;
    }

    int current_level = gpio_get_level(relay->gpio_pin);

    // Update relay state (if necessary)
    if (relay->inverted) {
        relay->state = (current_level == 1) ? RELAY_STATE_OFF : RELAY_STATE_ON;
    } else {
        relay->state = (current_level == 1) ? RELAY_STATE_ON : RELAY_STATE_OFF;
    }

    // Get NVS key for the relay and save state to NVS
    char *relay_nvs_key = get_contact_sensor_nvs_key(relay->channel);
    if (relay_nvs_key == NULL) {
        ESP_LOGE(TAG, "Failed to get NVS key for channel %d", relay->channel);
    }

    ESP_LOGI(TAG, ">>> Saving new relay contact state (%d) to NVS. Key (%s), channel (%d), pin (%d)", (int)relay->state, relay_nvs_key, relay->channel, relay->gpio_pin);
    esp_err_t err = save_relay_to_nvs(relay_nvs_key, relay);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save contact sensor state to NVS");
    }

    free(relay_nvs_key);  // Free the dynamically allocated NVS key
    return ESP_OK;
}


/**
 * @brief: Reset the GPIO pin configuration and free the associated object
 * 
 * @param[out] relay Pointer to the relay unit for which GPIO pin shall be de-initialized and freed
 * @return esp_err_t result of the operation
 */
esp_err_t relay_gpio_deinit(relay_unit_t *relay) {

    // is relay an instance ?
    if (relay == NULL) {
        ESP_LOGE(TAG, "NULL value for relay unit. Nothing to de-init. Channel (%d), type (%d)", relay->channel, relay->type);
        return ESP_ERR_INVALID_ARG;
    }

    // Protect against non-initialized GPIO
    if (!relay->gpio_initialized) {
        ESP_LOGW(TAG, "GPIO pin %d is not initialized. Nothing to de-init. Channel (%d), type (%d)", relay->gpio_pin, relay->channel, relay->type);
        return ESP_OK;
    }

    // Deinitialize the GPIO pin
    relay->io_conf = (gpio_config_t){0};
    relay->gpio_initialized = false;

    ESP_LOGI(TAG, "GPIO pin %d de-initialized. Channel (%d), type (%d)", relay->gpio_pin, relay->channel, relay->type);

    return ESP_OK;
}

/**
 * @brief Save a relay unit to NVS
 *
 * @param key NVS key
 * @param relay Pointer to the relay unit to be saved
 * @return esp_err_t result of the NVS operation
 */
esp_err_t save_relay_to_nvs(const char *key, relay_unit_t *relay) {

    if (relay == NULL) {
        ESP_LOGE(TAG, "Got NULL as relay. Cannot save.");
        return ESP_ERR_INVALID_ARG;
    }

    // create a working copy
    relay_unit_t *relay_copy = (relay_unit_t *)malloc(sizeof(relay_unit_t));
    if (relay_copy == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for relay copy");
        return ESP_ERR_NO_MEM;
    }
    memcpy(relay_copy, relay, sizeof(relay_unit_t));

    //reset the io_conf property and the flag
    relay_copy->io_conf = (gpio_config_t){0};
    relay_copy->gpio_initialized = false;

    // save the copy to NVS
    esp_err_t err = nvs_write_blob(S_NAMESPACE, key, relay_copy, sizeof(relay_unit_t));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Relay successfully saved to NVS under key: %s", key);
    } else {
        ESP_LOGE(TAG, "Failed to save relay to NVS: %s", esp_err_to_name(err));
    }

    free(relay_copy);
    return err;
}

/**
 * @brief Load a relay actuator from NVS
 *
 * @param namespace NVS namespace
 * @param key NVS key
 * @param relay Pointer to the relay unit to be loaded
 * @return esp_err_t result of the NVS operation
 */
esp_err_t load_relay_actuator_from_nvs(const char *key, relay_unit_t *relay) {
    esp_err_t err = nvs_read_blob(S_NAMESPACE, key, relay, sizeof(relay_unit_t));
    if (err != ESP_OK) {
        /* ESP_LOGE(TAG, "Failed to load relay from NVS: %s", esp_err_to_name(err)); */
        return err;
    }

    if(INIT_RELAY_ON_LOAD) {
        if(relay_gpio_init(relay) != ESP_OK) {
            // raise just warning
            ESP_LOGW(TAG, "Failed to init GPIO pin (%d) for actuator relay unit (%d)", relay->gpio_pin, relay->channel);
        }
    } else {
        relay->io_conf = (gpio_config_t){0};
    }

    ESP_LOGI(TAG, "Relay actuator loaded successfully from NVS under key: %s", key);
    return ESP_OK;
}

/**
 * @brief Load a relay sensor from NVS
 *
 * @param key NVS key
 * @param relay Pointer to the relay unit to be loaded
 * @return esp_err_t result of the NVS operation
 */
esp_err_t load_relay_sensor_from_nvs(const char *key, relay_unit_t *relay) {
    esp_err_t err = nvs_read_blob(S_NAMESPACE, key, relay, sizeof(relay_unit_t));
    if (err != ESP_OK) {
        /* ESP_LOGE(TAG, "Failed to load relay from NVS: %s", esp_err_to_name(err)); */
        return err;
    }

    if(INIT_SENSORS_ON_LOAD) {
        if(relay_gpio_init(relay) != ESP_OK) {
            // raise just warning
            ESP_LOGW(TAG, "Failed to init GPIO pin (%d) for contact sensor relay unit (%d)", relay->gpio_pin, relay->channel);
        }
    } else {
        relay->io_conf = (gpio_config_t){0};
    }


    ESP_LOGI(TAG, "Relay sensor loaded successfully from NVS under key: %s", key);
    return ESP_OK;
}

/**
 * @brief Get the NVS key for a relay channel
 *
 * @param channel The relay channel number
 * @return Dynamically allocated string with the NVS key, or NULL if out of range
 */
char *get_relay_nvs_key(int channel) {
    // Check if the channel is within the valid range
    if (channel < CHANNEL_COUNT_MIN || channel > CHANNEL_COUNT_MAX) {
        ESP_LOGE(TAG, "Channel %d is out of valid range (%d - %d)", channel, CHANNEL_COUNT_MIN, CHANNEL_COUNT_MAX);
        return NULL;
    }

    // Allocate memory for the key string
    size_t key_size = snprintf(NULL, 0, "%s%d", S_KEY_CH_PREFIX, channel) + 1;
    char *key = malloc(key_size);
    if (key == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for NVS key");
        return NULL;
    }

    // Create the key string
    snprintf(key, key_size, "%s%d", S_KEY_CH_PREFIX, channel);

    return key;
}

/**
 * @brief Get the NVS key for a contact sensor channel
 *
 * @param channel The relay contact sensor number
 * @return Dynamically allocated string with the NVS key, or NULL if out of range
 */
char *get_contact_sensor_nvs_key(int channel) {
    // Check if the channel is within the valid range
    if (channel < CONTACT_SENSORS_COUNT_MIN || channel > CONTACT_SENSORS_COUNT_MAX) {
        ESP_LOGE(TAG, "Channel %d is out of valid range (%d - %d)", channel, CONTACT_SENSORS_COUNT_MIN, CONTACT_SENSORS_COUNT_MAX);
        return NULL;
    }

    // Allocate memory for the key string
    size_t key_size = snprintf(NULL, 0, "%s%d", S_KEY_SN_PREFIX, channel) + 1;
    char *key = malloc(key_size);
    if (key == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for NVS key");
        return NULL;
    }

    // Create the key string
    snprintf(key, key_size, "%s%d", S_KEY_SN_PREFIX, channel);

    return key;
}


/**
 * @brief: Get NVS key for the provided relay
 * 
 * @param relay Pointer to the relay_unit_t structure
 * @return Dynamically allocated string with the NVS key, or NULL if type or other params are out of range
 */
char *get_unit_nvs_key(const relay_unit_t *relay) {

    // is relay an instance ?
    if (relay == NULL) {
        ESP_LOGE(TAG, "NULL value for relay unit");
        return NULL;
    }
    // Configure GPIO based on relay type (actuator or sensor)
    switch (relay->type) {
        case RELAY_TYPE_SENSOR:
            return get_contact_sensor_nvs_key(relay->channel);
            break;
        case RELAY_TYPE_ACTUATOR:
            return get_relay_nvs_key(relay->channel);
            break;
        default:
            ESP_LOGE(TAG, "Invalid relay type: %d", relay->type);
            return NULL;
    }
}


/**
 * @brief Determines the relay type from the relay_key.
 * 
 * This function checks the prefix of the provided relay_key and determines whether
 * the relay is an actuator or a contact sensor based on the prefix.
 * 
 * @param[in] relay_key The key that identifies the relay (e.g., "relay_ch_0" or "relay_sn_1").
 * @return relay_type_t The type of relay (RELAY_TYPE_ACTUATOR or RELAY_TYPE_SENSOR). 
 *         Returns -1 if the prefix doesn't match.
 */
relay_type_t get_relay_type_from_key(const char *relay_key) {
    // Check for the actuator prefix
    if (strncmp(relay_key, S_KEY_CH_PREFIX, strlen(S_KEY_CH_PREFIX)) == 0) {
        return RELAY_TYPE_ACTUATOR;
    }
    // Check for the contact sensor prefix
    else if (strncmp(relay_key, S_KEY_SN_PREFIX, strlen(S_KEY_SN_PREFIX)) == 0) {
        return RELAY_TYPE_SENSOR;
    }
    // Return -1 for unknown types
    else {
        ESP_LOGE(TAG, "Unknown relay type for key: %s", relay_key);
        return -1;
    }
}

/**
 * @brief Serialize relay_unit_t structure to a JSON string
 *
 * This function takes a pointer to a relay_unit_t structure and serializes its fields 
 * into a JSON formatted string. The returned JSON string should be freed by the caller 
 * using `free()` after use.
 *
 * @param[in] relay Pointer to the relay_unit_t structure to be serialized
 * @return A dynamically allocated JSON string representing the relay_unit_t structure,
 *         or NULL if serialization fails.
 */
char* serialize_relay_unit(const relay_unit_t *relay) {
    cJSON *relay_json = cJSON_CreateObject();
    if (relay_json == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object for relay serialization");
        return NULL;
    }

    // Generate relay_key based on type
    char *relay_key = (relay->type == RELAY_TYPE_ACTUATOR) ? get_relay_nvs_key(relay->channel) : get_contact_sensor_nvs_key(relay->channel);
    cJSON_AddStringToObject(relay_json, "relay_key", relay_key);

    // Serialize the fields
    cJSON_AddNumberToObject(relay_json, "channel", relay->channel);
    cJSON_AddBoolToObject(relay_json, "state", relay->state == RELAY_STATE_ON);
    cJSON_AddBoolToObject(relay_json, "inverted", relay->inverted);
    cJSON_AddNumberToObject(relay_json, "gpio_pin", relay->gpio_pin);
    cJSON_AddBoolToObject(relay_json, "enabled", relay->enabled);
    cJSON_AddNumberToObject(relay_json, "type", relay->type);

    // Convert the JSON object to a string
    char *json_string = cJSON_PrintUnformatted(relay_json);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to convert relay JSON object to string");
    }

    // Free the JSON object and relay_key
    cJSON_Delete(relay_json);
    free(relay_key);

    return json_string;
}

/**
 * @brief Deserialize a JSON string to relay_unit_t structure
 *
 * This function parses a JSON formatted string and populates the relay_unit_t structure 
 * with its corresponding fields. The function expects the input JSON string to have the 
 * same structure as that produced by `serialize_relay_unit()`.
 *
 * @param[in] json_str The JSON string to be deserialized
 * @param[out] relay Pointer to the relay_unit_t structure to populate with deserialized data
 * @return ESP_OK on success, or ESP_FAIL if deserialization fails or fields are invalid.
 */
esp_err_t deserialize_relay_unit(const char *json_str, relay_unit_t *relay) {
    cJSON *relay_json = cJSON_Parse(json_str);
    if (relay_json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON string for relay deserialization");
        return ESP_FAIL;
    }

    // Deserialize the fields
    cJSON *channel = cJSON_GetObjectItem(relay_json, "channel");
    cJSON *state = cJSON_GetObjectItem(relay_json, "state");
    cJSON *inverted = cJSON_GetObjectItem(relay_json, "inverted");
    cJSON *gpio_pin = cJSON_GetObjectItem(relay_json, "gpio_pin");
    cJSON *enabled = cJSON_GetObjectItem(relay_json, "enabled");
    cJSON *type = cJSON_GetObjectItem(relay_json, "type");

    if (!cJSON_IsNumber(channel) || !cJSON_IsBool(state) || !cJSON_IsBool(inverted) || !cJSON_IsNumber(gpio_pin) || !cJSON_IsBool(enabled) || !cJSON_IsNumber(type)) {
        ESP_LOGE(TAG, "Invalid or missing fields in JSON object for relay deserialization");
        cJSON_Delete(relay_json);
        return ESP_FAIL;
    }

    relay->channel = channel->valueint;
    relay->state = state->valueint ? RELAY_STATE_ON : RELAY_STATE_OFF;
    relay->inverted = inverted->valueint;
    relay->gpio_pin = gpio_pin->valueint;
    relay->enabled = enabled->valueint;
    relay->type = (relay_type_t)type->valueint;

    cJSON_Delete(relay_json);
    return ESP_OK;
}

/**
 * @brief: Create list of safe pins
 */
void populate_safe_gpio_pins(char *buffer, size_t buffer_size) {
    // Initialize the buffer with an empty string
    buffer[0] = '\0';

    // Iterate over SAFE_GPIO_PINS array and append each pin to the buffer
    for (int i = 0; i < SAFE_GPIO_COUNT; i++) {
        // Append a comma after each pin except the last one
        if (i == SAFE_GPIO_COUNT - 1) {
            snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%d", SAFE_GPIO_PINS[i]);
        } else {
            snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%d, ", SAFE_GPIO_PINS[i]);
        }
    }
}

/**
 * @brief Retrieves the list of relay actuators from NVS.
 * 
 * This function reads the number of relay actuators from NVS, allocates memory 
 * for the list, and loads each relay actuator's configuration from NVS into the list.
 * 
 * @param[out] relay_list Pointer to the array of relay_unit_t that will hold the relay actuators.
 * @param[out] count Pointer to store the number of relay actuators retrieved.
 * 
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t get_relay_list(relay_unit_t **relay_list, uint16_t *count) {

    uint16_t stored_relays_count = 0; // number of actually stored and initialized relays
    uint16_t relay_ch_count;
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_CHANNEL_COUNT, &relay_ch_count));

    // If there are no relays, skip allocation and return success
    if (relay_ch_count == 0) {
        ESP_LOGW(TAG, "No relays allocated. NULL array will be returned.");
        *relay_list = NULL;  // No need to allocate memory for an empty list
        return ESP_OK;
    }

    // Allocate memory for the list of relays
    *relay_list = (relay_unit_t *)malloc(sizeof(relay_unit_t) * relay_ch_count);
    if (*relay_list == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for relay list");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Processing relay array...");
    for (int i_channel = 0; i_channel < relay_ch_count; i_channel++) {
        relay_unit_t relay;
        char *relay_nvs_key = get_relay_nvs_key(i_channel);
        if (load_relay_actuator_from_nvs(relay_nvs_key, &relay) == ESP_OK) {
            (*relay_list)[i_channel] = relay;  // Add relay to the list
            stored_relays_count++;
        }
        free(relay_nvs_key);
    }

    *count = stored_relays_count;

    return ESP_OK;
}

/**
 * @brief Retrieves the list of contact sensors from NVS.
 * 
 * This function reads the number of contact sensors from NVS, allocates memory 
 * for the list, and loads each contact sensor's configuration from NVS into the list.
 * 
 * @param[out] sensor_list Pointer to the array of relay_unit_t that will hold the contact sensors.
 * @param[out] count Pointer to store the number of contact sensors retrieved.
 * 
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t get_contact_sensor_list(relay_unit_t **sensor_list, uint16_t *count) {
    uint16_t stored_sensors_count = 0; // number of actually stored and initialized contact sensors

    // Read the number of contact sensors from NVS
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_CONTACT_SENSORS_COUNT, count));

    // If there are no contact sensors, skip allocation and return success
    if (*count == 0) {
        ESP_LOGW(TAG, "No contact sensors allocated. NULL array will be returned.");
        *sensor_list = NULL;  // No need to allocate memory for an empty list
        return ESP_OK;
    }

    // Allocate memory for the list of contact sensors
    *sensor_list = (relay_unit_t *)malloc(sizeof(relay_unit_t) * (*count));
    if (*sensor_list == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for contact sensor list");
        return ESP_ERR_NO_MEM;
    }

    // Loop through and load contact sensors from NVS
    ESP_LOGI(TAG, "Processing contact sensors array...");
    for (int i_channel = 0; i_channel < *count; i_channel++) {
        relay_unit_t relay;
        char *relay_nvs_key = get_contact_sensor_nvs_key(i_channel);
        if (load_relay_sensor_from_nvs(relay_nvs_key, &relay) == ESP_OK) {
            (*sensor_list)[i_channel] = relay;  // Add contact sensor to the list
            stored_sensors_count++;
        }
        free(relay_nvs_key);
    }

    *count = stored_sensors_count;

    return ESP_OK;
}

/**
 * @brief Combines the list of relay actuators and contact sensors into one list.
 * 
 * This function retrieves both the relay actuators and contact sensors from NVS, 
 * allocates memory for a combined list, and appends both lists together. The combined 
 * list includes all relay actuators and contact sensors.
 * 
 * @param[out] relay_list Pointer to the combined array of relay_unit_t that will hold both relays and sensors.
 * @param[out] total_count Pointer to store the total number of relays and sensors retrieved.
 * 
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t get_all_relay_units(relay_unit_t **relay_list, uint16_t *total_count) {
    relay_unit_t *actuators = NULL, *sensors = NULL;
    uint16_t actuator_count = 0, sensor_count = 0;

    // Get actuators (relays) and handle the case where there are no actuators
    ESP_ERROR_CHECK(get_relay_list(&actuators, &actuator_count));

    // Get contact sensors and handle the case where there are no contact sensors
    ESP_ERROR_CHECK(get_contact_sensor_list(&sensors, &sensor_count));

    // Calculate the total count
    *total_count = actuator_count + sensor_count;

    ESP_LOGI(TAG, "Found %d actuator(s) and %d contact sensor(s). Total: %d unit(s).", actuator_count, sensor_count, *total_count);

    // If there are no relays or sensors, return success without allocating memory
    if (*total_count == 0) {
        *relay_list = NULL;  // No relays or sensors, so no allocation needed
        return ESP_OK;
    }

    // Allocate memory for the combined list of relays and sensors
    *relay_list = (relay_unit_t *)malloc(sizeof(relay_unit_t) * (*total_count));
    if (*relay_list == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for combined relay list");
        if (INIT_RELAY_ON_LOAD) {
            ESP_ERROR_CHECK(free_relays_array(actuators, actuator_count));
        } else {
            free(actuators);
        }
        if (INIT_SENSORS_ON_LOAD) {
            ESP_ERROR_CHECK(free_relays_array(sensors, sensor_count));
        } else {
            free(sensors);
        }
        return ESP_ERR_NO_MEM;
    }

    // Copy actuators to the combined list
    if (actuator_count > 0) {
        memcpy(*relay_list, actuators, sizeof(relay_unit_t) * actuator_count);
        if (INIT_RELAY_ON_LOAD) {
            ESP_ERROR_CHECK(free_relays_array(actuators, actuator_count));
        } else {
            free(actuators);
        }
    }

    // Copy sensors to the combined list after actuators
    if (sensor_count > 0) {
        memcpy(*relay_list + actuator_count, sensors, sizeof(relay_unit_t) * sensor_count);
        if (INIT_SENSORS_ON_LOAD) {
            ESP_ERROR_CHECK(free_relays_array(sensors, sensor_count));
        } else {
            free(sensors);
        }
    }

    return ESP_OK;
}


/**
 * @brief Frees memory allocated for relay_unit_t array, including dynamically allocated io_conf field.
 *
 * @param relay_list Pointer to an array of relay_unit_t.
 * @param count Number of elements in the relay_list array.
 * @return
 *     - ESP_OK: Successfully freed all memory.
 *     - ESP_ERR_INVALID_ARG: relay_list is NULL.
 */
esp_err_t free_relays_array(relay_unit_t *relay_list, size_t count) {
    if (relay_list == NULL || count < 1) {
        ESP_LOGE(TAG, "relay_list is NULL or array size is zero, nothing to free");
        return ESP_ERR_INVALID_ARG;
    }

    // Iterate through each relay and free the dynamically allocated io_conf field
    for (size_t i = 0; i < count; i++) {
        relay_gpio_deinit(&relay_list[i]);
    }

    // Free the relay list itself
    free(relay_list);

    return ESP_OK;  // Indicate success
}


/**
 * @brief: Runs relay_sensor_register_isr() for all contact sensors in the system. Recommended to be called once at the start of the app.
 * 
 * @return
 *     - ESP_OK: Successfully freed all memory.
 *     - ESP_ERR_INVALID_ARG: relay_list is NULL.
 */
esp_err_t relay_all_sensors_register_isr() {
    /* Init the queue */
    gpio_evt_queue = xQueueCreate(10, sizeof(gpio_event_t));
    if (gpio_evt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create the queue");
        return ESP_FAIL;  // Exit if queue creation fails
    }

    /* Install ISR service with default configuration */
    if (gpio_install_isr_service(0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install ISR service with default configuration");
        return ESP_FAIL;
    }

    /* Process sensors */
    relay_unit_t *sensors = NULL;
    uint16_t sensor_count = 0;

    // Get contact sensors and handle the case where there are no contact sensors
    if(get_contact_sensor_list(&sensors, &sensor_count) != ESP_OK) {
        ESP_LOGE(TAG, "Unable to get sensors list from NVS");
        return ESP_FAIL;
    }

    // check if there're any sensor to deal with
    if (sensor_count == 0) {
        ESP_LOGW(TAG, "No sensors found to register ISR for.");
        return ESP_OK;
    }

    // register ISR for each sensor
    for(int i = 0; i < sensor_count; i++) {
        if (relay_sensor_register_isr(&sensors[i]) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to register ISR for pin %d", sensors[i].gpio_pin);
            continue;
        }
        if (relay_sensor_gpio_state_refresh(&sensors[i]) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to refresh state on pin %d", sensors[i].gpio_pin);
        } else {
            ESP_LOGI(TAG, "Refreshed state on pin %d, state %d", sensors[i].gpio_pin, sensors[i].state);
        }
    }

    // cleanup
    if (INIT_SENSORS_ON_LOAD) {
        ESP_ERROR_CHECK(free_relays_array(sensors, sensor_count));
    } else {
        free(sensors);
    }

    return ESP_OK;
}


/**
 * @brief: Set the state of the relay unit (actuator), activate corresponding GPIO and persist the state to NVS.
 * 
 * @param[in, out] relay Pointer to the relay_unit_t structure
 * @param state Relay state relay_state_t to set
 * @param persist Save the update state to NVS. true: sets GPIO level and saves the relay to NVS; false: just sets GPIO level
 * @return
 *     - ESP_OK: Successfully completed all operations.
 *     - ESP_ERR_INVALID_ARG: relay is NULL.
 */
esp_err_t relay_set_state(relay_unit_t *relay, relay_state_t state, bool persist) {

    dump_current_task();

    bool gpio_init_made = false;

    // is relay an instance ?
    if (relay == NULL) {
        ESP_LOGE(TAG, "NULL value for relay unit");
        return ESP_ERR_INVALID_ARG;
    }

    // check if the relay is actuator
    if (relay->type != RELAY_TYPE_ACTUATOR) {
         ESP_LOGE(TAG, "Setting state not applicable: relay unit is not an actuator. Channel (%d).", relay->channel);
        return ESP_ERR_INVALID_ARG;       
    }

    // Init GPIO
    if (!relay->gpio_initialized) {
        if (relay_gpio_init(relay) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init GPIO pin before setting the state. Channel (%d). ", relay->channel);
            return ESP_FAIL;
        } else {
            ESP_LOGI(TAG, "Initiated GPIO pin. Channel (%d). ", relay->channel);
            gpio_init_made = true;
        }
    }

    // set level
    if (gpio_set_level((gpio_num_t)relay->gpio_pin, relay->inverted ? (uint32_t)(!state) : (uint32_t)state) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO level. Channel (%d), level (%d).", relay->channel, relay->state);
        ESP_ERROR_CHECK(relay_gpio_deinit(relay));
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, ">|>|>| Successfully set GPIO level. Channel (%d), level (%d).", relay->channel, relay->state);
    }

    // Update the state in the object once it was successfully set to GPIO
    relay->state = (relay_state_t)state;

    // reset GPIO pin to free memory
    if (gpio_init_made) {
        ESP_ERROR_CHECK(relay_gpio_deinit(relay));
    }

    // update via MQTT
    uint16_t mqtt_connection_mode;
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));
    if (_DEVICE_ENABLE_MQTT && mqtt_connection_mode && g_mqtt_ready) {
       ESP_ERROR_CHECK(trigger_mqtt_publish(get_unit_nvs_key(relay), relay->type));
    }

    if (persist) {
        // persist new state to NVS
        char *relay_nvs_key = get_relay_nvs_key(relay->channel);
        if (relay_nvs_key == NULL) {
            ESP_LOGE(TAG, "Failed to get NVS key for channel %d", relay->channel);
            return ESP_FAIL; // Handle error accordingly
        }
        if (save_relay_to_nvs(relay_nvs_key, relay) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to save relay unit to NVS");
            return ESP_FAIL;
        }
        free(relay_nvs_key);
    }

    return ESP_OK;
}


/**
 * @brief Publishes all relay units' states to MQTT.
 * 
 * This function loads all relay units from NVS using `get_all_relay_units()`, 
 * generates the NVS key for each relay, and then publishes the relay's state 
 * to MQTT using `trigger_mqtt_publish()`. The function also subscribes the relays
 * to command MQTT topics to allow settings the state. The function ensures proper error 
 * handling and memory management by freeing any dynamically allocated memory 
 * (like relay keys and relay list).
 * 
 * @return 
 *      - ESP_OK on success
 *      - ESP_FAIL if any step in the process fails
 */
esp_err_t relay_publish_all_to_mqtt() {
    relay_unit_t *relay_list = NULL;
    uint16_t total_count = 0;
    esp_err_t err;

    // Load all relay units
    err = get_all_relay_units(&relay_list, &total_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load relay units from NVS.");
        return err;
    }

    // Iterate through each relay and publish to MQTT
    for (uint16_t i = 0; i < total_count; i++) {
        // Get the NVS key for the relay
        char *relay_key = get_unit_nvs_key(&relay_list[i]);
        if (relay_key == NULL) {
            ESP_LOGE(TAG, "Failed to get NVS key for relay channel %d.", relay_list[i].channel);
            continue;  // Move to the next relay if key generation fails
        }

        // Trigger MQTT publish for each relay
        err = trigger_mqtt_publish(relay_key, relay_list[i].type);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to publish relay channel %d to MQTT.", relay_list[i].channel);
        }

        // subscribe relay unit to MQTT set topic
        err = mqtt_relay_subscribe(&relay_list[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to subscribe relay channel %d to MQTT.", relay_list[i].channel);
        }
    }

    // Free the dynamically allocated relay_list
    free(relay_list);

    return ESP_OK;
}
