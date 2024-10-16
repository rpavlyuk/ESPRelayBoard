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

void app_main(void) {

    bool wifi_provisioned = false;
    
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
        ESP_LOGI(TAG, "WiFi is provisioned!");

        // start web
        if (_DEVICE_ENABLE_WEB) {
            ESP_LOGI(TAG, "WEB ENABLED!");

            xTaskCreate(run_http_server, "run_http_server", 8192, NULL, 5, NULL);
        }

        uint16_t mqtt_connection_mode;
        ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connection_mode));

        // start MQTT client
        if (_DEVICE_ENABLE_MQTT && mqtt_connection_mode) {
            ESP_LOGI(TAG, "MQTT ENABLED!");
            if (mqtt_init() == ESP_OK) {
                ESP_LOGI(TAG, "Connected to MQTT server!");
                g_mqtt_ready = true;
            } else {
                ESP_LOGE(TAG, "Unable to connect to MQTT broker");
                return;
            }

            ESP_ERROR_CHECK(start_mqtt_queue_task());

            /*
            // _DEVICE_ENABLE_HA
            if (_DEVICE_ENABLE_HA) {
                ESP_LOGI(TAG, "HA device status ENABLED!");
                xTaskCreate(mqtt_device_config_task, "mqtt_device_config_task", 4096, NULL, 5, NULL);
            }
            */
        }

    }

    if (_DEVICE_ENABLE_STATUS) {
        ESP_LOGI(TAG, "Status ENABLED!");
        // Start the status monitoring task 
        status_init();
    }

}
