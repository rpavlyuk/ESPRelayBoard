#ifndef RELAY_H
#define RELAY_H

#include "driver/gpio.h"


/** TYPES **/

/**
 * @brief: Relay state (ON or OFF)
 */
typedef enum {
    RELAY_STATE_OFF,
    RELAY_STATE_ON
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
    bool gpio_initialized;      // GPIO initialized
    gpio_config_t io_conf;     // GPIO IO configuration
} relay_unit_t;

// Event type for GPIO events
typedef struct {
    int gpio_num;  // The GPIO pin number that triggered the event
    int level;     // The level (state) of the GPIO pin (0 or 1)
} gpio_event_t;

/** SETTINGS AND CONSTANTS **/

#define INIT_RELAY_ON_LOAD     false
#define INIT_RELAY_ON_GET      false

#define INIT_SENSORS_ON_LOAD     true
#define INIT_SENSORS_ON_GET      true

#define SAFE_GPIO_COUNT 26
extern const int SAFE_GPIO_PINS[SAFE_GPIO_COUNT];

#define DEBOUNCE_TIME_MS 50  // Set the debounce time to 50 milliseconds (adjust as needed)

/** ROUTINES **/
esp_err_t init_relay_units_in_memory();
esp_err_t dump_relay_units_in_memory();

bool is_gpio_safe(int gpio_pin);
bool is_gpio_pin_in_use(int pin);
int get_next_available_safe_gpio_pin();
void populate_safe_gpio_pins(char *buffer, size_t buffer_size);

relay_unit_t get_actuator_relay(int channel, int pin);
relay_unit_t get_sensor_relay(int channel, int pin);

esp_err_t relay_gpio_init(relay_unit_t *relay);
esp_err_t relay_gpio_deinit(relay_unit_t *relay);
esp_err_t relay_sensor_register_isr(relay_unit_t *relay);
esp_err_t relay_sensor_gpio_state_refresh(relay_unit_t *relay);

esp_err_t relay_set_state(relay_unit_t *relay, relay_state_t state, bool persist);

void gpio_isr_handler(void *arg);
void gpio_event_task(void *arg);

esp_err_t save_relay_to_nvs(const char *key, relay_unit_t *relay);
esp_err_t load_relay_actuator_from_nvs(const char *key, relay_unit_t *relay);
esp_err_t load_relay_sensor_from_nvs(const char *key, relay_unit_t *relay);

esp_err_t get_relay_actuator_from_memory_by_channel(int channel, relay_unit_t **relay);
esp_err_t get_relay_sensor_from_memory_by_channel(int channel, relay_unit_t **relay);

esp_err_t get_relay_actuator_from_memory_by_key(const char *key, relay_unit_t **relay);
esp_err_t get_relay_sensor_from_memory_by_key(const char *key, relay_unit_t **relay);

char *get_relay_nvs_key(int channel);
char *get_contact_sensor_nvs_key(int channel);
char *get_unit_nvs_key(const relay_unit_t *relay);

relay_type_t get_relay_type_from_key(const char *relay_key);

char* serialize_relay_unit(const relay_unit_t *relay);
esp_err_t deserialize_relay_unit(const char *json_str, relay_unit_t *relay);

esp_err_t get_relay_list(relay_unit_t **relay_list, uint16_t *count);
esp_err_t get_contact_sensor_list(relay_unit_t **sensor_list, uint16_t *count);
esp_err_t get_all_relay_units(relay_unit_t **relay_list, uint16_t *total_count);

esp_err_t relay_all_sensors_register_isr();
esp_err_t free_relays_array(relay_unit_t *relay_list, size_t count);

esp_err_t relay_publish_all_to_mqtt(bool subscribe);
void refresh_relay_states_2_mqtt_task(void *arg);

#endif
