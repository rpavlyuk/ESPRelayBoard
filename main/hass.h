#ifndef HASS_H
#define HASS_H

#include "cJSON.h"
#include "esp_system.h"

#include "common.h"
#include "relay.h"
#include "status.h"

#define HA_DEVICE_MANUFACTURER     "espressif"
#define HA_DEVICE_MODEL            "esp32"
#define CFG_URL_LEN                26

#define HA_DEVICE_STATUS_PATH   "status"
#define HA_DEVICE_STATE_PATH_RELAY     "switch"
#define HA_DEVICE_STATE_PATH_SENSOR    "sensor"
#define HA_DEVICE_CONFIG_PATH   "config"

#define HA_DEVICE_ORIGIN_NAME   "ESP-IDF"
#define HA_DEVICE_ORIGIN_SW     ""
#define HA_DEVICE_ORIGIN_URL    "https://github.com/espressif/esp-idf"

#define HA_DEVICE_DEVICE_CLASS  "fluid_pressure"
#define HA_DEVICE_STATE_CLASS   "measurement"
#define HA_DEVICE_FAMILY_RELAY      "switch"
#define HA_DEVICE_FAMILY_SWITCH     "sensor"


typedef struct {
    char *topic;
} ha_entity_availability_t;

typedef struct {
    char *configuration_url;
    char *manufacturer;
    char *model;
    char *name;
    char *via_device;
    char *identifiers[1];
} ha_device_t;

typedef struct {
    char *name;
    char *sw;
    char *url;
} ha_entity_origin_t;

typedef struct {
    ha_entity_availability_t *availability;
    ha_device_t *device;
    char *device_class;
    bool enabled_by_default;
    char *json_attributes_topic;
    char *object_id;
    ha_entity_origin_t *origin;
    char *state_class;
    char *state_topic;
    char *unique_id;
    char *unit_of_measurement;
    char *value_template;
} ha_entity_discovery_t;


#endif