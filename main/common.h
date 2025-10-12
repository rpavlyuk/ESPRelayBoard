#ifndef COMMON_H
#define COMMON_H

/* version.h header is auto-generated during build */
#include "version.h"

/**
 * Enabling functional modules
 */
#define _DEVICE_ENABLE_WIFI     true
#define _DEVICE_ENABLE_WEB      (true && _DEVICE_ENABLE_WIFI)
#define _DEVICE_ENABLE_MQTT     (true && _DEVICE_ENABLE_WIFI)
#define _DEVICE_ENABLE_HA       (true && _DEVICE_ENABLE_MQTT)

#define _DEVICE_ENABLE_MQTT_REFRESH        (true && _DEVICE_ENABLE_MQTT)

#define _DEVICE_ENABLE_STATUS                       true
#define _DEVICE_ENABLE_STATUS_SYSINFO_MQTT          (true && _DEVICE_ENABLE_STATUS && _DEVICE_ENABLE_MQTT)
#define _DEVICE_ENABLE_STATUS_SYSINFO_HEAP          (false && _DEVICE_ENABLE_STATUS)
#define _DEVICE_ENABLE_STATUS_SYSINFO_HEAP_CHECK    (false && _DEVICE_ENABLE_STATUS)
#define _DEVICE_ENABLE_STATUS_SYSINFO_GPIO          (false && _DEVICE_ENABLE_STATUS)


static const char *TAG = "RelayBoard";

#endif