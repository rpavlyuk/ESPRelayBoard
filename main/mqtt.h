#ifndef MQTT_H
#define MQTT_H

#include "common.h"
#include "mqtt_client.h"

typedef enum {
    MQTT_CONN_MODE_DISABLE,           // soft disable MQTT
    MQTT_CONN_MODE_NO_RECONNECT,      // connect initially to MQTT, but do NOT reconnect
    MQTT_CONN_MODE_AUTOCONNECT,       // connect initially to MQTT and reconnect when lost
} mqtt_connection_mode_t;

// Define the SPIFFS configuration
#define CA_CERT_PATH "/spiffs/ca.crt"

static void log_error_if_nonzero(const char *message, int error_code);

// load CA certification from the filesystem
esp_err_t load_ca_certificate(char **ca_cert);

// save CA certification to the filesystem
esp_err_t save_ca_certificate(const char *ca_cert);

// Call this function when you are shutting down the application or no longer need the MQTT client
void cleanup_mqtt();

#endif // MQTT_H
