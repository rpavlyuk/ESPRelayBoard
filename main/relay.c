#include "driver/gpio.h"
#include "esp_log.h"

#include "cJSON.h"

#include "non_volatile_storage.h"

#include "common.h"
#include "settings.h"
#include "relay.h"

const int SAFE_GPIO_PINS[SAFE_GPIO_COUNT] = {4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 26, 27, 28, 29, 30, 31};

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
    relay.type = RELAY_TYPE_ACTUATOR;

    // Allocate memory for io_conf
    gpio_config_t *io_conf = malloc(sizeof(gpio_config_t));
    if (io_conf == NULL) {
        // Use ESP-IDF logging to handle memory allocation failure
        ESP_LOGE(TAG, "Failed to allocate memory for gpio_config_t");
        return relay; // Return an uninitialized relay object
    }

    // Initialize gpio_config_t properties
    io_conf->pin_bit_mask = (1ULL << pin);  // Set the pin bit mask for the specified GPIO pin
    io_conf->mode = GPIO_MODE_OUTPUT;       // Set as output mode
    io_conf->pull_up_en = GPIO_PULLUP_DISABLE; // No internal pull-up
    io_conf->pull_down_en = GPIO_PULLDOWN_DISABLE; // No internal pull-down
    io_conf->intr_type = GPIO_INTR_DISABLE; // No interrupt

    // Apply the configuration
    esp_err_t err = gpio_config(io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed for pin %d, error: %s", pin, esp_err_to_name(err));
        free(io_conf); // Clean up allocated memory if configuration fails
        return relay;  // Return an uninitialized relay object
    }

    // Assign the io_conf pointer to relay struct
    relay.io_conf = io_conf;

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
    relay.type = RELAY_TYPE_SENSOR;

    // Allocate memory for io_conf
    gpio_config_t *io_conf = malloc(sizeof(gpio_config_t));
    if (io_conf == NULL) {
        // Use ESP-IDF logging to handle memory allocation failure
        ESP_LOGE(TAG, "Failed to allocate memory for gpio_config_t");
        return relay; // Return an uninitialized relay object
    }

    // Initialize gpio_config_t properties for input with pull-up
    io_conf->pin_bit_mask = (1ULL << pin);     // Set the pin bit mask for the specified GPIO pin
    io_conf->mode = GPIO_MODE_INPUT;           // Set as input mode
    io_conf->pull_up_en = GPIO_PULLUP_ENABLE;  // Enable internal pull-up
    io_conf->pull_down_en = GPIO_PULLDOWN_DISABLE; // No internal pull-down
    io_conf->intr_type = GPIO_INTR_DISABLE;    // No interrupt

    // Apply the configuration
    esp_err_t err = gpio_config(io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed for pin %d, error: %s", pin, esp_err_to_name(err));
        free(io_conf); // Clean up allocated memory if configuration fails
        return relay;  // Return an uninitialized relay object
    }

    // Assign the io_conf pointer to relay struct
    relay.io_conf = io_conf;

    ESP_LOGI(TAG, "Contact sensor initialized on channel %d, GPIO pin %d", channel, pin);
    return relay;
}

/**
 * @brief Save a relay unit to NVS
 *
 * @param key NVS key
 * @param relay Pointer to the relay unit to be saved
 * @return esp_err_t result of the NVS operation
 */
esp_err_t save_relay_to_nvs(const char *key, relay_unit_t *relay) {
    esp_err_t err = nvs_write_blob(S_NAMESPACE, key, relay, sizeof(relay_unit_t));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Relay successfully saved to NVS under key: %s", key);
    } else {
        ESP_LOGE(TAG, "Failed to save relay to NVS: %s", esp_err_to_name(err));
    }
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

    // Initialize io_conf for the actuator relay
    relay->io_conf = malloc(sizeof(gpio_config_t));
    if (relay->io_conf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for gpio_config_t");
        return ESP_ERR_NO_MEM;
    }

    // Configure the GPIO as an output for the actuator
    relay->io_conf->pin_bit_mask = (1ULL << relay->gpio_pin);
    relay->io_conf->mode = GPIO_MODE_OUTPUT;
    relay->io_conf->pull_up_en = GPIO_PULLUP_DISABLE;
    relay->io_conf->pull_down_en = GPIO_PULLDOWN_DISABLE;
    relay->io_conf->intr_type = GPIO_INTR_DISABLE;

    // Apply the configuration
    err = gpio_config(relay->io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed for pin %d, error: %s", relay->gpio_pin, esp_err_to_name(err));
        free(relay->io_conf);
        return err;
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

    // Initialize io_conf for the sensor relay
    relay->io_conf = malloc(sizeof(gpio_config_t));
    if (relay->io_conf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for gpio_config_t");
        return ESP_ERR_NO_MEM;
    }

    // Configure the GPIO as an input with pull-up for the sensor
    relay->io_conf->pin_bit_mask = (1ULL << relay->gpio_pin);
    relay->io_conf->mode = GPIO_MODE_INPUT;
    relay->io_conf->pull_up_en = GPIO_PULLUP_ENABLE;
    relay->io_conf->pull_down_en = GPIO_PULLDOWN_DISABLE;
    relay->io_conf->intr_type = GPIO_INTR_DISABLE;

    // Apply the configuration
    err = gpio_config(relay->io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed for pin %d, error: %s", relay->gpio_pin, esp_err_to_name(err));
        free(relay->io_conf);
        return err;
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

// Serialization function: Converts relay_unit_t to a JSON string
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
    char *json_string = cJSON_Print(relay_json);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to convert relay JSON object to string");
    }

    // Free the JSON object and relay_key
    cJSON_Delete(relay_json);
    free(relay_key);

    return json_string;
}

// Deserialization function: Converts a JSON string to relay_unit_t
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
        ESP_ERROR_CHECK(free_relays_array(actuators, actuator_count));
        ESP_ERROR_CHECK(free_relays_array(sensors, sensor_count));
        return ESP_ERR_NO_MEM;
    }

    // Copy actuators to the combined list
    if (actuator_count > 0) {
        memcpy(*relay_list, actuators, sizeof(relay_unit_t) * actuator_count);
        ESP_ERROR_CHECK(free_relays_array(actuators, actuator_count));
    }

    // Copy sensors to the combined list after actuators
    if (sensor_count > 0) {
        memcpy(*relay_list + actuator_count, sensors, sizeof(relay_unit_t) * sensor_count);
        ESP_ERROR_CHECK(free_relays_array(sensors, sensor_count));
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
        if (relay_list[i].io_conf != NULL) {
            ESP_LOGI(TAG, "Releasing dynamic memory for unit channel (%d) and type (%d)", relay_list[i].channel, (int)relay_list[i].type);
            free(relay_list[i].io_conf);  // Free the GPIO configuration for this relay
            relay_list[i].io_conf = NULL; // Set pointer to NULL to avoid dangling references
        }
    }

    // Free the relay list itself
    free(relay_list);

    return ESP_OK;  // Indicate success
}



