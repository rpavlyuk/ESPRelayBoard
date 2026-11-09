#ifndef SYSTEM_OTA_UPDATE_H
#define SYSTEM_OTA_UPDATE_H

#include <stdio.h>
#include "esp_err.h"

/**
 * Settings handlers and structures
 */
static const char *OTA_TAG = "OTAUpdate";

/**
 * Paths
 */

#if !defined(CA_CERT_PATH_MQTTS)
#define CA_CERT_PATH_MQTTS  "/spiffs/ca-mqtts.crt"
#endif

#if !defined(CA_CERT_PATH_HTTPS)
#define CA_CERT_PATH_HTTPS  "/spiffs/ca-https.crt"
#endif

#if !defined(OTA_STORAGE_IMAGE_NAME)
#define OTA_STORAGE_IMAGE_NAME  "storage.bin"
#endif

/**
 * Static settings
 */
#if !defined(OTA_UPDATE_URL_LENGTH)
#define OTA_UPDATE_URL_LENGTH  256
#endif

#if !defined(DO_OTA_STORAGE_UPDATE)
#define DO_OTA_STORAGE_UPDATE       true
#endif

/** 
 * Initialize types
 */

/* Structure to pass the OTA URL to the task */
typedef struct {
    char ota_url[OTA_UPDATE_URL_LENGTH];  // Adjust the size if needed
} ota_update_param_t;

/**
 * @brief: Call OTA update
 */
esp_err_t perform_ota_update(const char *url);

/**
 * @brief: Generate the storage update URL
 */
esp_err_t generate_storage_update_url(const char *firmware_url, char **storage_url);

/**
 * @brief: Download and update the SPIFFS partition
 */
esp_err_t download_and_update_spiffs_partition(const char *url);

/**
 * @brief: Check if the OTA partitions are valid
 */
esp_err_t check_ota_partitions(void);

#endif // SYSTEM_OTA_UPDATE_H