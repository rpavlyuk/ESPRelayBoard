#ifndef RELAY_H
#define RELAY_H

#include "driver/gpio.h"

/**
 * @brief: Relay state (ON or OFF)
 */
typedef enum {
    RELAY_STATE_ON,
    RELAY_STATE_OFF
} relay_state_t;

/**
 * @brief: Relay type
 */
typedef enum {
    RELAY_TYPE_ACTUATOR,        // A state-actuated relay switch
    RELAY_TYPE_SENSOR           // A contact sensor: determining if contact is closed or open
} relay_type_t;

/**
 * @brief: Relay channel definition
 */
typedef struct {
    int channel;                // Relay channel. For example, from 1 to 4 in 4-channel board
    relay_state_t state;        // ON or OFF
    bool inverted;              // Inverted actuation. GPIO_LOW for enverted.
    int gpio_pin;               // GPIO pin
    bool enabled;               // Enable the channel
    relay_type_t type;          // Relay type
    gpio_config_t *io_conf;     // GPIO IO configuration
} relay_unit_t;

#define SAFE_GPIO_COUNT 18
extern const int SAFE_GPIO_PINS[SAFE_GPIO_COUNT];

bool is_gpio_safe(int gpio_pin);
bool is_gpio_pin_in_use(int pin);
int get_next_available_safe_gpio_pin();
void populate_safe_gpio_pins(char *buffer, size_t buffer_size);

relay_unit_t get_actuator_relay(int channel, int pin);
relay_unit_t get_sensor_relay(int channel, int pin);

esp_err_t save_relay_to_nvs(const char *key, relay_unit_t *relay);
esp_err_t load_relay_actuator_from_nvs(const char *key, relay_unit_t *relay);
esp_err_t load_relay_sensor_from_nvs(const char *key, relay_unit_t *relay);

char *get_relay_nvs_key(int channel);
char *get_contact_sensor_nvs_key(int channel);

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
char* serialize_relay_unit(const relay_unit_t *relay);

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
esp_err_t deserialize_relay_unit(const char *json_str, relay_unit_t *relay);

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
esp_err_t get_relay_list(relay_unit_t **relay_list, uint16_t *count);

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
esp_err_t get_contact_sensor_list(relay_unit_t **sensor_list, uint16_t *count);

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
esp_err_t get_all_relay_units(relay_unit_t **relay_list, uint16_t *total_count);


/**
 * @brief Frees memory allocated for relay_unit_t array, including dynamically allocated io_conf field.
 *
 * @param relay_list Pointer to an array of relay_unit_t.
 * @param count Number of elements in the relay_list array.
 * @return
 *     - ESP_OK: Successfully freed all memory.
 *     - ESP_ERR_INVALID_ARG: relay_list is NULL.
 */
esp_err_t free_relays_array(relay_unit_t *relay_list, size_t count);


#endif
