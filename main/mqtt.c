#include "freertos/FreeRTOS.h"   // must be first
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"      // if you use queues

#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "non_volatile_storage.h"
#include "nvs_large.h"
#include "ca_cert_manager.h"
#include "mqtt_client.h"
#include "esp_check.h"

#include "cJSON.h"

#include "flags.h"
#include "mqtt.h"
#include "settings.h"
#include "wifi.h"
#include "relay.h"  // To access relay data
#include "hass.h"
#include "status.h"

/* Queue global variables */
static QueueHandle_t mqtt_event_queue = NULL; 
static QueueHandle_t mqtt_command_queue = NULL;

/* MQTT client global variables */
esp_mqtt_client_handle_t mqtt_client = NULL;

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

    /*
    ESP_LOGI(TAG, "+ + + + Waiting for MQTT connection to become ready...");


    EventBits_t bits = xEventGroupWaitBits(
        g_sys_events,             // Event group handle
        BIT_MQTT_READY,           // Bit(s) to wait for
        pdFALSE,                  // Don’t clear bit on exit
        pdTRUE,                   // Wait until *all* bits are set (only one here)
        pdMS_TO_TICKS(60000)      // Timeout after 60 seconds
    );

    if (bits & BIT_MQTT_READY) {
        ESP_LOGI(TAG, "MQTT connection is ready!");
        // proceed with normal operation
    } else {
        ESP_LOGE(TAG, "Timeout waiting for MQTT to become ready");
        return ESP_FAIL;
    }
    */

    // Create the event queue
    mqtt_event_queue = xQueueCreate(MQTT_QUEUE_LENGTH, sizeof(relay_event_t));
    if (mqtt_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue for MQTT");
        return ESP_FAIL;
    }
    // Start the MQTT event task
    xTaskCreate(mqtt_event_task, "mqtt_event_task", 8192, NULL, 5, NULL);


    // mqtt_subscribe_relays_task
    mqtt_command_queue = xQueueCreate(MQTT_QUEUE_LENGTH, sizeof(mqtt_command_event_t));
    if (mqtt_command_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue for MQTT command subscriptions");
        return ESP_FAIL;
    }
    // Start the MQTT command subscription
    xTaskCreate(mqtt_subscribe_relays_task, "mqtt_subscribe_relays_task", 8192, NULL, 5, NULL);

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
    relay_unit_t relay;
    esp_err_t err;

    while (1) {
        // Wait for events to arrive in the queue
        if (xQueueReceive(mqtt_event_queue, &event, portMAX_DELAY)) {
            
            ESP_LOGI(TAG, "mqtt_event_task: Recevied MQTT publish message. Key (%s), type (%d)", event.relay_key, event.relay_type);

            // Load relay based on event type
            if (event.relay_type == RELAY_TYPE_ACTUATOR) {
                err = load_relay_actuator_from_nvs(event.relay_key, &relay);
            } else {
                err = load_relay_sensor_from_nvs(event.relay_key, &relay);
            }

            if (err == ESP_OK) {
                // Publish the relay state to MQTT
                mqtt_publish_relay_data(&relay);

                // Free relay memory if dynamically allocated
                if (relay.type == RELAY_TYPE_ACTUATOR) {
                    if (INIT_RELAY_ON_LOAD) {
                        ESP_ERROR_CHECK(relay_gpio_deinit(&relay));
                    }
                } else {
                    if (INIT_SENSORS_ON_LOAD) {
                        ESP_ERROR_CHECK(relay_gpio_deinit(&relay));
                    }
                }
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

    event.relay_key = relay_key;
    event.relay_type = relay_type;

    ESP_LOGI(TAG, "trigger_mqtt_publish: +-> Pushing MQTT publish event to the queue. Key (%s), type(%d)", event.relay_key, (int)event.relay_type);

    // Send the event to the MQTT event task
    if (xQueueSend(mqtt_event_queue, &event, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send event to MQTT queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief: Log an error message if the error code is non-zero.
 * 
 * This function is used to log an error message if the error code is non-zero.
 * 
 * @param[in] message The message to log.
 * @param[in] error_code The error code to check.
 * 
 * @return void
 */
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
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
        xEventGroupSetBits(g_sys_events, BIT_MQTT_CONNECTED);
        xEventGroupSetBits(g_sys_events, BIT_MQTT_READY);
        // update all relays to MQTT
        ESP_ERROR_CHECK(relay_publish_all_to_mqtt(true));
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupClearBits(g_sys_events, BIT_MQTT_CONNECTED);
        xEventGroupClearBits(g_sys_events, BIT_MQTT_READY);
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
    case MQTT_EVENT_DATA: {
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        // TODO: check the code for heap memory leaks

        // Log the topic and data properly using the length
        ESP_LOGI(TAG, "TOPIC=%.*s, len: %i", event->topic_len, event->topic, event->topic_len);
        ESP_LOGI(TAG, "DATA=%.*s, len: %i", event->data_len, event->data, event->data_len);

        // Dynamically allocate memory for topic and data buffers
        char *topic_buf = (char *)malloc(event->topic_len + 1);  // +1 for the null terminator
        char *data_buf = (char *)malloc(event->data_len + 1);  // +1 for the null terminator

        if (topic_buf == NULL || data_buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for MQTT topic or data buffers");
            free(topic_buf);  // Ensure any allocated memory is freed
            free(data_buf);
            break;
        }

        // Copy the topic and data, ensuring they are null-terminated
        strncpy(topic_buf, event->topic, event->topic_len);
        topic_buf[event->topic_len] = '\0';  // Null-terminate the string

        strncpy(data_buf, event->data, event->data_len);
        data_buf[event->data_len] = '\0';  // Null-terminate the string

        // Log to ensure correct handling of topic and data
        ESP_LOGI(TAG, "MQTT_EVENT_DATA: Got MQTT topic to extract relay key from: %s", topic_buf);

        // Create the command event and resolve the relay key
        mqtt_command_event_t command_event;
        command_event.relay_key = resolve_key_from_topic(topic_buf);  // Use the topic buffer
        if (command_event.relay_key == NULL) {
            ESP_LOGE(TAG, "Failed to resolve relay key from topic %s (NULL)", topic_buf);
            break;  // Handle the error
        }

        // Handle state based on data buffer
        command_event.state = (strcmp(data_buf, "True") == 0 || strcmp(data_buf, "true") == 0) ? RELAY_STATE_ON : RELAY_STATE_OFF;

        // Send the event to the queue
        if (xQueueSend(mqtt_command_queue, &command_event, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "Failed to send MQTT command event to the queue");
        }
        break;
    }
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        xEventGroupClearBits(g_sys_events, BIT_MQTT_CONNECTED | BIT_MQTT_READY);
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}


/**
 * @brief Initialize the MQTT client.
 * 
 * This function initializes the MQTT client with the configuration parameters
 * stored in NVS. The function reads the MQTT server, port, protocol, username,
 * password, and prefix from NVS and uses them to configure the MQTT client.
 * 
 * @return esp_err_t    ESP_OK on success, ESP_FAIL if the MQTT client cannot be initialized.
 * 
 */
esp_err_t mqtt_init(void) {
    
    uint16_t mqtt_connection_mode;
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));
    if (mqtt_connection_mode < (uint16_t)MQTT_CONN_MODE_NO_RECONNECT) {
        ESP_LOGW(TAG, "MQTT disabled in device settings. Publishing skipped.");
        return ESP_OK; // not an issue
    }

    EventBits_t bits = xEventGroupWaitBits(
        g_sys_events,
        BIT_WIFI_CONNECTED,
        pdFALSE,                // don't clear
        pdTRUE,
        pdMS_TO_TICKS(30000)    // wait up to 30 seconds
    );

    if (bits & BIT_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi/network is ready!");
    } else {
        ESP_LOGW(TAG, "Timeout waiting for Wi-Fi to connect");
        return ESP_FAIL;
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
        if (load_ca_certificate(&ca_cert, CA_CERT_PATH_MQTTS) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to load CA certificate");
            if (strcmp(mqtt_protocol,"mqtts") == 0) {
                ESP_LOGE(TAG, "MQTTS protocol cannot be managed without CA certificate.");
                return ESP_FAIL;
            }
        } else {
            ESP_LOGI(TAG, "Loaded CA certificate: %s", CA_CERT_PATH_MQTTS);
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

    // 🟢 Wait up to 10 seconds for MQTT to become fully ready
    ESP_LOGI(TAG, "Waiting for MQTT client to connect...");

    bits = xEventGroupWaitBits(
        g_sys_events,
        BIT_MQTT_CONNECTED | BIT_MQTT_READY,  // wait for both
        pdFALSE,                              // don’t clear bits
        pdTRUE,                               // wait for *all* bits
        pdMS_TO_TICKS(10000)                  // timeout 10 seconds
    );

    if ((bits & (BIT_MQTT_CONNECTED | BIT_MQTT_READY)) ==
        (BIT_MQTT_CONNECTED | BIT_MQTT_READY)) {
        ESP_LOGI(TAG, "MQTT is connected and ready!");
    } else {
        ESP_LOGE(TAG, "Timeout waiting for MQTT to connect/initialize");
        ret = ESP_FAIL;
    }

    return ret;
}

/**
 * @brief Stop the MQTT client.
 * 
 * This function stops the MQTT client and frees the resources used by the client.
 * 
 * @return esp_err_t    ESP_OK on success, ESP_FAIL if the client cannot be stopped.
 */
esp_err_t mqtt_stop(void) {
    if (mqtt_client) {
        cleanup_mqtt();
    }
    return ESP_OK;
}

/**
 * @brief Cleanup the MQTT client.
 * 
 * This function stops and destroys the MQTT client, freeing the resources used by the client.
 * 
 */
void cleanup_mqtt() {
    if (mqtt_client) {
        xEventGroupClearBits(g_sys_events, BIT_MQTT_CONNECTED | BIT_MQTT_READY);
        ESP_RETURN_VOID_ON_ERROR(esp_mqtt_client_stop(mqtt_client), TAG, "Failed to stop the MQTT client");
        ESP_RETURN_VOID_ON_ERROR(esp_mqtt_client_destroy(mqtt_client), TAG, "Failed to destroy the MQTT client");  // Free the resources
        mqtt_client = NULL;
    }
}

/**
 * @brief Publish relay data to MQTT.
 * 
 * This function publishes the relay data to MQTT. The function reads the relay state,
 * channel, inverted, and GPIO pin from the relay structure and publishes the data to
 * the appropriate MQTT topics. The function also reads the MQTT prefix and device ID
 * from NVS to construct the MQTT topics.
 * 
 * @param[in] relay The relay data to publish.
 * 
 * @return esp_err_t    ESP_OK on success, ESP_FAIL if the data cannot be published.
 */
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
    dump_sys_bits("mqtt_publish_relay_data: Before MQTT client check");
    if (mqtt_client == NULL || !IS_MQTT_CONNECTED()) {
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
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 1, 1);
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
        msg_id = esp_mqtt_client_publish(mqtt_client, topic, relay_json, 0, 1, 1);
        if (msg_id < 0) {
            ESP_LOGW(TAG, "Topic %s not published", topic);
            is_error = true;
        }
        cJSON_free(relay_json);
    } else {
        ESP_LOGW(TAG, "Get NULL when tried serialize_relay_unit(). Relay's JSON data will not be publised to MQTT.");
        is_error = true;
    }

    // Free allocated resources for MQTT prefix and device ID
    free(mqtt_prefix);
    free(device_id);
    free(relay_key);

    if (is_error) {
        ESP_LOGE(TAG, "There were errors when publishing relay data to MQTT");
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "MQTT relay data published successfully.");
        return ESP_OK;
    }
}


/**
 * @brief Publish system information to MQTT.
 * 
 * This function publishes the system information to MQTT. The function reads the
 * device status from the provided structure and publishes the data to the appropriate
 * MQTT topics. The function also reads the MQTT prefix and device ID from NVS to
 * construct the MQTT topics.
 * 
 * @param[in] status The device status to publish.
 * 
 * @return esp_err_t    ESP_OK on success, ESP_FAIL if the data cannot be published.
 */
esp_err_t mqtt_publish_system_info(device_status_t *status) {

    // Dump current task information for debugging
    dump_current_task();

    // It makes no sense to proceed if status object is NULL
    if (status == NULL) {
        ESP_LOGE(TAG, "Got NULL as status in mqtt_publish_system_info.");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Publish system information to MQTT: Uptime (%llu microseconds), Free heap (%u bytes), Min free heap (%u bytes)", 
            (unsigned long long)status->time_since_boot, status->free_heap, status->min_free_heap);

    
    /* Process MQTT connection mode: check if MQTT is enabled and client is connected */
    uint16_t mqtt_connection_mode;
    esp_err_t err;

    // Read MQTT connection mode from NVS
    err = nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT connection mode from NVS");
        return ESP_FAIL;
    }

    // Check if MQTT is disabled in the device settings
    if (mqtt_connection_mode < (uint16_t)MQTT_CONN_MODE_NO_RECONNECT) {
        ESP_LOGW(TAG, "MQTT disabled in device settings. Publishing skipped. That's not an issue.");
        return ESP_OK;
    }

    // Ensure MQTT client is initialized and connected
    dump_sys_bits("mqtt_publish_system_info: Before MQTT client check");
    if (mqtt_client == NULL || !IS_MQTT_CONNECTED()) {
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

    // Read MQTT prefix and device ID from NVS
    char *mqtt_prefix = NULL;
    char *device_id = NULL;
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));

    // Publish system information to MQTT
    char topic[256];
    int msg_id;
    bool is_error = false;
    char value[32];

    // Publish entire status as JSON
    snprintf(topic, sizeof(topic), "%s/%s/system", mqtt_prefix, device_id);
    char *payload = serialize_device_status(status);
    ESP_LOGI(TAG, "Publishing system information to MQTT topic: %s", topic);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish system information to MQTT topic: %s", topic);
        free(mqtt_prefix);
        free(device_id);
        free(payload);
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "System information published to MQTT topic: %s, msg_id: %d", topic, msg_id);
    }
    
    // Publish individual fields for compatibility
    // Uptime
    snprintf(topic, sizeof(topic), "%s/%s/system/uptime", mqtt_prefix, device_id);
    snprintf(value, sizeof(value), "%llu", (unsigned long long)status->time_since_boot);
    ESP_LOGI(TAG, "mqtt_publish_system_info: Publish value (%s) to topic (%s)", value, topic);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic);
        is_error = true;
    }

    // Free heap
    snprintf(topic, sizeof(topic), "%s/%s/system/free_heap", mqtt_prefix, device_id);
    snprintf(value, sizeof(value), "%u", status->free_heap);
    ESP_LOGI(TAG, "mqtt_publish_system_info: Publish value (%s) to topic (%s)", value, topic);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic);
        is_error = true;
    }

    // Min free heap
    snprintf(topic, sizeof(topic), "%s/%s/system/min_free_heap", mqtt_prefix, device_id);
    snprintf(value, sizeof(value), "%u", status->min_free_heap);
    ESP_LOGI(TAG, "mqtt_publish_system_info: Publish value (%s) to topic (%s)", value, topic);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Topic %s not published", topic);
        is_error = true;
    }

    // Free allocated resources
    free(mqtt_prefix);
    free(device_id);
    free(payload);

    if (is_error) {
        ESP_LOGE(TAG, "There were errors when publishing system information to MQTT");
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "MQTT system information published successfully.");
    }
    return ESP_OK;
}

