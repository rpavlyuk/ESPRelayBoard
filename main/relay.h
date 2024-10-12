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


relay_unit_t get_actuator_relay(int channel, int pin);
relay_unit_t get_sensor_relay(int channel, int pin);

esp_err_t save_relay_to_nvs(const char *key, relay_unit_t *relay);
esp_err_t load_relay_actuator_from_nvs(const char *key, relay_unit_t *relay);
esp_err_t load_relay_sensor_from_nvs(const char *key, relay_unit_t *relay);

char *get_relay_nvs_key(int channel);

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


#endif
