# OTA Update Helper Functions

## Functions

### `perform_ota_update`

```c
esp_err_t perform_ota_update(const char *url);
```

Performs an OTA firmware update from the given URL. Loads the CA certificate (if available), configures the HTTPS client, downloads the firmware and flashes it. On success the device reboots automatically.

- **url**: URL of the firmware binary.
- **Returns**: `ESP_OK` on success, or an error code otherwise.

### `generate_storage_update_url`

```c
esp_err_t generate_storage_update_url(const char *firmware_url, char **storage_url);
```

Builds the storage image URL by taking the base path of the firmware URL and appending `OTA_STORAGE_IMAGE_NAME`. Memory for `*storage_url` is allocated dynamically and must be freed by the caller.

- **firmware_url**: OTA firmware update URL.
- **storage_url** (out): Receives the allocated storage update URL.
- **Returns**: `ESP_OK` on success, `ESP_ERR_INVALID_ARG` for invalid input or a malformed URL, `ESP_ERR_NO_MEM` if allocation fails.

### `download_and_update_spiffs_partition`

```c
esp_err_t download_and_update_spiffs_partition(const char *url);
```

Downloads a full SPIFFS partition image and writes it to the `storage` partition.

Steps:
1. Locate the `storage` SPIFFS partition.
2. Load the CA certificate while SPIFFS is mounted.
3. Send a HEAD request and validate the size.
4. Send a GET request and validate the size against the partition.
5. Read the first chunk before erasing.
6. Unmount SPIFFS, erase the partition and stream the image into flash.
7. Verify the complete transfer.
8. Release resources and remount SPIFFS if it is safe (never auto-formats).

- **url**: URL of the storage partition image.
- **Returns**: `ESP_OK` on success, or an error code otherwise (`ESP_ERR_INVALID_ARG`, `ESP_ERR_NOT_FOUND`, `ESP_ERR_INVALID_SIZE`, `ESP_FAIL`, etc.).

> **Warning:** If a failure occurs after flash has been modified, SPIFFS is intentionally left unmounted.

### `check_ota_partitions`

```c
esp_err_t check_ota_partitions(void);
```

Checks the currently running OTA partition and logs its label and OTA image state.

- **Returns**: `ESP_OK` if the check completes, `ESP_ERR_OTA_BASE` if the running partition cannot be found.

## Usage Examples

### Firmware update task (based on `ota_update_task`)

Run the update from a FreeRTOS task so the HTTP server and Wi-Fi stack are not blocked. The task frees its parameters and reboots on success.

```c
void ota_update_task(void *param) {
    ota_update_param_t *update_param = (ota_update_param_t *)param;

    esp_err_t ret = perform_ota_update(update_param->ota_url);

    if (ret == ESP_OK) {
        if (DO_OTA_STORAGE_UPDATE) {
            char *storage_url = NULL;
            if (generate_storage_update_url(update_param->ota_url, &storage_url) == ESP_OK) {
                ret = download_and_update_spiffs_partition(storage_url);
                if (ret != ESP_OK) {
                    ESP_LOGE(OTA_TAG, "SPIFFS update failed: %s", esp_err_to_name(ret));
                }
                free(storage_url);
            } else {
                ESP_LOGE(OTA_TAG, "Failed to generate storage URL");
            }
        }
        system_reboot();
    } else {
        ESP_LOGE(OTA_TAG, "OTA update failed: %s", esp_err_to_name(ret));
    }

    free(update_param);
    vTaskDelete(NULL);
}
```

Starting the task:

```c
ota_update_param_t *param = malloc(sizeof(ota_update_param_t));
if (param != NULL) {
    strlcpy(param->ota_url, url, sizeof(param->ota_url));
    xTaskCreate(ota_update_task, "ota_update_task", 8192, param, 5, NULL);
}
```

### `generate_storage_update_url`

```c
char *storage_url = NULL;
esp_err_t err = generate_storage_update_url("https://example.com/fw/app.bin", &storage_url);
if (err == ESP_OK) {
    ESP_LOGI(TAG, "Storage URL: %s", storage_url); // https://example.com/fw/<OTA_STORAGE_IMAGE_NAME>
    free(storage_url); // caller must free
} else {
    ESP_LOGE(TAG, "Failed to build storage URL: %s", esp_err_to_name(err));
}
```

### `download_and_update_spiffs_partition`

```c
char *storage_url = NULL;
if (generate_storage_update_url(firmware_url, &storage_url) == ESP_OK) {
    esp_err_t err = download_and_update_spiffs_partition(storage_url);
    if (err != ESP_OK) {
        // Note: SPIFFS may remain unmounted if flash was already modified.
        ESP_LOGE(TAG, "SPIFFS update failed: %s", esp_err_to_name(err));
    }
    free(storage_url);
}
```

### `check_ota_partitions`

```c
esp_err_t err = check_ota_partitions();
if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA partition check failed: %s", esp_err_to_name(err));
}
```

## Integration

Add the component to your component's `CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "src/my_component.c"
    INCLUDE_DIRS "include"
    REQUIRES system_ota_update
)
```

## License

This project is licensed under the [MIT License](LICENSE).

Author: Roman Pavlyuk <roman@pavlyuk.consulting>
