#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "non_volatile_storage.h"
#include "mqtt_client.h"
#include "esp_check.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "mqtt.h"
#include "settings.h"
#include "wifi.h"
#include "relay.h"  // To access relay data
#include "hass.h"

static QueueHandle_t mqtt_event_queue = NULL; 

esp_mqtt_client_handle_t mqtt_client = NULL;
bool mqtt_connected = false;
bool g_mqtt_ready = false;

/**
 * @brief Starts the MQTT event queue task.
 * 
 * This function creates the MQTT event queue and starts the task responsible for 
 * handling MQTT publishing requests. The task monitors the queue for events and 
 * processes them by loading the necessary relay data from NVS and publishing the 
 * state to MQTT.
 * 
 * @return 
 *      - ESP_OK on success
 *      - ESP_FAIL if the queue or task creation fails
 */
esp_err_t start_mqtt_queue_task(void) {

// Create the event queue
    mqtt_event_queue = xQueueCreate(MQTT_QUEUE_LENGTH, sizeof(relay_event_t));
    if (mqtt_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue for MQTT");
        return ESP_FAIL;
    }

    // Start the MQTT event task
    xTaskCreate(mqtt_event_task, "mqtt_event_task", 8192, NULL, 5, NULL);

    return ESP_OK;
}

/**
 * @brief FreeRTOS task to handle MQTT relay publish events.
 * 
 * This task monitors an event queue for relay publish requests. When an event is 
 * received, it loads the relay data from NVS based on the event type (actuator or 
 * contact sensor), and then publishes the state to MQTT using the 
 * mqtt_publish_relay_data() function.
 * 
 * @param[in] arg Unused task argument.
 */
void mqtt_event_task(void *arg) {
    relay_event_t event;
    relay_unit_t *relay = NULL;
    esp_err_t err;

    while (1) {
        // Wait for events to arrive in the queue
        if (xQueueReceive(mqtt_event_queue, &event, portMAX_DELAY)) {
            
            ESP_LOGI(TAG, "mqtt_event_task: Recevied MQTT publish message. Key (%s), type (%d)", event.relay_key, event.relay_type);

            // Allocate memory for relay
            relay = (relay_unit_t *)malloc(sizeof(relay_unit_t));
            if (relay == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for relay_unit_t");
                free(event.relay_key);  // Clean up
                continue;               // Skip this iteration
            }

            // Load relay based on event type
            if (event.relay_type == RELAY_TYPE_ACTUATOR) {
                err = load_relay_actuator_from_nvs(event.relay_key, relay);
            } else {
                err = load_relay_sensor_from_nvs(event.relay_key, relay);
            }

            if (err == ESP_OK && relay != NULL) {
                // Publish the relay state to MQTT
                mqtt_publish_relay_data(relay);

                // Free relay memory if dynamically allocated
                if (relay->type == RELAY_TYPE_ACTUATOR) {
                    if (INIT_RELAY_ON_LOAD) {
                        ESP_ERROR_CHECK(relay_gpio_deinit(relay));
                    }
                } else {
                    if (INIT_SENSORS_ON_LOAD) {
                        ESP_ERROR_CHECK(relay_gpio_deinit(relay));
                    }
                }
                free(relay);
            } else {
                ESP_LOGE(TAG, "Failed to load relay from NVS for key %s", event.relay_key);
            }

            // Free the dynamically allocated relay_key
            free(event.relay_key);
        }
    }
}

/**
 * @brief Sends a relay publish event to the MQTT queue.
 * 
 * This function triggers an MQTT publish by sending a relay event to the 
 * mqtt_event_queue. The event includes the relay key and type (actuator or contact sensor).
 * The event will be processed by the MQTT event task, which will load the relay data 
 * from NVS and publish its state to MQTT.
 * 
 * @param[in] relay_key The key identifying the relay in NVS.
 * @param[in] relay_type The type of relay (e.g., actuator or sensor).
 * 
 * @return 
 *      - ESP_OK on success
 *      - ESP_FAIL if the event cannot be sent to the queue
 */
