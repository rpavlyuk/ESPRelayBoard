#ifndef MQTT_H
#define MQTT_H

#include "common.h"
#include "mqtt_client.h"
#include "relay.h"

/**
 * @brief: MQTT connection mode for the device
 */
typedef enum {
    MQTT_CONN_MODE_DISABLE,           // soft disable MQTT
    MQTT_CONN_MODE_NO_RECONNECT,      // connect initially to MQTT, but do NOT reconnect
    MQTT_CONN_MODE_AUTOCONNECT,       // connect initially to MQTT and reconnect when lost
} mqtt_connection_mode_t;

/**
 * @brief: Event data used to communicate between MQTT publishing event queue and other tasks
 */
typedef struct {
    char *relay_key;            // Relay key (dynamically allocated or stored)
    relay_type_t relay_type;    // Type of relay (e.g., actuator or sensor)
} relay_event_t;

/**
 * @brief: Event data used to communicate between MQTT subscription event queue and other tasks
 */
typedef struct {
    char *relay_key;
    relay_state_t state;
} mqtt_command_event_t;


// Define the SPIFFS configuration
#define CA_CERT_PATH "/spiffs/ca.crt"

#define MQTT_QUEUE_LENGTH 10  // Number of items the queue can hold

extern bool g_mqtt_ready;

static void log_error_if_nonzero(const char *message, int error_code);

esp_err_t start_mqtt_queue_task(void);

void mqtt_event_task(void *arg);

esp_err_t trigger_mqtt_publish(const char *relay_key, relay_type_t relay_type);

// load CA certification from the filesystem
esp_err_t load_ca_certificate(char **ca_cert);

// save CA certification to the filesystem
esp_err_t save_ca_certificate(const char *ca_cert);

// init MQTT connection
esp_err_t mqtt_init(void);

// Call this function when you are shutting down the application or no longer need the MQTT client
void cleanup_mqtt();

// publish relay to MQTT
esp_err_t mqtt_publish_relay_data(const relay_unit_t *relay);

esp_err_t mqtt_publish_home_assistant_config(const char *device_id, const char *mqtt_prefix, const char *homeassistant_prefix);
void mqtt_device_config_task(void *param);

void mqtt_subscribe_relays_task(void *arg);
relay_unit_t *resolve_relay_from_topic(const char *topic);
char *resolve_key_from_topic(const char *topic);
char *get_element_from_path(const char *path, int index);
char** str_split(char* a_str, const char a_delim, size_t *element_count);

esp_err_t mqtt_relay_subscribe(relay_unit_t *relay);

#endif // MQTT_H