/**
 * @brief: Task for regular device auto-discovery updates for Home Assistant
 * 
 * This function creates a task that periodically updates the Home Assistant device
 * configuration in MQTT. The task reads the device ID, MQTT prefix, and Home Assistant
 * prefix from NVS and uses them to publish the device configuration to MQTT. The task
 * waits for the MQTT connection to become ready before publishing the configuration.
 * 
 * @param[in] param Unused task parameter.
 * 
 * @return esp_err_t    ESP_OK on success, ESP_FAIL if HA data connot be published.
 */
esp_err_t mqtt_publish_home_assistant_config(const char *device_id, const char *mqtt_prefix, const char *homeassistant_prefix) {
    
    uint16_t mqtt_connection_mode;
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));
    if (mqtt_connection_mode < (uint16_t)MQTT_CONN_MODE_NO_RECONNECT) {
        ESP_LOGW(TAG, "MQTT disabled in device settings. Publishing skipped.");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "mqtt-hass: Waiting for MQTT connection to become ready...");

    // Wait up to 10 seconds total
    EventBits_t bits = xEventGroupWaitBits(
        g_sys_events,             // event group handle
        BIT_MQTT_CONNECTED | BIT_MQTT_READY,       // bit(s) to wait for
        pdFALSE,                  // don't clear the bit on exit
        pdTRUE,                   // wait for all bits (only one here)
        pdMS_TO_TICKS(10000)      // timeout 10 seconds
    );

    if ((bits & BIT_MQTT_CONNECTED) && (bits & BIT_MQTT_READY)) {
        ESP_LOGI(TAG, "mqtt-hass: MQTT connection is ready!");
        // Continue normal operation
    } else {
        ESP_LOGE(TAG, "mqtt-hass: MQTT never became ready after 10 seconds");
        return ESP_FAIL;
    }
    
    char topic[512];
    char discovery_path[256];
    int msg_id;
    bool is_error = false;
    const char *metric = HA_DEVICE_METRIC_STATE;
    const char *device_class = HA_DEVICE_DEVICE_CLASS;
    char *relay_key = NULL;

    relay_unit_t *relay_list = NULL;
    uint16_t total_count = 0;


    // Load all relay units
    esp_err_t err = get_all_relay_units(&relay_list, &total_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load relay units from NVS.");
        return err;
    }
    if (!total_count) {
        ESP_LOGW(TAG, "No relays found to update HA auto-discovery records for.");
        return ESP_OK;
    }

    ha_entity_discovery_t *entity_discovery = (ha_entity_discovery_t *)malloc(sizeof(ha_entity_discovery_t));
    char *discovery_json;

    // Iterate through each relay and publish to MQTT
    for (uint16_t i = 0; i < total_count; i++) {
        // Get the NVS key for the relay
        relay_key = get_unit_nvs_key(&relay_list[i]);
        if (relay_key == NULL) {
            ESP_LOGE(TAG, "Failed to get NVS key for relay channel %d.", relay_list[i].channel);
            continue;  // Move to the next relay if key generation fails
        }

        if (ha_entity_discovery_fullfill(entity_discovery, device_class, relay_key, metric,relay_list[i].type) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to initiate entity discovery for %s", metric);
            return ESP_FAIL;
        }

        discovery_json = ha_entity_discovery_print_JSON(entity_discovery);
        ESP_LOGI(TAG, "Device discovery serialized:\n%s", discovery_json);

        memset(discovery_path, 0, sizeof(discovery_path));
        sprintf(discovery_path, "%s/%s", homeassistant_prefix, HA_DEVICE_FAMILY);
        sprintf(topic, "%s/%s_%s/%s/%s", discovery_path, device_id, relay_key, HA_DEVICE_FAMILY, HA_DEVICE_CONFIG_PATH);

        msg_id = esp_mqtt_client_publish(mqtt_client, topic, discovery_json, 0, 1, 1);
        if (msg_id < 0) {
            ESP_LOGW(TAG, "Discovery topic %s not published", topic);
            is_error = true;
        }
        // Free allocated resources
        cJSON_free(discovery_json);
        free(relay_key);
        relay_key = NULL;
        // memset(entity_discovery, 0, sizeof(ha_entity_discovery_t)); // reset for next use
    }

    // tell we are online
    msg_id = esp_mqtt_client_publish(mqtt_client, entity_discovery->availability->topic, ha_availability_entry_print_JSON("online"), 0, 0, true);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Discovery topic %s not published", topic);
        is_error = true;
    }
    ha_entity_discovery_free(entity_discovery);

    // free relays list
    ESP_ERROR_CHECK(free_relays_array(relay_list, total_count));

    if (is_error) {
        ESP_LOGE(TAG, "There were errors when publishing Home Assistant device configuration to MQTT.");
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "Home Assistant device configuration published.");
    }

    return ESP_OK;
}

