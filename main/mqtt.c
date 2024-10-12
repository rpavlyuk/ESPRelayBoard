#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "non_volatile_storage.h"
#include "mqtt_client.h"
#include "esp_check.h"

#include "mqtt.h"
#include "settings.h"
#include "wifi.h"
#include "relay.h"  // To access relay data
// #include "hass.h"

esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

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

/*
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
    /*
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    */
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

// Call this function when you are shutting down the application or no longer need the MQTT client
void cleanup_mqtt() {
    if (mqtt_client) {
        mqtt_connected = false;
        ESP_RETURN_VOID_ON_ERROR(esp_mqtt_client_stop(mqtt_client), TAG, "Failed to stop the MQTT client");
        ESP_RETURN_VOID_ON_ERROR(esp_mqtt_client_destroy(mqtt_client), TAG, "Failed to destroy the MQTT client");  // Free the resources
        mqtt_client = NULL;
    }
}