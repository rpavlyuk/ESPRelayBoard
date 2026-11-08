
 #include <ctype.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"

#include "non_volatile_storage.h"
#include "p_utils.h"
#include "p_web_utils.h"

#include "web_wifi_provisioning.h"
#include "web_device_authorization.h"

static const char *TAG = "WIFI_WEB_PROVISIONING";

/**
 * @brief HTTP GET handler for the WiFi provision page.
 *
 * This function handles HTTP GET requests for the WiFi provision page.
 * It sends a simple response indicating the WiFi provision page.
 *
 * @param req Pointer to the HTTP request structure.
 * @return ESP_OK on success.
 */
esp_err_t wifi_provision_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Processing wifi provision GET request");

    // Prefer one big allocation to reduce fragmentation
    const size_t buf_size = MAX_LARGE_TEMPLATE_SIZE + 1;
    // const size_t total = buf_size * 2;
    const size_t total = buf_size;

    char *mem = (char *)malloc(total);

    if (!mem) {
        ESP_LOGE(TAG, "OOM: wifi page handler needs %u bytes (free=%u, min_free=%u)",
                 (unsigned)total,
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)esp_get_minimum_free_heap_size());
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "ESP device is out of free memory");
        return ESP_FAIL;
    }

    // Assign pointers within the allocated block
    char *html_output    = mem;

    // html_template[0] = '\0';
    html_output[0] = '\0';

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/wifi.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        free(mem);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Load the template into html_output
    size_t len = fread(html_output, 1, buf_size - 1, f);
    if (len == buf_size - 1 && !feof(f)) {
        ESP_LOGE(TAG, "wifi.html too large (>%u bytes)", (unsigned)(buf_size - 1));
        fclose(f);
        free(mem);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    fclose(f);
    html_output[len] = '\0'; // Null-terminate the string

    char *device_id = NULL;
    char *device_serial = NULL;
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    replace_placeholder(html_output, "{VAL_DEVICE_ID}", device_id);
    replace_placeholder(html_output, "{VAL_DEVICE_SERIAL}", device_serial);

    // replace static fields
    assign_static_page_variables(html_output);

    // get current Wi-Fi settings
    wifi_config_t config = {0};
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot read Wi-Fi settings: %s",
                 esp_err_to_name(err));
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "Cannot read current Wi-Fi settings");
        return ESP_FAIL;
    }
    replace_placeholder(html_output, "{VAL_WIFI_SSID}", (char *)config.sta.ssid);
    replace_placeholder(html_output, "{VAL_WIFI_PASSWORD}", (char *)config.sta.password);
    wifi_provision_clear(&config, sizeof(config));

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_output, strlen(html_output));

    free(mem);
    free(device_id);
    free(device_serial);
    return ESP_OK;
}

/**
 * @brief Save STA credentials in the Wi-Fi driver's standard NVS storage.
 * POST /api/wifi/provision
 * {"device_id":"...", "device_serial":"...", "ssid":"...", "password":"..."}
 *
 * Requires initialized Wi-Fi with STA enabled (WIFI_MODE_STA or WIFI_MODE_APSTA)
 * and Wi-Fi NVS enabled. Does not connect, disconnect, or reboot. The caller
 * should arrange a reboot after receiving success. This updates the driver's
 * current STA configuration as well as flash; it is not a separate staging area.
 * Do not submit credentials concurrently through the provisioning manager.
 */
