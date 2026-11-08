/**
 * @file web_device_authorization.c
 * @brief Device authorization utilities for web requests.
 */

// ESP-IDF Components
#include "esp_http_server.h"
#include "esp_err.h"

// User-defined Components
#include "non_volatile_storage.h"
#include "p_utils.h"
#include "p_web_utils.h"
#include "cJSON.h"

// Load header for this component
#include "web_device_authorization.h"

/**
 * @brief Validates device identity from HTTP request query parameters.
 *
 * This function extracts the 'device_id' and 'device_serial' parameters from the HTTP request's query string
 * and compares them with the values stored in NVS (Non-Volatile Storage). If the values match, the function
 * returns ESP_OK; otherwise, it sends an appropriate HTTP error response and returns ESP_FAIL.
 *
 * @param req Pointer to the HTTP request structure.
 * @return ESP_OK if the device identity is valid, ESP_FAIL otherwise.
 */
esp_err_t validate_device_identity_from_get_query(httpd_req_t *req)
{
    char device_id_in[DEVICE_ID_LENGTH + 1];
    char device_serial_in[DEVICE_SERIAL_LENGTH + 1];

    esp_err_t err;

    err = extract_param_value_from_get_query(req, "device_id", device_id_in, sizeof(device_id_in));
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing device_id");
        return ESP_FAIL;
    }

    err = extract_param_value_from_get_query(req, "device_serial", device_serial_in, sizeof(device_serial_in));
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing device_serial");
        return ESP_FAIL;
    }

    char *device_id_nvs = NULL;
    char *device_serial_nvs = NULL;

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id_nvs);
    if (err != ESP_OK || !device_id_nvs) {
        free(device_id_nvs);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read device_id from NVS");
        return ESP_FAIL;
    }

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial_nvs);
    if (err != ESP_OK || !device_serial_nvs) {
        free(device_id_nvs);
        free(device_serial_nvs);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read device_serial from NVS");
        return ESP_FAIL;
    }

    bool ok = (strcmp(device_id_in, device_id_nvs) == 0) &&
              (strcmp(device_serial_in, device_serial_nvs) == 0);

    free(device_id_nvs);
    free(device_serial_nvs);

    if (!ok) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Device ID or serial mismatch");
        return ESP_FAIL;
    }

    return ESP_OK;
}


/**
 * @brief Validates device identity from a cJSON object.
 *
 * This function extracts the 'device_id' and 'device_serial' fields from the provided cJSON object
 * and compares them with the values stored in NVS (Non-Volatile Storage). If the values match, the function
 * returns ESP_OK; otherwise, it returns ESP_FAIL.
 *
 * @param json Pointer to the cJSON object containing device identity fields.
 * @return ESP_OK if the device identity is valid, ESP_FAIL otherwise.
 */
esp_err_t validate_device_identity_from_json(const cJSON *json) {
    if (!json) return ESP_FAIL;

    const cJSON *device_id_in = cJSON_GetObjectItemCaseSensitive(json, "device_id");
    const cJSON *device_serial_in = cJSON_GetObjectItemCaseSensitive(json, "device_serial");

    if (!cJSON_IsString(device_id_in) || (device_id_in->valuestring == NULL)) {
        return ESP_FAIL;
    }

    if (!cJSON_IsString(device_serial_in) || (device_serial_in->valuestring == NULL)) {
        return ESP_FAIL;
    }

    char *device_id_nvs = NULL;
    char *device_serial_nvs = NULL;

    esp_err_t err;

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id_nvs);
    if (err != ESP_OK || !device_id_nvs) {
        free(device_id_nvs);
        return ESP_FAIL;
    }

    err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial_nvs);
    if (err != ESP_OK || !device_serial_nvs) {
        free(device_id_nvs);
        free(device_serial_nvs);
        return ESP_FAIL;
    }

    bool ok = (strcmp(device_id_in->valuestring, device_id_nvs) == 0) &&
              (strcmp(device_serial_in->valuestring, device_serial_nvs) == 0);

    free(device_id_nvs);
    free(device_serial_nvs);

    if (!ok) {
        return ESP_FAIL;
    }

    return ESP_OK;
}