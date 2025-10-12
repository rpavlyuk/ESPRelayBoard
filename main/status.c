#include <stddef.h>
#include <stdint.h>

#include "status.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_heap_trace.h"
#include "esp_debug_helpers.h"  // For esp_backtrace_print
#include "esp_timer.h"
#include "driver/gpio.h"

#include "relay.h"
#include "mqtt.h"

static heap_trace_record_t trace_buffer[NUM_RECORDS];  // Buffer to store the trace records

void status_task(void *pvParameters) {
    int cycle = 0;
    while (1) {
        ESP_LOGI(STATUS_TAG, "=== System Status ===");
        cycle++;

        if (_DEVICE_ENABLE_STATUS_SYSINFO_HEAP) {
            ESP_LOGI(STATUS_TAG, "--- Heap Information ---");
            // Print heap information
            heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);


            // Print free memory
            size_t free_heap = esp_get_free_heap_size();
            ESP_LOGI(STATUS_TAG, "Free heap: %u bytes", free_heap);

            // Print minimum free heap size (i.e., lowest value during program execution)
            size_t min_free_heap = esp_get_minimum_free_heap_size();
            ESP_LOGI(STATUS_TAG, "Minimum free heap size: %u bytes", min_free_heap);
        }

        if (_DEVICE_ENABLE_STATUS_SYSINFO_HEAP_CHECK) {
            ESP_LOGI(STATUS_TAG, "--- Checking heap integrity ---");
            // Check heap integrity
            if (!heap_caps_check_integrity_all(true)) {
                ESP_LOGE(STATUS_TAG, "Heap corruption detected!");
            } else {
                ESP_LOGI(STATUS_TAG, "No heap corruption detected");
            }
        }

        // Post system status. Function mqtt_publish_system_info() will check if MQTT is enabled and connected
        if (_DEVICE_ENABLE_STATUS_SYSINFO_MQTT) {
            device_status_t status;
            if (device_status_init(&status) == ESP_OK) {
                if (mqtt_publish_system_info(&status) != ESP_OK) {
                    ESP_LOGE(STATUS_TAG, "Failed to publish system status to MQTT");
                } else {
                    ESP_LOGI(STATUS_TAG, "System status published to MQTT");
                }
            } else {
                ESP_LOGE(STATUS_TAG, "Failed to initialize device status");
            }
        }

        /* DISABLED -- sometimes causes crash 
        // Dump heap trace after some time (e.g., every 3 heap monitor cycles)
        if (cycle > period && heap_trace_stop() == ESP_OK) {
            ESP_LOGI(TAG, "Heap trace stopped. Dumping results...");
            heap_trace_dump();  // Dump the trace logs to check for leaks
            // Restart heap trace after dump
            heap_trace_start(HEAP_TRACE_LEAKS);  
        }
         */

        // Dump GPIO configurations for all safe GPIO pins
        if (_DEVICE_ENABLE_STATUS_SYSINFO_GPIO) {
            dump_all_gpio_configurations();
        }

        // Delay for a period (e.g., 10 seconds)
        vTaskDelay(pdMS_TO_TICKS(HEAP_DUMP_INTERVAL_MS));
    }
}

// Function to dump all configurations of safe GPIO pins
void dump_all_gpio_configurations() {
    relay_unit_t *relay_list = NULL;
    uint16_t total_count = 0;

    // Retrieve all relay units (both actuators and sensors)
    esp_err_t err = get_all_relay_units(&relay_list, &total_count);
    if (err != ESP_OK) {
        printf("Failed to get relay units: %s\n", esp_err_to_name(err));
        return;
    }

    // Create a bit mask from the GPIO pins used by the relay units
    uint64_t io_bit_mask = 0;
    for (int i = 0; i < total_count; i++) {
        int gpio_num = relay_list[i].gpio_pin;
        if (GPIO_IS_VALID_GPIO(gpio_num)) {
            io_bit_mask |= (1ULL << gpio_num);  // Set the corresponding bit in the mask
        } else {
            printf("GPIO[%d] is not a valid GPIO.\n", gpio_num);
        }
    }

    // Dump the configuration using gpio_dump_io_configuration
    err = gpio_dump_io_configuration(stdout, io_bit_mask);
    if (err != ESP_OK) {
        printf("Failed to dump GPIO configurations: %s\n", esp_err_to_name(err));
    }

    // Free the relay list
    free(relay_list);
}