esp_err_t wifi_provision_post_handler(httpd_req_t *req) {
    char content[MAX_JSON_BUFFER_SIZE] = {0};
    wifi_config_t wifi_config = {0};
    cJSON *json = NULL;
    cJSON *password_item = NULL;
    const char *http_status = "400 Bad Request";
    const char *message = "Invalid JSON request";
    bool saved = false;
    bool close_connection = false;
    size_t received = 0;

    if (req->content_len == 0 || req->content_len >= sizeof(content)) {
        message = "Empty or oversized request body";
        close_connection = true;
        goto cleanup;
    }
    while (received < req->content_len) {
        int n = httpd_req_recv(req, content + received, req->content_len - received);
        if (n <= 0) {
            http_status = n == HTTPD_SOCK_ERR_TIMEOUT ? "408 Request Timeout" : "400 Bad Request";
            message = "Could not receive the complete request";
            close_connection = true;
            goto cleanup;
        }
        received += (size_t)n;
    }
    content[received] = '\0';
    if (memchr(content, '\0', received)) goto cleanup;

    // Reject escaped NUL: cJSON represents strings as NUL-terminated C strings.
    // Skip escaped backslashes so a literal "\\u0000" is not misinterpreted.
    for (size_t i = 0; i < received; ++i) {
        if (content[i] == '\\' && i + 1 < received) {
            ++i;
            if (content[i] == 'u' && i + 4 < received &&
                memcmp(content + i + 1, "0000", 4) == 0) goto cleanup;
        }
    }
    // Reject trailing non-whitespace data as well as invalid JSON.
    json = cJSON_ParseWithLengthOpts(content, received + 1, NULL, true);
    if (!cJSON_IsObject(json)) goto cleanup;

    // Require exactly one string value for each field, including empty password.
    const char *keys[] = {"device_id", "device_serial", "ssid", "password"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        unsigned count = 0;
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, json) {
            if (item->string && strcmp(item->string, keys[i]) == 0) ++count;
        }
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(json, keys[i]);
        if (count != 1 || !cJSON_IsString(value) || !value->valuestring) {
            message = "Provide device_id, device_serial, ssid and password as unique string fields";
            goto cleanup;
        }
    }
    // This helper returns a result only; the handler must send its own response.
    if (validate_device_identity_from_json(json) != ESP_OK) {
        http_status = "403 Forbidden";
        message = "Device identity validation failed";
        goto cleanup;
    }

    const cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    password_item = cJSON_GetObjectItemCaseSensitive(json, "password");
    size_t ssid_len = strlen(ssid_item->valuestring);
    size_t password_len = strlen(password_item->valuestring);
    if (ssid_len == 0 || ssid_len > sizeof(wifi_config.sta.ssid)) {
        message = "SSID must contain 1-32 bytes";
        goto cleanup;
    }
    // Open networks or WPA/WPA2/WPA3 personal credentials; no enterprise EAP.
    if (password_len != 0 && (password_len < 8 || password_len > 64)) {
        message = "Password must be empty, 8-63 ASCII characters, or 64 hexadecimal digits";
        goto cleanup;
    }
    for (size_t i = 0; i < password_len; ++i) {
        unsigned char c = (unsigned char)password_item->valuestring[i];
        if ((password_len == 64 && !isxdigit(c)) ||
            (password_len != 64 && (c < 32 || c > 126))) {
            message = "Password must be empty, 8-63 ASCII characters, or 64 hexadecimal digits";
            goto cleanup;
        }
    }

#if !CONFIG_ESP_WIFI_NVS_ENABLED
    http_status = "500 Internal Server Error";
    message = "Wi-Fi NVS storage is disabled in the firmware configuration";
    goto cleanup;
#endif

    wifi_mode_t mode;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK || (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA)) {
        http_status = "409 Conflict";
        message = "Initialize Wi-Fi in STA or APSTA mode before saving credentials";
        goto cleanup;
    }
    // Full-length SSID/PSK arrays need not be NUL-terminated in wifi_config_t.
    memcpy(wifi_config.sta.ssid, ssid_item->valuestring, ssid_len);
    memcpy(wifi_config.sta.password, password_item->valuestring, password_len);
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    err = esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        // Never log the request body, Wi-Fi password, or device serial.
        ESP_LOGE(TAG, "Saving Wi-Fi credentials failed: %s", esp_err_to_name(err));
        if (err == ESP_ERR_WIFI_STATE) {
            http_status = "409 Conflict";
            message = "Wi-Fi is busy connecting; retry when the connection attempt has stopped";
        } else if (err == ESP_ERR_WIFI_PASSWORD || err == ESP_ERR_INVALID_ARG) {
            message = "Wi-Fi driver rejected the supplied credentials";
        } else {
            http_status = "500 Internal Server Error";
            message = "Failed to save Wi-Fi credentials";
        }
        goto cleanup;
    }
    saved = true;
    http_status = "200 OK";
    message = "Wi-Fi credentials saved";
    ESP_LOGI(TAG, "Wi-Fi STA credentials saved; connection has not been tested");

cleanup:
    // Wipe all parsed top-level string values, including identity and password.
    if (cJSON_IsObject(json)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, json) {
            if (cJSON_IsString(item) && item->valuestring)
                wifi_provision_clear(item->valuestring, strlen(item->valuestring));
        }
    }
    cJSON_Delete(json);
    wifi_provision_clear(content, sizeof(content));
    wifi_provision_clear(&wifi_config, sizeof(wifi_config));

    // Messages above are fixed literals, never interpolated user input.
    char response[320];
    snprintf(response, sizeof(response),
             "{\"status\":%d,\"message\":\"%s\",\"reboot_required\":%s,\"connection_tested\":false}",
             saved ? 0 : 1, message, saved ? "true" : "false");
    httpd_resp_set_status(req, http_status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (close_connection) httpd_resp_set_hdr(req, "Connection", "close");
    esp_err_t result = httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return close_connection ? ESP_FAIL : result;
}