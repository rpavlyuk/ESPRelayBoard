#include <stdio.h>
#include "esp_log.h"

#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"

#include "non_volatile_storage.h"

#include "main.h"
#include "settings.h"
#include "wifi.h"
#include "web.h"
#include "status.h"
#include "relay.h"
#include "mqtt.h"

/**
 * @brief Main application entry point
 */
void app_main(void) {

    bool wifi_provisioned = false;

    /* Logging setup */
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("non_volatile_storage", ESP_LOG_WARN);
    esp_log_level_set("NVS_LARGE", ESP_LOG_VERBOSE);
    
    /* Initializations */

    // Check partition table
    ESP_ERROR_CHECK(check_ota_partitions());

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_init());

    // Init settings
    ESP_ERROR_CHECK(settings_init());

    // Register ISRs for the GPIO pins
    ESP_ERROR_CHECK(relay_all_sensors_register_isr());

    // Start monitoring the GPIO events queue for sensor units
    // Create the GPIO event task to process the ISR queue
    xTaskCreate(gpio_event_task, "gpio_event_task", 4096, NULL, 10, NULL);

        // enable filesystem needed for WEB server
    if (_DEVICE_ENABLE_WEB) {
        // init internal filesystem
        init_filesystem();
    }

    /* Warm welcome in the console */
    char *device_id;
    char *device_serial;
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    ESP_LOGI(TAG, "*** Starting ESP32-based Relay Board device ***");
    ESP_LOGI(TAG, "Version: %s", DEVICE_SW_VERSION);
    ESP_LOGI(TAG, "Device ID: %s", device_id);
    ESP_LOGI(TAG, "Device Serial: %s", device_serial);

    // Free allocated memory after usage
    free(device_id);
    free(device_serial);

    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (_DEVICE_ENABLE_WIFI) {
        ESP_LOGI(TAG, "WIFI ENABLED!");

        /* Initialize WIFI */
        initialize_wifi();

        // Initialize Wi-Fi provisioning manager
        wifi_prov_mgr_config_t wifi_config = {
            .scheme = wifi_prov_scheme_softap,
            .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
        };
        ESP_ERROR_CHECK(wifi_prov_mgr_init(wifi_config));
        ESP_LOGI(TAG, "WiFi provisioning manager initialization complete");

        // Check if the device is already provisioned
        
        ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&wifi_provisioned));

        // Initialize and start Wi-Fi
        start_wifi(wifi_provisioned);
    }

    if (wifi_provisioned) {
        ESP_LOGI(TAG, "WiFi is provisioned! Let's wait for Wi-Fi to be ready...");
        
        // Wifi is provisioned but might be ready for now. Wait until Wi-Fi is ready
        int i = 0, c_limit = 30;
        while(!g_wifi_ready && i < c_limit) {
            ESP_LOGI(TAG, "Waiting for Wi-Fi/network to become ready...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            i++;
        }
        // If Wi-Fi is not ready after the limit, restart the device
        // as it makes no sense to continue without network connection
        if (i == c_limit) {
            ESP_LOGW(TAG, "Wi-Fi/network never became ready");
            ESP_LOGW(TAG, "Sending ESP32 to reboot...");
            esp_restart();
            return;
        }

        // start web
        if (_DEVICE_ENABLE_WEB) {
            ESP_LOGI(TAG, "WEB ENABLED!");

            xTaskCreate(run_http_server, "run_http_server", 16384, NULL, 5, NULL);
        }

        uint16_t mqtt_connection_mode;
        ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));

        // start MQTT client
        if (_DEVICE_ENABLE_MQTT && mqtt_connection_mode) {
            ESP_LOGI(TAG, "MQTT ENABLED!");
            if (mqtt_init() == ESP_OK) {
                ESP_LOGI(TAG, "Connected to MQTT server!");
            } else {
                ESP_LOGE(TAG, "Unable to connect to MQTT broker");
                return;
            }

            /* Start MQTT publishig queue */
            ESP_ERROR_CHECK(start_mqtt_queue_task());

            /* Do initial push of all unit states to MQTT */
            ESP_ERROR_CHECK(relay_publish_all_to_mqtt());

            // _DEVICE_ENABLE_HA
            if (_DEVICE_ENABLE_HA) {
                ESP_LOGI(TAG, "HA device status ENABLED!");
                xTaskCreate(mqtt_device_config_task, "mqtt_device_config_task", 4096, NULL, 5, NULL);
            }
        }

    } else {
        ESP_LOGW(TAG, "WiFi is NOT provisioned. Provisioning process should be started and available now.");
    }


    if (_DEVICE_ENABLE_STATUS) {
        ESP_LOGI(TAG, "Status ENABLED!");
        // Start the status monitoring task 
        status_init();
    }

}