/**
 * @brief: Task for regular device auto-discovery updates for Home Assistant
 * 
 * This function creates a FreeRTOS task that periodically updates the Home Assistant device
 * configuration in MQTT. The task reads the device ID, MQTT prefix, and Home Assistant
 * prefix from NVS and uses them to publish the device configuration to MQTT. The task
 * waits for the MQTT connection to become ready before publishing the configuration.
 * 
 * @param[in] param Unused task parameter.
 * 
 * @return void   This function does not return a value.
 */
void mqtt_device_config_task(void *param) {
    char *device_id = NULL;
    char *mqtt_prefix = NULL;
    char *ha_prefix = NULL;
    uint32_t ha_upd_intervl;
    uint32_t ha_retry_interval = 5000;
    bool ha_update_successful = false;

    const char* LOG_TAG = "HA MQTT DEVICE";
      
    // Load MQTT prefix from NVS
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, &ha_upd_intervl));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_HA_PREFIX, &ha_prefix));

    ESP_LOGI(LOG_TAG, "Starting HA MQTT device update task. Update interval: %lu minutes.", (uint32_t) ha_upd_intervl / 1000 / 60);

    while (true) {
        // Update Home Assistant device configuration
        ESP_LOGI(LOG_TAG, "Updating HA device configurations...");
        if (mqtt_publish_home_assistant_config(device_id, mqtt_prefix, ha_prefix) != ESP_OK) {
            ha_update_successful = false;
            ESP_LOGI(LOG_TAG, "HA device configurations end up with errors. Will retry in %li seconds.", (uint32_t)ha_retry_interval/1000);
        } else {
            ha_update_successful = true;
            ESP_LOGI(LOG_TAG, "HA device configurations update complet. Next update in %li seconds.", (uint32_t)ha_upd_intervl/1000);
        }

        // Wait for the defined interval before the next update
        vTaskDelay(ha_update_successful?ha_upd_intervl:ha_retry_interval);
    }

    free(device_id);
    free(mqtt_prefix);
    free(ha_prefix);
}