void status_init() {
    // Initialize heap tracing with a standalone buffer
    esp_err_t err = heap_trace_init_standalone(trace_buffer, NUM_RECORDS);
    if (err != ESP_OK) {
        ESP_LOGE(STATUS_TAG, "Heap trace initialization failed: %s", esp_err_to_name(err));
        return;
    }

    // Start heap trace with leak detection enabled
    heap_trace_start(HEAP_TRACE_LEAKS);
    ESP_LOGI(STATUS_TAG, "Heap trace started for leak detection");

    // Start monitoring task
    ESP_LOGI(STATUS_TAG, "Starting system status task");
    xTaskCreate(status_task, "status_task", 4096, NULL, 1, NULL); // 4096 bytes of stack space
}

/**
 * @brief: Initialize sensor device status
 */
esp_err_t device_status_init(device_status_t *status_data) {

    // Dump current task information for debugging
    dump_current_task();

    status_data->free_heap = esp_get_free_heap_size();
    status_data->min_free_heap = esp_get_minimum_free_heap_size();
    status_data->time_since_boot = esp_timer_get_time();

    ESP_LOGD(STATUS_TAG, "Device status initialized: Free heap (%u bytes), Min free heap (%u bytes), Time since boot (%llu microseconds)", 
             status_data->free_heap, status_data->min_free_heap, (unsigned long long)status_data->time_since_boot);

    return ESP_OK;
}

/**
 * @brief: Get CJSON object of sensor_status_t
 */
cJSON *device_status_to_JSON(device_status_t *s_data) {

    cJSON *root = cJSON_CreateObject();

    cJSON *j_free_heap = cJSON_CreateNumber(s_data->free_heap);
    if (j_free_heap != NULL) {
        cJSON_AddItemToObject(root, "free_heap", j_free_heap);
    }

    cJSON *j_min_free_heap = cJSON_CreateNumber(s_data->min_free_heap);
    if (j_min_free_heap != NULL) {
        cJSON_AddItemToObject(root, "min_free_heap", j_min_free_heap);
    }

    cJSON *j_time_since_boot = cJSON_CreateNumber(s_data->time_since_boot);
    if (j_time_since_boot != NULL) {
        cJSON_AddItemToObject(root, "time_since_boot", j_time_since_boot);
    }

    return root;

}

/**
 * @brief: Serialize sensor status information to JSON string
 */
char *serialize_device_status(device_status_t *s_data) {

    char *json = NULL;
    cJSON *c_json = device_status_to_JSON(s_data);

    json = cJSON_Print(c_json);
    cJSON_Delete(c_json);
    return json;
}

/**
 * @brief: Compile JSON object from device status and other data 
 */
cJSON *device_all_to_JSON(device_status_t *status) {

    cJSON *root = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "status", device_status_to_JSON(status));

    return root;

}

/**
 * @brief: Serialize all device data (sensor, <other data>) to JSON
 */
char *serialize_all_device_data(device_status_t *status) {
    cJSON *c_json = device_all_to_JSON(status);
    if (!c_json) {
        ESP_LOGE(STATUS_TAG, "Failed to create JSON object for device data serialization");
        return NULL;
    }

    char *json = cJSON_PrintUnformatted(c_json);
    cJSON_Delete(c_json);
    return json;

}

/**
 * @brief: Dump current task information to stdout
 * 
 */
void dump_current_task() {
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();  // Get handle of current task
    const char *task_name = pcTaskGetName(current_task);      // Get the task's name

    ESP_LOGI(TAG, "Current task: %s", task_name);      // Log the task name
}