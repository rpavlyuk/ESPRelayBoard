#include "esp_log.h"

#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_spiffs.h"
#include "ca_cert_manager.h"

#include "system_ota_update.h"

#if !defined(BUFFSIZE)
#define BUFFSIZE 1024
#endif
static char ota_write_data[BUFFSIZE + 1] = { 0 };

/**
 * @brief Perform an OTA update from the given URL.
 *
 * This function initiates an OTA update process using the URL provided.
 * It configures the OTA client, downloads the firmware, and flashes it to the device.
 * Upon successful completion, the device will automatically reboot.
 *
 * @param url The URL of the firmware binary to download.
 * @return ESP_OK if the OTA update was successful, or an error code otherwise.
 */
esp_err_t perform_ota_update(const char *url) {
    ESP_LOGI(OTA_TAG, "Starting OTA update from URL: %s", url);
    
    char *ca_cert = NULL;
    
    // Load the CA root certificate
    if (load_ca_certificate(&ca_cert, CA_CERT_PATH_HTTPS) != ESP_OK) {
        ESP_LOGW(OTA_TAG, "Failed to load CA certificate. Proceeding without it.");
    }

    // Configure HTTP client for OTA update
    esp_http_client_config_t ota_https_client_config = {
        .url = url,
        .timeout_ms = 5000,  // Timeout for OTA update
        .cert_pem = ca_cert  // Set CA certificate if loaded, otherwise NULL
    };

    // Configure OTA update
    esp_https_ota_config_t ota_config = {
        .http_config = &ota_https_client_config,
    };

    // Start the OTA update process
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(OTA_TAG, "OTA firmware image update successful!");
    } else {
        ESP_LOGE(OTA_TAG, "OTA firmware image update failed: %s", esp_err_to_name(ret));
    }

    // Free the CA certificate memory if it was dynamically allocated
    if (ca_cert) {
        free(ca_cert);
    }

    return ret;
}

/**
 * @brief Generate OTA Storage Update URL based on the OTA Firmware Update URL.
 *
 * This function extracts the base path from the OTA Firmware Update URL and appends "Storage.bin".
 *
 * @param firmware_url The OTA Firmware Update URL.
 * @param[out] storage_url Pointer to dynamically allocated string containing the OTA Storage Update URL.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if inputs are invalid, ESP_ERR_NO_MEM if memory allocation fails.
 */