/**
 * @brief: Resolve the device object from MQTT topic
 * 
 * This function resolves the device object from the MQTT topic. The function extracts the
 * device ID from the topic and uses it to load the device object from NVS. The function
 * returns a pointer to the device object if successful, or NULL if the device cannot be
 * resolved.
 * 
 * @param[in] topic The MQTT topic to resolve.
 * 
 * @return device_t*    A pointer to the unit structure, or NULL if the device cannot be resolved.
 */
relay_unit_t *resolve_relay_from_topic(const char *topic) {

    ESP_LOGI(TAG, "Got MQTT topic to match the relay for: %s", topic);

    // Extract the relay key from the topic
    char *relay_key = resolve_key_from_topic(topic);
    if (relay_key == NULL) {
        ESP_LOGE(TAG, "Failed to resolve relay key from topic: %s (NULL)", topic);
        return NULL;
    }

    ESP_LOGI(TAG, "Extracted relay key: %s", relay_key);

    // Allocate memory for the relay
    relay_unit_t *relay = (relay_unit_t *)malloc(sizeof(relay_unit_t));
    if (relay == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for relay unit");
        free(relay_key);  // Free the relay key before returning
        return NULL;
    }

    // Load the relay from NVS
    esp_err_t err = load_relay_actuator_from_nvs(relay_key, relay);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load relay actuator from NVS for key: %s", relay_key);
        free(relay_key);
        free(relay);  // Free the allocated relay before returning
        return NULL;
    }

    // Free the dynamically allocated relay key
    free(relay_key);

    // Return the relay pointer
    return relay;
}

