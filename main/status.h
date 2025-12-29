#ifndef STATUS_H
#define STATUS_H

#include <stddef.h>
#include "cJSON.h"
#include "esp_system.h"
#include "common.h"

#define HEAP_DUMP_INTERVAL_MS 10000  // 10 seconds
#define NUM_RECORDS 100  // Number of allocations to trace
#define BACKTRACE_DEPTH 6  // Number of stack frames to capture in backtrace

#define MEMGUARD_BOOT_PROTECTION_TIME_MINUTES 3  // Minimum uptime in minutes before allowing reboot
#define MEMGUARD_CONSECUTIVE_THRESHOLD_COUNT 3  // Number of consecutive checks below threshold before action

static const char *STATUS_TAG = "D HeapMonitor";

/**
 * Sensor readings information
 */
typedef struct {
    size_t free_heap;
    size_t min_free_heap;
    int64_t time_since_boot;
    size_t memguard_threshold;
    uint16_t memguard_mode;
} device_status_t;

void status_init(void);

void dump_all_gpio_configurations();

esp_err_t device_status_init(device_status_t *status_data);

cJSON *device_status_to_JSON(device_status_t *s_data);
char *serialize_device_status(device_status_t *s_data);

cJSON *device_all_to_JSON(device_status_t *status);
char *serialize_all_device_data(device_status_t *status);

void dump_current_task();

#endif // STATUS_H