esp_err_t  trigger_mqtt_publish(const char *relay_key, relay_type_t relay_type) {
    relay_event_t event;

    // Dynamically allocate memory for the relay_key and copy the key
    event.relay_key = strdup(relay_key);
    if (event.relay_key == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for relay_key");
        return ESP_FAIL;
    }
    
    event.relay_type = relay_type;

    ESP_LOGI(TAG, "trigger_mqtt_publish: +-> Pushing MQTT publish event to the queue. Key (%s), type(%d)", event.relay_key, (int)event.relay_type);

    // Send the event to the MQTT event task
    if (xQueueSend(mqtt_event_queue, &event, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send event to MQTT queue");
        free(event.relay_key);  // Free relay_key if event send fails
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

esp_err_t load_ca_certificate(char **ca_cert)
{
    FILE *f = fopen(CA_CERT_PATH, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open CA certificate file");
        return ESP_FAIL;
    }

    // Seek to the end to find the file size
    fseek(f, 0, SEEK_END);
    long cert_size = ftell(f);
    fseek(f, 0, SEEK_SET);  // Go back to the beginning of the file

    if (cert_size <= 0) {
        ESP_LOGE(TAG, "Invalid CA certificate file size");
        fclose(f);
        return ESP_FAIL;
    }

    // Allocate memory for the certificate
    *ca_cert = (char *) malloc(cert_size + 1);  // +1 for the null terminator
    if (*ca_cert == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for CA certificate");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    // Read the certificate into the buffer
    fread(*ca_cert, 1, cert_size, f);
    (*ca_cert)[cert_size] = '\0';  // Null-terminate the string

    fclose(f);
    ESP_LOGI(TAG, "Successfully loaded CA certificate");

    return ESP_OK;
}

esp_err_t save_ca_certificate(const char *ca_cert)
{
    if (ca_cert == NULL) {
        ESP_LOGE(TAG, "CA certificate data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(CA_CERT_PATH, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open CA certificate file for writing");
        return ESP_FAIL;
    }

    // Write the certificate to the file
    size_t written = fwrite(ca_cert, 1, strlen(ca_cert), f);
    if (written != strlen(ca_cert)) {
        ESP_LOGE(TAG, "Failed to write entire CA certificate to file");
        fclose(f);
        return ESP_FAIL;
    }

    fclose(f);
    ESP_LOGI(TAG, "Successfully saved CA certificate");

    return ESP_OK;
}

/**
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt_connected = true;  // Set flag when connected
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected = false;  // Reset flag when disconnected
        cleanup_mqtt();  // Ensure proper cleanup on disconnection
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        mqtt_connected = false;  // Handle connection error
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}


// Function to initialize the MQTT client
esp_err_t mqtt_init(void) {
    
    uint16_t mqtt_connection_mode;
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));
    if (mqtt_connection_mode < (uint16_t)MQTT_CONN_MODE_NO_RECONNECT) {
        ESP_LOGW(TAG, "MQTT disabled in device settings. Publishing skipped.");
        return ESP_OK; // not an issue
    }

    // wait for Wi-Fi to connect
    while(!g_wifi_ready) {
        ESP_LOGI(TAG, "Waiting for Wi-Fi/network to become ready...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Proceed with MQTT connection
    char *mqtt_server = NULL;
    char *mqtt_protocol = NULL;
    char *mqtt_user = NULL;
    char *mqtt_password = NULL;
    char *mqtt_prefix = NULL;
    char *device_id = NULL;

    uint16_t mqtt_port;

    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_SERVER, &mqtt_server));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, &mqtt_port));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, &mqtt_protocol));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_USER, &mqtt_user));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, &mqtt_password));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));

    char broker_url[256];
    snprintf(broker_url, sizeof(broker_url), "%s://%s:%d", mqtt_protocol, mqtt_server, mqtt_port);
    ESP_LOGI(TAG, "MQTT Broker URL: %s", broker_url);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_url,
        .network.timeout_ms = 5000,  // Increase timeout if needed
    };

    if (mqtt_user[0]) {
        mqtt_cfg.credentials.username = mqtt_user;
    }
    if (mqtt_password[0]) {
        mqtt_cfg.credentials.authentication.password = mqtt_password;
    }
    if (strcmp(mqtt_protocol,"mqtts") == 0) {
        // Load the CA certificate
        char *ca_cert = NULL;
        if (load_ca_certificate(&ca_cert) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to load CA certificate");
            if (strcmp(mqtt_protocol,"mqtts") == 0) {
                ESP_LOGE(TAG, "MQTTS protocol cannot be managed without CA certificate.");
                return ESP_FAIL;
            }
        } else {
            ESP_LOGI(TAG, "Loaded CA certificate: %s", CA_CERT_PATH);
        }
        if (ca_cert) {
            mqtt_cfg.broker.verification.certificate = ca_cert;
        }
    }

    esp_err_t ret;
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ret = esp_mqtt_client_start(mqtt_client);
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(device_id);

    return ret;
}


// Call this function when you are shutting down the application or no longer need the MQTT client
void cleanup_mqtt() {
    if (mqtt_client) {
        mqtt_connected = false;
        ESP_RETURN_VOID_ON_ERROR(esp_mqtt_client_stop(mqtt_client), TAG, "Failed to stop the MQTT client");
        ESP_RETURN_VOID_ON_ERROR(esp_mqtt_client_destroy(mqtt_client), TAG, "Failed to destroy the MQTT client");  // Free the resources
        mqtt_client = NULL;
    }
}

// Publish relay state to MQTT
esp_err_t mqtt_publish_relay_data(const relay_unit_t *relay) {

    dump_current_task();

    if (relay == NULL) {
        ESP_LOGE(TAG, "Got NULL as relay in mqtt_publish_relay_data.");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Publish relay/sensor information to MQTT. Channel (%i), type (%i)", relay->channel, relay->type);

    uint16_t mqtt_connection_mode;
    esp_err_t err;

    // Read MQTT connection mode
    err = nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT connection mode from NVS");
        return ESP_FAIL;
    }

    // Check if MQTT is disabled in the device settings
    if (mqtt_connection_mode < (uint16_t)MQTT_CONN_MODE_NO_RECONNECT) {
        ESP_LOGW(TAG, "MQTT disabled in device settings. Publishing skipped.");
        return ESP_OK;
    }

    // Ensure MQTT client is initialized and connected
    if (mqtt_client == NULL || !mqtt_connected) {
        ESP_LOGW(TAG, "MQTT client is not initialized or not connected.");
        if (mqtt_connection_mode > (uint16_t)MQTT_CONN_MODE_NO_RECONNECT) {
            ESP_LOGI(TAG, "Restoring connection to MQTT...");
            if (mqtt_init() != ESP_OK) {
                ESP_LOGE(TAG, "MQTT client re-init failed. Will not publish any data to MQTT.");
                return ESP_FAIL;
            }
        } else {
            ESP_LOGW(TAG, "Re-connect disabled by MQTT mode setting. Visit device WEB interface to adjust it.");
            return ESP_FAIL;
        }
    }

    /* Declare NULL pointer for string variables */
    char *mqtt_prefix = NULL;
    char *device_id = NULL;
    char *relay_key = NULL;

    // Read MQTT prefix and device ID from NVS
    err = nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix);
    if (err != ESP_OK || mqtt_prefix == NULL) {
        ESP_LOGE(TAG, "Failed to read MQTT prefix from NVS or NULL prefix");
        return ESP_ERR_NVS_BASE;
    }

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id);
    if (err != ESP_OK || device_id == NULL) {
        ESP_LOGE(TAG, "Failed to read device ID from NVS or NULL device ID");
        free(mqtt_prefix);  // Ensure mqtt_prefix is freed before returning
        return ESP_ERR_NVS_BASE;
    }

    // Get relay_key
    relay_key = get_unit_nvs_key(relay);
    if (relay_key == NULL) {
        ESP_LOGE(TAG, "Failed to get relay key for channel %d", relay->channel);
        free(mqtt_prefix);
        free(device_id);
        return ESP_ERR_INVALID_ARG;
    }

    // Create MQTT topics
    char topic[256]; 

    // Publish each sensor data field separately
    int msg_id;
    bool is_error = false;
    char value[32];

    // Publish state
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s/state", mqtt_prefix, device_id, relay_key, (relay->type == RELAY_TYPE_ACTUATOR)?HA_DEVICE_STATE_PATH_RELAY:HA_DEVICE_STATE_PATH_SENSOR);
    snprintf(value, sizeof(value), "%i", (int)relay->state);
    ESP_LOGI(TAG, "mqtt_publish_relay_data: Publish value (%s) to topic (%s)", value, topic);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic);
        is_error = true;
    }

    // Publish channel
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s/channel", mqtt_prefix, device_id, relay_key, (relay->type == RELAY_TYPE_ACTUATOR)?HA_DEVICE_STATE_PATH_RELAY:HA_DEVICE_STATE_PATH_SENSOR);
    snprintf(value, sizeof(value), "%i", (int)relay->channel);
    ESP_LOGI(TAG, "mqtt_publish_relay_data: Publish value (%s) to topic (%s)", value, topic);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic);
        is_error = true;
    }

    // Publish inverted
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s/inverted", mqtt_prefix, device_id, relay_key, (relay->type == RELAY_TYPE_ACTUATOR)?HA_DEVICE_STATE_PATH_RELAY:HA_DEVICE_STATE_PATH_SENSOR);
    snprintf(value, sizeof(value), "%i", (int)relay->inverted);
    ESP_LOGI(TAG, "mqtt_publish_relay_data: Publish value (%s) to topic (%s)", value, topic);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic);
        is_error = true;
    }

    // Publish gpio_pin
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s/gpio_pin", mqtt_prefix, device_id, relay_key, (relay->type == RELAY_TYPE_ACTUATOR)?HA_DEVICE_STATE_PATH_RELAY:HA_DEVICE_STATE_PATH_SENSOR);
    snprintf(value, sizeof(value), "%i", (int)relay->gpio_pin);
    ESP_LOGI(TAG, "mqtt_publish_relay_data: Publish value (%s) to topic (%s)", value, topic);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic);
        is_error = true;
    }

    // Publish enabled
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s/enabled", mqtt_prefix, device_id, relay_key, (relay->type == RELAY_TYPE_ACTUATOR)?HA_DEVICE_STATE_PATH_RELAY:HA_DEVICE_STATE_PATH_SENSOR);
    snprintf(value, sizeof(value), "%i", (int)relay->enabled);
    ESP_LOGI(TAG, "mqtt_publish_relay_data: Publish value (%s) to topic (%s)", value, topic);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic);
        is_error = true;
    }

    // Publish type
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s/type", mqtt_prefix, device_id, relay_key, (relay->type == RELAY_TYPE_ACTUATOR)?HA_DEVICE_STATE_PATH_RELAY:HA_DEVICE_STATE_PATH_SENSOR);
    snprintf(value, sizeof(value), "%i", (int)relay->type);
    ESP_LOGI(TAG, "mqtt_publish_relay_data: Publish value (%s) to topic (%s)", value, topic);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic);
        is_error = true;
    }

    // process JSON status
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s", mqtt_prefix, device_id, relay_key, (relay->type == RELAY_TYPE_ACTUATOR)?HA_DEVICE_STATE_PATH_RELAY:HA_DEVICE_STATE_PATH_SENSOR);
    char *relay_json = serialize_relay_unit(relay);
    if (relay_json != NULL) {
        ESP_LOGI(TAG, "mqtt_publish_relay_data: Publish value (%s) to topic (%s)", relay_json, topic);
        msg_id = esp_mqtt_client_publish(mqtt_client, topic, relay_json, 0, 1, 0);
        if (msg_id < 0) {
            ESP_LOGW(TAG, "Topic %s not published", topic);
            is_error = true;
        }
    } else {
        ESP_LOGW(TAG, "Get NULL when tried serialize_relay_unit(). Relay's JSON data will not be publised to MQTT.");
        is_error = true;
    }

    // Free allocated resources for MQTT prefix and device ID
    free(mqtt_prefix);
    free(device_id);

    if (is_error) {
        ESP_LOGE(TAG, "There were errors when publishing relay data to MQTT");
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "MQTT relay data published successfully.");
        return ESP_OK;
    }
}