/**
 * @brief: Get relay key from MQTT topic
 * 
 * This function extracts the relay key from the MQTT topic. The function splits the topic
 * using '/' as a delimiter and retrieves the element at index 2. The function returns a
 * dynamically allocated string containing the relay key if successful, or NULL if the key
 * cannot be resolved. The caller is responsible for freeing the returned string.
 * 
 * @param[in] topic The MQTT topic to extract the relay key from.
 * 
 * @return char*    A dynamically allocated string containing the relay key, or NULL if the key cannot be resolved.
 */
char *resolve_key_from_topic(const char *topic) {
    return get_element_from_path(topic, 2);
}

/**
 * @brief Extracts the element at a specific index from a given path.
 *
 * This function splits the path using '/' as a delimiter and retrieves the element
 * at the specified zero-based index.
 *
 * @param path The full MQTT topic path (e.g., "relay_board/DCDA0C7E08A4/relay_ch_1/switch/set")
 * @param index The zero-based index of the element to retrieve (e.g., 2 to get "relay_ch_1")
 * @return A dynamically allocated string containing the element at the specified index,
 *         or NULL if the index is out of bounds or on any error. The caller is responsible
 *         for freeing the returned string.
 */
char *get_element_from_path(const char *path, int index) {
    if (path == NULL || index < 0) {
        ESP_LOGE(TAG, "Invalid input: path is NULL or index is negative.");
        return NULL;
    }

    // Split the path into tokens
    size_t count = 0;
    char *path_copy = strdup(path);  // Copy the path since strtok modifies it
    char **tokens = str_split(path_copy, '/', &count);

    // Check if index is out of bounds
    if (index >= (int)count) {
        ESP_LOGE(TAG, "Index out of bounds: count(%i), index(%i)", count, index);
        free(path_copy);  // Free the duplicated path
        free(tokens);     // Free the tokens array
        return NULL;
    }

    // Allocate memory for the selected element and return it
    char *result = strdup(tokens[index]);

    // Free the allocated memory used by tokens
    free(path_copy);
    free(tokens);  // Free the tokens array itself

    return result;  // Caller is responsible for freeing this
}