esp_err_t generate_storage_update_url(const char *firmware_url, char **storage_url) {
    if (firmware_url == NULL || storage_url == NULL) {
        ESP_LOGE(OTA_TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    // Find the last '/' in the firmware URL to extract the base path
    const char *last_slash = strrchr(firmware_url, '/');
    if (last_slash == NULL) {
        ESP_LOGE(OTA_TAG, "Malformed URL: %s", firmware_url);
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate the base URL length (everything up to and including the last '/')
    size_t base_length = last_slash - firmware_url + 1;

    // Allocate memory for the new URL (base length + OTA_STORAGE_IMAGE_NAME + null terminator)
    *storage_url = malloc(base_length + strlen(OTA_STORAGE_IMAGE_NAME) + 1);
    if (*storage_url == NULL) {
        ESP_LOGE(OTA_TAG, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }

    // Copy the base URL and append OTA_STORAGE_IMAGE_NAME
    strncpy(*storage_url, firmware_url, base_length);
    strcpy(*storage_url + base_length, OTA_STORAGE_IMAGE_NAME);

    ESP_LOGI(OTA_TAG, "Generated Storage Update URL: %s", *storage_url);
    return ESP_OK;
}

/**
 * @brief Download and update the SPIFFS partition with the data from the given URL.
 *
 * This function downloads the partition image from the given URL and writes it to the SPIFFS.
 *
 * @param url The URL of the storage partition image to download.
 * @return ESP_OK if the SPIFFS partition was updated successfully, or an error code otherwise.
 */

esp_err_t download_and_update_spiffs_partition(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(OTA_TAG, "Starting OTA storage update: %s", url);

    esp_err_t ret = ESP_FAIL;
    esp_http_client_handle_t client = NULL;
    char *ca_cert = NULL;

    bool was_mounted = false;
    bool unmounted = false;
    bool flash_touched = false;

    size_t offset = 0;
    int data_read = 0;

    // Find the actual storage partition on this device.
    const esp_partition_t *spiffs_partition =
        esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
            "storage"
        );

    if (spiffs_partition == NULL) {
        ESP_LOGE(OTA_TAG, "SPIFFS partition not found");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(OTA_TAG,
             "SPIFFS: address=0x%lx, size=%lu bytes",
             (unsigned long)spiffs_partition->address,
             (unsigned long)spiffs_partition->size);

    // Safe remount configuration.
    // Never automatically format after an OTA update.
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };

    // Load CA certificate while SPIFFS is still mounted.
    if (load_ca_certificate(&ca_cert, CA_CERT_PATH_HTTPS)
            != ESP_OK) {
        ESP_LOGW(OTA_TAG,
                 "Failed to load CA certificate");
        // Preserve existing certificate fallback behavior.
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
        .cert_pem = ca_cert
    };

    /*
     * STEP 1: HEAD request
     */
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    esp_http_client_set_method(client, HTTP_METHOD_HEAD);

    ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(OTA_TAG, "HEAD connection failed: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }

    int64_t head_length = esp_http_client_fetch_headers(client);

    if (head_length < 0 ||
        esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(OTA_TAG, "HEAD request failed");
        ret = ESP_FAIL;
        goto cleanup;
    }

    // Optional early rejection when HEAD supplies a size.
    if (head_length > 0 &&
        (uint64_t)head_length != spiffs_partition->size) {

        ESP_LOGE(OTA_TAG,
                 "SPIFFS image size mismatch (HEAD): "
                 "image=%lld, partition=%lu",
                 (long long)head_length,
                 (unsigned long)spiffs_partition->size);

        ret = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }

    esp_http_client_cleanup(client);
    client = NULL;

    /*
     * STEP 2: GET request
     */
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    esp_http_client_set_method(client, HTTP_METHOD_GET);

    ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(OTA_TAG, "GET connection failed: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }

    int64_t content_length =
        esp_http_client_fetch_headers(client);

    if (content_length < 0 ||
        esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(OTA_TAG, "GET request failed");
        ret = ESP_FAIL;
        goto cleanup;
    }

    /*
     * STEP 3: Validate image size BEFORE touching flash.
     *
     * Full SPIFFS images must match the partition size.
     */
    if (content_length <= 0 ||
        (uint64_t)content_length != spiffs_partition->size) {

        ESP_LOGE(OTA_TAG,
                 "SPIFFS image size mismatch: "
                 "image=%lld, partition=%lu",
                 (long long)content_length,
                 (unsigned long)spiffs_partition->size);

        ret = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }

    ESP_LOGI(OTA_TAG,
             "SPIFFS image size verified: %lld bytes",
             (long long)content_length);

    /*
     * STEP 4: Read the first chunk before erasing.
     */
    data_read = esp_http_client_read(
        client, ota_write_data, BUFFSIZE);

    if (data_read <= 0) {
        ESP_LOGE(OTA_TAG,
                 "Failed to read first image chunk: %d",
                 data_read);
        ret = ESP_FAIL;
        goto cleanup;
    }

    /*
     * STEP 5: Unmount SPIFFS before raw flash access.
     *
     * All other SPIFFS users must be stopped beforehand.
     */
    was_mounted = esp_spiffs_mounted(NULL);

    if (was_mounted) {
        ESP_LOGI(OTA_TAG, "Unmounting SPIFFS");

        ret = esp_vfs_spiffs_unregister(NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(OTA_TAG,
                     "SPIFFS unmount failed: %s",
                     esp_err_to_name(ret));
            goto cleanup;
        }

        unmounted = true;
    }

    /*
     * STEP 6: Erase the partition.
     *
     * From this point onward, failures may leave SPIFFS
     * unusable. Do not automatically remount on failure.
     */
    flash_touched = true;

    ret = esp_partition_erase_range(
        spiffs_partition, 0, spiffs_partition->size);

    if (ret != ESP_OK) {
        ESP_LOGE(OTA_TAG,
                 "SPIFFS erase failed: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(OTA_TAG, "SPIFFS partition erased");

    /*
     * STEP 7: Stream the image into flash.
     */
    offset = 0;

    while (offset < spiffs_partition->size) {

        // Detect premature end of stream or read errors.
        if (data_read <= 0) {
            ESP_LOGE(OTA_TAG,
                     "Incomplete SPIFFS image at offset %zu",
                     offset);
            ret = ESP_FAIL;
            goto cleanup;
        }

        // Prevent writing beyond the partition boundary.
        if ((size_t)data_read >
            spiffs_partition->size - offset) {

            ESP_LOGE(OTA_TAG,
                     "SPIFFS write exceeds partition boundary");
            ret = ESP_ERR_INVALID_SIZE;
            goto cleanup;
        }

        ret = esp_partition_write(
            spiffs_partition,
            offset,
            ota_write_data,
            (size_t)data_read
        );

        if (ret != ESP_OK) {
            ESP_LOGE(OTA_TAG,
                     "SPIFFS write failed at %zu: %s",
                     offset, esp_err_to_name(ret));
            goto cleanup;
        }

        offset += (size_t)data_read;

        // Entire expected image has been written.
        if (offset == spiffs_partition->size) {
            break;
        }

        // Never request more than the remaining space.
        size_t remaining = spiffs_partition->size - offset;
        int read_size = remaining < BUFFSIZE
                      ? (int)remaining
                      : BUFFSIZE;

        data_read = esp_http_client_read(
            client, ota_write_data, read_size);
    }

    /*
     * STEP 8: Verify complete HTTP transfer.
     */
    if (offset != spiffs_partition->size ||
        !esp_http_client_is_complete_data_received(client)) {

        ESP_LOGE(OTA_TAG,
                 "Incomplete SPIFFS transfer: wrote %zu/%lu",
                 offset,
                 (unsigned long)spiffs_partition->size);

        ret = ESP_FAIL;
        goto cleanup;
    }

    ESP_LOGI(OTA_TAG,
             "SPIFFS image written successfully: %zu bytes",
             offset);

    ret = ESP_OK;

cleanup:
    /*
     * STEP 9: Release HTTP and certificate resources.
     */
    if (client != NULL) {
        esp_http_client_cleanup(client);
    }

    free(ca_cert);

    /*
     * STEP 10: Restore SPIFFS if safe.
     *
     * Remount after a complete update, or if nothing
     * was erased/written. Never format automatically.
     */
    if (unmounted) {

        if (ret == ESP_OK || !flash_touched) {

            ESP_LOGI(OTA_TAG, "Remounting SPIFFS");

            esp_err_t mount_ret =
                esp_vfs_spiffs_register(&spiffs_conf);

            if (mount_ret != ESP_OK) {
                ESP_LOGE(OTA_TAG,
                         "SPIFFS remount failed: %s",
                         esp_err_to_name(mount_ret));
                ret = mount_ret;
            } else {
                ESP_LOGI(OTA_TAG,
                         "SPIFFS remounted successfully");
            }

        } else {
            ESP_LOGE(OTA_TAG,
                     "SPIFFS update failed after flash "
                     "modification. Leaving filesystem "
                     "unmounted to prevent further damage.");
        }
    }

    return ret;
}


/**
 * @file: settings.c
 * @brief Check the OTA partitions for the current running partition and its state.
 * 
 * This function checks the OTA partitions to determine the current running partition
 * and its state. It prints the running partition label and the OTA state to the console.
 * 
 * @return ESP_OK if the OTA partitions were checked successfully, or an error code otherwise.
 */
esp_err_t check_ota_partitions(void) {

    ESP_LOGI(OTA_TAG, "Checking OTA partitions");

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_LOGE(OTA_TAG, "Running partition not found!");
        return ESP_ERR_OTA_BASE;
    } else {
        ESP_LOGI(OTA_TAG, "Running from partition: %s", running->label);
    }

    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        ESP_LOGI(OTA_TAG, "OTA partition state: %d", ota_state);
    } else {
        ESP_LOGE(OTA_TAG, "Failed to get OTA state");
    }

    return ESP_OK;
}