/**
 * @brief: Split character string by delimiter
 * 
 * This function splits a character string by a delimiter and returns an array of strings.
 * The function dynamically allocates memory for the array and the individual strings.
 * The caller is responsible for freeing the memory allocated by the function.
 * 
 * @param[in] a_str The character string to split.
 * @param[in] a_delim The delimiter character to split the string by.
 * @param[out] element_count The number of elements in the resulting array.
 * 
 * @return char**    An array of strings containing the split elements, or NULL if the string cannot be split.
 */
char** str_split(char* a_str, const char a_delim, size_t *element_count)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    *element_count = count;
    return result;
}


/**
 * @brief: Subscribe to MQTT relay update commands
 * 
 * This function subscribes to the MQTT topics for relay update commands. The function
 * creates a FreeRTOS task that listens for relay update commands and updates the relay
 * state accordingly. The function reads the MQTT prefix and device ID from NVS and uses
 * them to construct the MQTT topics. The function waits for the MQTT connection to become
 * ready before subscribing to the topics.
 * 
 * @param[in] param Unused task parameter.
 * 
 * @return void   This function does not return a value.
 */
void mqtt_subscribe_relays_task(void *arg) {
    mqtt_command_event_t event;

    while (1) {
        if (xQueueReceive(mqtt_command_queue, &event, portMAX_DELAY)) {

            ESP_LOGI(TAG, "Recevied subscription event: key (%s), state (%i)", event.relay_key, (int)event.state);

            relay_type_t relay_type = get_relay_type_from_key(event.relay_key);
            if (relay_type != RELAY_TYPE_ACTUATOR) {
                ESP_LOGW(TAG, "Wrong relay type got request for state update (key: %s, type: %i). Ignoring.", event.relay_key, relay_type);
                continue;
            }
            relay_unit_t *relay = (relay_unit_t *)malloc(sizeof(relay_unit_t));
            if (load_relay_actuator_from_nvs(event.relay_key, relay) == ESP_OK) {
                relay_set_state(relay, event.state, true);  // Update the relay state
            }
            free(event.relay_key);  // Free the allocated key
            if (INIT_RELAY_ON_LOAD) {
                relay_gpio_deinit(relay);
            }
            free(relay);
        }
    }
}

/**
 * @brief: Subscribe relay to corresponding MQTT topic
 * 
 * This function subscribes the relay to the MQTT topic for relay update commands. The function
 * reads the MQTT prefix and device ID from NVS and uses them to construct the MQTT topics. The
 * function waits for the MQTT connection to become ready before subscribing to the topics.
 * 
 * @param[in] relay The relay to subscribe to the MQTT topic.
 * 
 * @return esp_err_t    ESP_OK on success, ESP_FAIL if the relay cannot be subscribed.
 */
esp_err_t mqtt_relay_subscribe(relay_unit_t *relay) {

    dump_current_task();

    if (relay == NULL) {
        ESP_LOGE(TAG, "Got NULL as relay in mqtt_relay_subscribe.");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Subscribe relay/sensor information to receive information from MQTT. Channel (%i), type (%i)", relay->channel, relay->type);

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
    dump_sys_bits("mqtt_relay_subscribe: Before MQTT client check");
    if (mqtt_client == NULL) {
        ESP_LOGW(TAG, "MQTT client is not initialized (NULL).");
        return ESP_FAIL;
    }
    if (!IS_MQTT_READY()) {
        ESP_LOGW(TAG, "MQTT client is not marked as connected and ready.");
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

    // Allocate and read the MQTT prefix
    char *mqtt_prefix = NULL;
    err = nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MQTT prefix from NVS");
        return err;
    }

    // Allocate and read the device ID
    char *device_id = NULL;
    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device ID from NVS");
        free(mqtt_prefix);  // Clean up previous allocation
        return err;
    }

    // Allocate memory for the command topic
    size_t topic_len = strlen(mqtt_prefix) + strlen(device_id) + strlen(get_unit_nvs_key(relay)) + strlen(HA_DEVICE_FAMILY) + strlen("/set") + 4; // extra for slashes and null terminator
    char *command_topic = (char *)malloc(topic_len);
    if (command_topic == NULL) {
        free(mqtt_prefix);
        free(device_id);
        ESP_LOGE(TAG, "Failed to allocate memory for command topic");
        return ESP_ERR_NO_MEM;
    }

    // Format the command topic
    snprintf(command_topic, topic_len, "%s/%s/%s/%s/set", mqtt_prefix, device_id, get_unit_nvs_key(relay), HA_DEVICE_FAMILY);

    // Subscribe to the command topic
    int msg_id = esp_mqtt_client_subscribe_single(mqtt_client, command_topic, 1);  // QoS 1 for reliability
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", command_topic);
        free(mqtt_prefix);
        free(device_id);
        free(command_topic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Subscribed to topic: %s", command_topic);

    // Free allocated memory for command topic
    free(mqtt_prefix);
    free(device_id);
    free(command_topic);

    return ESP_OK;
}