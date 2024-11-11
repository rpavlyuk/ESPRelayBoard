#include <ctype.h>
#include "esp_spiffs.h"  // Include for SPIFFS
#include "esp_vfs.h"
#include "esp_vfs_fat.h"

#include "esp_http_server.h"
#include "non_volatile_storage.h"
#include "ca_cert_manager.h"
#include "cJSON.h"

#include "common.h"
#include "settings.h"
#include "relay.h"
#include "web.h"
#include "status.h"
// #include "hass.h"
#include "mqtt.h"
#include "wifi.h"

static httpd_handle_t server = NULL;

/**
 * @brief: Run the HTTP server
 * 
 * This function creates an HTTP server and registers the necessary URI handlers.
 * It waits for the Wi-Fi to connect before starting the server.
 * 
 * @param[in] param: Task parameters (unused)
 * 
 * @return void No return value
 */
void run_http_server(void *param) {

    // wait for Wi-Fi to connect
    while(!g_wifi_ready) {
        ESP_LOGI(TAG, "Waiting for Wi-Fi/network to become ready...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started. Registering handlers...");
        // Set URI handlers
        httpd_uri_t root_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = config_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        // Set URI handlers
        httpd_uri_t config_uri = {
            .uri       = "/config",
            .method    = HTTP_GET,
            .handler   = config_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &config_uri);

        httpd_uri_t submit_uri = {
            .uri       = "/submit",
            .method    = HTTP_POST,
            .handler   = submit_post_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &submit_uri);

        httpd_uri_t ca_cert_uri = {
            .uri       = "/ca-cert",
            .method    = HTTP_POST,
            .handler   = ca_cert_post_handler,
            .user_ctx  = NULL
        };

        // Register the handler
        httpd_register_uri_handler(server, &ca_cert_uri);

        // URI handler for reboot action
        httpd_uri_t reboot_uri = {
            .uri = "/reboot",
            .method = HTTP_POST,
            .handler = reboot_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &reboot_uri);

        // URI handler for relays action
        httpd_uri_t relays_uri = {
            .uri = "/relays",
            .method = HTTP_GET,
            .handler = relays_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &relays_uri);     

        // Register the /update-relay POST handler
        httpd_uri_t update_relay_uri = {
            .uri      = "/api/relay/update",   
            .method   = HTTP_POST,         // Only accept POST requests
            .handler  = update_relay_post_handler, // Handler function
            .user_ctx = NULL              
        };

        httpd_register_uri_handler(server, &update_relay_uri);  

        httpd_uri_t status_uri = {
            .uri       = "/status",
            .method    = HTTP_GET,
            .handler   = status_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status_uri);
       
        // Register the status web service handler
        httpd_uri_t status_webserver_get_uri = {
            .uri       = "/api/status",
            .method    = HTTP_GET,
            .handler   = status_data_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status_webserver_get_uri);

        // Register the relays data web service handler
        httpd_uri_t relays_data = {
            .uri      = "/api/relays",           
            .method   = HTTP_GET,                
            .handler  = relays_data_get_handler, 
            .user_ctx = NULL                     
        };
        httpd_register_uri_handler(server, &relays_data);

        httpd_uri_t ota_update_uri = {
            .uri      = "/ota-update",  // URL endpoint
            .method   = HTTP_POST,       // HTTP method
            .handler  = ota_post_handler, // Function to handle the request
            .user_ctx = NULL            // User context, if needed
        };

        // Register the OTA update URI handler
        httpd_register_uri_handler(server, &ota_update_uri);

        httpd_uri_t reset_uri = {
            .uri      = "/reset",  // URL endpoint
            .method   = HTTP_POST,       // HTTP method
            .handler  = reset_post_handler, // Function to handle the request
            .user_ctx = NULL            // User context, if needed
        };

        // Register the reset URI handler
        httpd_register_uri_handler(server, &reset_uri);

        
        ESP_LOGI(TAG, "HTTP handlers registered. Server ready!");
    } else {
        ESP_LOGI(TAG, "Error starting server!");
        return;
    }

    while(server) {
        vTaskDelay(5);
    }  
}

/**
 * @brief: Helper function to fill in the static variables in the template
 * 
 * This function replaces the placeholders in the HTML template with the actual values.
 * 
 * @param[in,out] html_output: The HTML template to modify
 * 
 * @return void No return value
 */
void assign_static_page_variables(char *html_output) {

    // replace size fields
    char f_len[10];
    snprintf(f_len, sizeof(f_len), "%i", MQTT_SERVER_LENGTH);
    replace_placeholder(html_output, "{LEN_MQTT_SERVER}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", MQTT_PROTOCOL_LENGTH);
    replace_placeholder(html_output, "{LEN_MQTT_PROTOCOL}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", MQTT_USER_LENGTH);
    replace_placeholder(html_output, "{LEN_MQTT_USER}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", MQTT_PASSWORD_LENGTH);
    replace_placeholder(html_output, "{LEN_MQTT_PASSWORD}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", MQTT_PREFIX_LENGTH);
    replace_placeholder(html_output, "{LEN_MQTT_PREFIX}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", HA_PREFIX_LENGTH);
    replace_placeholder(html_output, "{LEN_HA_PREFIX}", f_len);
    
    snprintf(f_len, sizeof(f_len), "%li", (long int) HA_UPDATE_INTERVAL_MIN);
    replace_placeholder(html_output, "{MIN_HA_UPDATE_INTERVAL}", f_len);
    snprintf(f_len, sizeof(f_len), "%li", (long int) HA_UPDATE_INTERVAL_MAX);
    replace_placeholder(html_output, "{MAX_HA_UPDATE_INTERVAL}", f_len); 

    snprintf(f_len, sizeof(f_len), "%i", RELAY_REFRESH_INTERVAL_MIN);
    replace_placeholder(html_output, "{MIN_RELAY_REFRESH_INTERVAL}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", RELAY_REFRESH_INTERVAL_MAX);
    replace_placeholder(html_output, "{MAX_RELAY_REFRESH_INTERVAL}", f_len);

    snprintf(f_len, sizeof(f_len), "%i", CHANNEL_COUNT_MIN+1);
    replace_placeholder(html_output, "{MIN_RELAY_CHANNEL_COUNT}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", CHANNEL_COUNT_MAX+1);
    replace_placeholder(html_output, "{MAX_RELAY_CHANNEL_COUNT}", f_len); 

    snprintf(f_len, sizeof(f_len), "%i", CONTACT_SENSORS_COUNT_MIN);
    replace_placeholder(html_output, "{MIN_CONTACT_SENSORS_COUNT}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", CONTACT_SENSORS_COUNT_MAX);
    replace_placeholder(html_output, "{MAX_CONTACT_SENSORS_COUNT}", f_len);

    snprintf(f_len, sizeof(f_len), "%i", RELAY_GPIO_PIN_MIN);
    replace_placeholder(html_output, "{MIN_RELAY_GPIO_PIN}", f_len);
    snprintf(f_len, sizeof(f_len), "%i", RELAY_GPIO_PIN_MAX);
    replace_placeholder(html_output, "{MAX_RELAY_GPIO_PIN}", f_len);

    snprintf(f_len, sizeof(f_len), "%i", CA_CERT_LENGTH);
    replace_placeholder(html_output, "{VAL_CA_CERT_LEN_MAX}", f_len);

    replace_placeholder(html_output, "{VAL_SW_VERSION}", DEVICE_SW_VERSION);
}

/**
 * @brief: Helper function to replace placeholders in the template with actual values
 * 
 * This function replaces the placeholders in the HTML template with the actual values.
 * 
 * @param[in,out] html_output: The HTML template to modify
 * @param[in] placeholder: The placeholder to replace
 * @param[in] value: The value to insert
 * 
 * @return void No return value
 */ 
void replace_placeholder(char *html_output, const char *placeholder, const char *value) {
    char *pos;
    while ((pos = strstr(html_output, placeholder)) != NULL) {
        size_t len_before = pos - html_output;
        size_t len_placeholder = strlen(placeholder);
        size_t len_value = strlen(value);
        size_t len_after = strlen(pos + len_placeholder);

        // Shift the rest of the string to make space for the replacement value
        memmove(pos + len_value, pos + len_placeholder, len_after + 1);

        // Copy the replacement value into the position of the placeholder
        memcpy(pos, value, len_value);
    }
}

/**
 * @brief: Helper function to truncate a string after the last occurrence of a substring
 * 
 * This function truncates a string after the last occurrence of a substring.
 * 
 * @param[in,out] str: The string to truncate
 * @param[in] lookup: The substring to search for
 * 
 * @return void No return value
 */
void str_trunc_after(char *str, const char *lookup) {
    if (str == NULL || lookup == NULL) {
        return; // Return if either input is NULL
    }

    char *last_occurrence = NULL;
    char *current_position = str;

    // Find the last occurrence of the substring 'lookup' in 'str'
    while ((current_position = strstr(current_position, lookup)) != NULL) {
        last_occurrence = current_position;
        current_position += strlen(lookup); // Move past the current match
    }

    // If the 'lookup' substring was found, truncate the string after its last occurrence
    if (last_occurrence != NULL) {
        last_occurrence[strlen(lookup)] = '\0'; // Null-terminate after the last occurrence
    }
}

/**
 * @file web.c
 * @brief: Helper function to convert a hexadecimal character to its decimal value
 * 
 * This function takes a single hexadecimal character and converts it to its decimal value.
 * It supports both uppercase and lowercase hexadecimal characters.
 * 
 * @param[in] c: The hexadecimal character to convert
 * 
 * @return int: The decimal value of the hexadecimal character, or -1 if the input is not a valid hexadecimal character
 */
int hex_to_dec(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * @file web.c
 * @brief URL decoding function
 * 
 * This file contains the implementation of the URL decoding function.
 * 
 */
void url_decode(char *src) {
    char *dst = src;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            // Convert hex to character
            *dst = (char)((hex_to_dec(src[1]) << 4) | hex_to_dec(src[2]));
            src += 2;
        } else if (*src == '+') {
            // Replace '+' with space
            *dst = ' ';
        } else {
            *dst = *src;
        }
        src++;
        dst++;
    }
    *dst = '\0'; // Null-terminate the decoded string
}

/**
 * @brief Extracts the value of a specified parameter from a buffer.
 *
 * This function searches for a parameter name within a given buffer and extracts its corresponding value.
 * The extracted value is then stored in the provided output buffer.
 *
 * @param buf The buffer containing the parameters.
 * @param param_name The name of the parameter to search for.
 * @param output The buffer where the extracted parameter value will be stored.
 * @param output_size The size of the output buffer.
 * @return An integer indicating the success or failure of the extraction.
 */
int extract_param_value(const char *buf, const char *param_name, char *output, size_t output_size) {
    char *start = strstr(buf, param_name);
    if (start != NULL) {
        start += strlen(param_name);  // Move to the value part
        char *end = strchr(start, '&');  // Find the next '&'
        if (end == NULL) {
            end = start + strlen(start);  // If no '&' found, take till the end of the string
        }
        size_t len = end - start;
        if (len >= output_size) {
            len = output_size - 1;  // Ensure we don't overflow the output buffer
        }
        strncpy(output, start, len);
        output[len] = '\0';  // Null-terminate the result
        return len;  // Return the length of the extracted value
    } else {
        output[0] = '\0';  // If not found, return an empty string
        return 0;  // Return 0 length when not found
    }
}

/* HANDLERS */
/**
 * @brief Calculates the factorial of a given number.
 *
 * This function takes an integer as input and returns the factorial of that number.
 * The factorial of a non-negative integer n is the product of all positive integers less than or equal to n.
 * For example, the factorial of 5 is 5 * 4 * 3 * 2 * 1 = 120.
 *
 * @param n The integer for which the factorial is to be calculated. Must be non-negative.
 * @return The factorial of the input number. If the input is 0, the function returns 1.
 */
static esp_err_t config_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Processing config web request");

    // empty message
    const char* message = "";

    // Allocate memory dynamically for template and output
    char *html_template = (char *)malloc(MAX_TEMPLATE_SIZE);
    char *html_output = (char *)malloc(MAX_TEMPLATE_SIZE);

    if (html_template == NULL || html_output == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        if (html_template) free(html_template);
        if (html_output) free(html_output);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/config.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        free(html_template);
        free(html_output);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Load the template into html_template
    size_t len = fread(html_template, 1, MAX_TEMPLATE_SIZE, f);
    fclose(f);
    html_template[len] = '\0';  // Null-terminate the string

    // Copy template into html_output for modification
    strcpy(html_output, html_template);

    // Allocate memory for the strings you will retrieve from NVS
    char *mqtt_server = NULL;
    char *mqtt_protocol = NULL;
    char *mqtt_user = NULL;
    char *mqtt_password = NULL;
    char *mqtt_prefix = NULL;
    char *ha_prefix = NULL;
    char *device_id = NULL;
    char *device_serial = NULL;
    char *ota_update_url = NULL;

    uint16_t mqtt_connect;
    uint16_t mqtt_port;

    uint32_t ha_upd_intervl;

    uint16_t relay_refr_int;
    uint16_t relay_ch_count;
    uint16_t relay_sn_count;

    // Load settings from NVS (use default values if not set)
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connect));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_SERVER, &mqtt_server));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, &mqtt_port));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, &mqtt_protocol));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_USER, &mqtt_user));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, &mqtt_password));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_HA_PREFIX, &ha_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    ESP_ERROR_CHECK(nvs_read_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, &ha_upd_intervl));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_RELAY_REFRESH_INTERVAL, &relay_refr_int));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_CHANNEL_COUNT, &relay_ch_count));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_CONTACT_SENSORS_COUNT, &relay_sn_count));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_OTA_UPDATE_URL, &ota_update_url));

    // Load MQTTS CA certificate
    char *ca_cert_mqtts = NULL;
    if (load_ca_certificate(&ca_cert_mqtts, CA_CERT_PATH_MQTTS) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load MQTTS CA certificate from %s", CA_CERT_PATH_MQTTS);
        if (html_template) free(html_template);
        if (html_output) free(html_output);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    } else {
        ESP_LOGD(TAG, "Loaded MQTTS CA certificate: %s", (ca_cert_mqtts == NULL) ? "NULL" : ca_cert_mqtts);
    }
    // Load HTTPS CA certificate
    char *ca_cert_https = NULL;
    if (load_ca_certificate(&ca_cert_https, CA_CERT_PATH_HTTPS) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load HTTPS CA certificate from %s", CA_CERT_PATH_HTTPS);
        if (html_template) free(html_template);
        if (html_output) free(html_output);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    } else {
        ESP_LOGD(TAG, "Loaded HTTPS CA certificate: %s", (ca_cert_https == NULL) ? "NULL" : ca_cert_https);
    }

    // Replace placeholders in the template with actual values
    char mqtt_port_str[6];
    char ha_upd_intervl_str[10];
    char relay_refr_int_str[10];
    char mqtt_connect_str[10];
    char relay_ch_count_str[10];
    char relay_sn_count_str[10];

    snprintf(mqtt_port_str, sizeof(mqtt_port_str), "%u", mqtt_port);
    snprintf(ha_upd_intervl_str, sizeof(ha_upd_intervl_str), "%li", (uint32_t) ha_upd_intervl);
    snprintf(mqtt_connect_str, sizeof(mqtt_connect_str), "%i", (uint16_t) mqtt_connect);
    snprintf(relay_refr_int_str, sizeof(relay_refr_int_str), "%i", (uint16_t) relay_refr_int);
    snprintf(relay_ch_count_str, sizeof(relay_ch_count_str), "%i", (uint16_t) relay_ch_count);
    snprintf(relay_sn_count_str, sizeof(relay_sn_count_str), "%i", (uint16_t) relay_sn_count);

    replace_placeholder(html_output, "{VAL_DEVICE_ID}", device_id);
    replace_placeholder(html_output, "{VAL_DEVICE_SERIAL}", device_serial);
    replace_placeholder(html_output, "{VAL_MQTT_SERVER}", mqtt_server);
    replace_placeholder(html_output, "{VAL_MQTT_PORT}", mqtt_port_str);
    replace_placeholder(html_output, "{VAL_MQTT_PROTOCOL}", mqtt_protocol);
    replace_placeholder(html_output, "{VAL_MQTT_USER}", mqtt_user);
    replace_placeholder(html_output, "{VAL_MQTT_PASSWORD}", mqtt_password);
    replace_placeholder(html_output, "{VAL_MQTT_PREFIX}", mqtt_prefix);
    replace_placeholder(html_output, "{VAL_HA_PREFIX}", ha_prefix);
    replace_placeholder(html_output, "{VAL_MESSAGE}", message);
    replace_placeholder(html_output, "{VAL_HA_UPDATE_INTERVAL}", ha_upd_intervl_str);
    replace_placeholder(html_output, "{VAL_MQTT_CONNECT}", mqtt_connect_str);
    replace_placeholder(html_output, "{VAL_CA_CERT_MQTTS}", ca_cert_mqtts);
    replace_placeholder(html_output, "{VAL_CA_CERT_HTTPS}", ca_cert_https);
    replace_placeholder(html_output, "{VAL_RELAY_REFRESH_INTERVAL}", relay_refr_int_str);
    replace_placeholder(html_output, "{VAL_RELAY_CHANNEL_COUNT}", relay_ch_count_str);
    replace_placeholder(html_output, "{VAL_CONTACT_SENSORS_COUNT}", relay_sn_count_str);
    replace_placeholder(html_output, "{VAL_OTA_UPDATE_URL}", ota_update_url);

    // replace static fields
    assign_static_page_variables(html_output);


    // Send the final HTML response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_output, strlen(html_output));

    // Free dynamically allocated memory
    free(html_template);
    free(html_output);
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(ha_prefix);
    free(device_id);
    free(device_serial);
    free(ca_cert_mqtts);
    free(ca_cert_https);
    free(ota_update_url);

    return ESP_OK;
}

/**
 * @brief: Handler for the POST request to submit the configuration form
 * 
 * This function handles the POST request to submit the configuration form.
 * It extracts the form data, saves the parameters to NVS, and sends a response back to the client.
 * 
 * @param[in] req: The HTTP request object
 * 
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t submit_post_handler(httpd_req_t *req) {
    // Extract form data
    char buf[1024];
    memset(buf, 0, sizeof(buf));  // Initialize the buffer with zeros to avoid any garbage
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }

    // See what we got from client
    ESP_LOGI(TAG, "Received request: %s", buf);

    // empty message
    const char* success_message = "<div class=\"alert alert-primary alert-dismissible fade show\" role=\"alert\"> Parameters saved successfully. A device reboot might be required for the setting to come into effect.<button type=\"button\" class=\"btn-close\" data-bs-dismiss=\"alert\" aria-label=\"Close\"></button></div>";
    
    // Allocate memory dynamically for template and output
    char *html_template = (char *)malloc(MAX_TEMPLATE_SIZE);
    char *html_output = (char *)malloc(MAX_TEMPLATE_SIZE);

    if (html_template == NULL || html_output == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        if (html_template) free(html_template);
        if (html_output) free(html_output);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/config.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        free(html_template);
        free(html_output);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Load the template into html_template
    size_t len = fread(html_template, 1, MAX_TEMPLATE_SIZE, f);
    fclose(f);
    html_template[len] = '\0';  // Null-terminate the string

    // Copy template into html_output for modification
    strcpy(html_output, html_template);

    // Allocate memory for the strings you will retrieve from NVS
    // We need to pre-allocate memory as we are loading those values from POST request
    char *mqtt_server = (char *)malloc(MQTT_SERVER_LENGTH);
    char *mqtt_protocol = (char *)malloc(MQTT_PROTOCOL_LENGTH);
    char *mqtt_user = (char *)malloc(MQTT_USER_LENGTH);
    char *mqtt_password = (char *)malloc(MQTT_PASSWORD_LENGTH);
    char *mqtt_prefix = (char *)malloc(MQTT_PREFIX_LENGTH);
    char *ha_prefix = (char *)malloc(HA_PREFIX_LENGTH);
    char *ota_update_url = (char *)malloc(OTA_UPDATE_URL_LENGTH);

    char mqtt_port_str[6];
    char ha_upd_intervl_str[10];
    char mqtt_connect_str[10];
    char relay_ch_count_str[10];
    char relay_sn_count_str[10];
    char relay_refr_int_str[10];

    // Extract parameters from the buffer
    extract_param_value(buf, "mqtt_server=", mqtt_server, MQTT_SERVER_LENGTH);
    extract_param_value(buf, "mqtt_protocol=", mqtt_protocol, MQTT_PROTOCOL_LENGTH);
    extract_param_value(buf, "mqtt_user=", mqtt_user, MQTT_USER_LENGTH);
    extract_param_value(buf, "mqtt_password=", mqtt_password, MQTT_PASSWORD_LENGTH);
    extract_param_value(buf, "mqtt_prefix=", mqtt_prefix, MQTT_PREFIX_LENGTH);
    extract_param_value(buf, "ha_prefix=", ha_prefix, HA_PREFIX_LENGTH);
    extract_param_value(buf, "mqtt_port=", mqtt_port_str, sizeof(mqtt_port_str));
    extract_param_value(buf, "ha_upd_intervl=", ha_upd_intervl_str, sizeof(ha_upd_intervl_str));
    extract_param_value(buf, "mqtt_connect=", mqtt_connect_str, sizeof(mqtt_connect_str));
    extract_param_value(buf, "relay_ch_count=", relay_ch_count_str, sizeof(relay_ch_count_str));
    extract_param_value(buf, "relay_sn_count=", relay_sn_count_str, sizeof(relay_sn_count_str));
    extract_param_value(buf, "relay_refr_int=", relay_refr_int_str, sizeof(relay_refr_int_str));
    extract_param_value(buf, "ota_update_url=", ota_update_url, OTA_UPDATE_URL_LENGTH);



    // Convert mqtt_port and sensor_offset to their respective types
    uint16_t mqtt_port = (uint16_t)atoi(mqtt_port_str);  // Convert to uint16_t
    uint32_t ha_upd_intervl = (uint32_t)atoi(ha_upd_intervl_str);
    uint16_t mqtt_connect = (uint16_t)atoi(mqtt_connect_str);

    uint16_t relay_refr_int = (uint16_t)atoi(relay_refr_int_str);
    uint16_t relay_ch_count = (uint16_t)atoi(relay_ch_count_str);
    uint16_t relay_sn_count = (uint16_t)atoi(relay_sn_count_str); 

    // Decode potentially URL-encoded parameters
    url_decode(mqtt_server);
    url_decode(mqtt_protocol);
    url_decode(mqtt_user);
    url_decode(mqtt_password);
    url_decode(mqtt_prefix);
    url_decode(ha_prefix);
    url_decode(ota_update_url);

    // dump parameters for debugging pursposes
    ESP_LOGI(TAG, "Received configuration parameters:");
    ESP_LOGI(TAG, "mqtt_connect: %i", mqtt_connect);
    ESP_LOGI(TAG, "mqtt_server: %s", mqtt_server);
    ESP_LOGI(TAG, "mqtt_port: %i", mqtt_port);
    ESP_LOGI(TAG, "mqtt_protocol: %s", mqtt_protocol);
    ESP_LOGI(TAG, "mqtt_user: %s", mqtt_user);
    ESP_LOGI(TAG, "mqtt_password: %s", mqtt_password);
    ESP_LOGI(TAG, "mqtt_prefix: %s", mqtt_prefix);
    ESP_LOGI(TAG, "ha_prefix: %s", ha_prefix);
    ESP_LOGI(TAG, "ha_upd_intervl: %li", ha_upd_intervl);
    ESP_LOGI(TAG, "relay_refr_int: %i", relay_refr_int);
    ESP_LOGI(TAG, "relay_ch_count: %i", relay_ch_count);
    ESP_LOGI(TAG, "relay_sn_count: %i", relay_sn_count);
    ESP_LOGI(TAG, "ota_update_url: %s", ota_update_url);


    // Save parsed values to NVS or apply them directly
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_MQTT_SERVER, mqtt_server));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, mqtt_port));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, mqtt_protocol));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_MQTT_USER, mqtt_user));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, mqtt_password));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, mqtt_prefix));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_HA_PREFIX, ha_prefix));
    ESP_ERROR_CHECK(nvs_write_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, ha_upd_intervl));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, mqtt_connect));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_RELAY_REFRESH_INTERVAL, relay_refr_int));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_CHANNEL_COUNT, relay_ch_count));
    ESP_ERROR_CHECK(nvs_write_uint16(S_NAMESPACE, S_KEY_CONTACT_SENSORS_COUNT, relay_sn_count));
    ESP_ERROR_CHECK(nvs_write_string(S_NAMESPACE, S_KEY_OTA_UPDATE_URL, ota_update_url));


    /** Load and display settings */

    // Free pointers to previosly used strings
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(ha_prefix);
    free(ota_update_url);

    // declaring NULL pointers for neccessary variables
    char *device_id = NULL;
    char *device_serial = NULL;

    // resetting the pointers
    mqtt_server = NULL;
    mqtt_protocol = NULL;
    mqtt_user = NULL;
    mqtt_password = NULL;
    mqtt_prefix = NULL;
    ha_prefix = NULL;
    ota_update_url = NULL;

    // Load settings from NVS (use default values if not set)
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_SERVER, &mqtt_server));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_PORT, &mqtt_port));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PROTOCOL, &mqtt_protocol));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_USER, &mqtt_user));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PASSWORD, &mqtt_password));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_MQTT_PREFIX, &mqtt_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_HA_PREFIX, &ha_prefix));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    ESP_ERROR_CHECK(nvs_read_uint32(S_NAMESPACE, S_KEY_HA_UPDATE_INTERVAL, &ha_upd_intervl));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_MQTT_CONNECT, &mqtt_connect));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_RELAY_REFRESH_INTERVAL, &relay_refr_int));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_CHANNEL_COUNT, &relay_ch_count));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_CONTACT_SENSORS_COUNT, &relay_sn_count));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_OTA_UPDATE_URL, &ota_update_url));

    // Load MQTTS CA certificate
    char *ca_cert_mqtts = NULL;
    if (load_ca_certificate(&ca_cert_mqtts, CA_CERT_PATH_MQTTS) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load MQTTS CA certificate from %s", CA_CERT_PATH_MQTTS);
        if (html_template) free(html_template);
        if (html_output) free(html_output);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    } else {
        ESP_LOGD(TAG, "Loaded MQTTS CA certificate: %s", (ca_cert_mqtts == NULL) ? "NULL" : ca_cert_mqtts);
    }
    // Load HTTPS CA certificate
    char *ca_cert_https = NULL;
    if (load_ca_certificate(&ca_cert_https, CA_CERT_PATH_HTTPS) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load HTTPS CA certificate from %s", CA_CERT_PATH_HTTPS);
        if (html_template) free(html_template);
        if (html_output) free(html_output);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    } else {
        ESP_LOGD(TAG, "Loaded HTTPS CA certificate: %s", (ca_cert_https == NULL) ? "NULL" : ca_cert_https);
    }

    // Replace placeholders in the template with actual values
    snprintf(mqtt_port_str, sizeof(mqtt_port_str), "%u", mqtt_port);
    snprintf(ha_upd_intervl_str, sizeof(ha_upd_intervl_str), "%li", (uint32_t) ha_upd_intervl);
    snprintf(mqtt_connect_str, sizeof(mqtt_connect_str), "%i", (uint16_t) mqtt_connect);
    snprintf(relay_refr_int_str, sizeof(relay_refr_int_str), "%i", (uint16_t) relay_refr_int);
    snprintf(relay_ch_count_str, sizeof(relay_ch_count_str), "%i", (uint16_t) relay_ch_count);
    snprintf(relay_sn_count_str, sizeof(relay_sn_count_str), "%i", (uint16_t) relay_sn_count);


    // ESP_LOGI(TAG, "Current HTML output size: %i, MAX_TEMPLATE_SIZE: %i", sizeof(html_output), MAX_TEMPLATE_SIZE);

    replace_placeholder(html_output, "{VAL_DEVICE_ID}", device_id);
    replace_placeholder(html_output, "{VAL_DEVICE_SERIAL}", device_serial);
    replace_placeholder(html_output, "{VAL_MQTT_SERVER}", mqtt_server);
    replace_placeholder(html_output, "{VAL_MQTT_PORT}", mqtt_port_str);
    replace_placeholder(html_output, "{VAL_MQTT_PROTOCOL}", mqtt_protocol);
    replace_placeholder(html_output, "{VAL_MQTT_USER}", mqtt_user);
    replace_placeholder(html_output, "{VAL_MQTT_PASSWORD}", mqtt_password);
    replace_placeholder(html_output, "{VAL_MQTT_PREFIX}", mqtt_prefix);
    replace_placeholder(html_output, "{VAL_HA_PREFIX}", ha_prefix);
    replace_placeholder(html_output, "{VAL_MESSAGE}", success_message);
    replace_placeholder(html_output, "{VAL_HA_UPDATE_INTERVAL}", ha_upd_intervl_str);
    replace_placeholder(html_output, "{VAL_MQTT_CONNECT}", mqtt_connect_str);
    replace_placeholder(html_output, "{VAL_CA_CERT_MQTTS}", ca_cert_mqtts);
    replace_placeholder(html_output, "{VAL_CA_CERT_HTTPS}", ca_cert_https);
    replace_placeholder(html_output, "{VAL_RELAY_REFRESH_INTERVAL}", relay_refr_int_str);
    replace_placeholder(html_output, "{VAL_RELAY_CHANNEL_COUNT}", relay_ch_count_str);
    replace_placeholder(html_output, "{VAL_CONTACT_SENSORS_COUNT}", relay_sn_count_str);
    replace_placeholder(html_output, "{VAL_OTA_UPDATE_URL}", ota_update_url);

    // replace static fields
    assign_static_page_variables(html_output);

    // ESP_LOGI(TAG, "Final HTML output size: %i, MAX_TEMPLATE_SIZE: %i", sizeof(html_output), MAX_TEMPLATE_SIZE);

    // Send the final HTML response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_output, strlen(html_output));

    // Free dynamically allocated memory
    free(html_template);
    free(html_output);
    free(mqtt_server);
    free(mqtt_protocol);
    free(mqtt_user);
    free(mqtt_password);
    free(mqtt_prefix);
    free(ha_prefix);
    free(device_id);
    free(device_serial);
    free(ca_cert_mqtts);
    free(ca_cert_https);
    free(ota_update_url);

    return ESP_OK;
}

/**
 * @brief: Handler for the POST request to update the relay state
 * 
 * This function handles the POST request to update the relay state.
 * It extracts the relay ID and state from the request, updates the relay state, and sends a response back to the client.
 * 
 * @param[in] req: The HTTP request object
 * 
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t ca_cert_post_handler(httpd_req_t *req) {

    ESP_LOGI(TAG, "Processing certificate saving web request");

    // Buffer to hold the received certificate
    char buf[512];
    memset(buf, 0, sizeof(buf));  // Initialize the buffer with zeros to avoid any garbage
    int total_len = req->content_len;
    int received = 0;

    // Allocate memory dynamically for template and output
    char *html_template = (char *)malloc(MAX_TEMPLATE_SIZE);
    char *html_output = (char *)malloc(MAX_TEMPLATE_SIZE);

    if (html_template == NULL || html_output == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        if (html_template) free(html_template);
        if (html_output) free(html_output);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/ca-cert-saving.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        free(html_template);
        free(html_output);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Load the template into html_template
    size_t len = fread(html_template, 1, MAX_TEMPLATE_SIZE, f);
    fclose(f);
    html_template[len] = '\0';  // Null-terminate the string

    // Copy template into html_output for modification
    strcpy(html_output, html_template);


    // Allocate memory for the certificate outside the stack
    char *content = (char *)malloc(total_len + 1);
    if (content == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for CA certificate");
        free(html_template);
        free(html_output);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }

    // Read the certificate data from the request in chunks
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf, MIN(total_len - received, sizeof(buf)));
        if (ret <= 0) {
            ESP_LOGE(TAG, "Failed to receive POST data");
            free(content); // Free the allocated memory in case of failure
            free(html_template);
            free(html_output);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }
        memcpy(content + received, buf, ret);
        received += ret;
    }

    ESP_LOGI(TAG, "POST Content:\n%s", content);

    // Extract certificate type from the request CA_CERT_TYPE_LENGTH
    char ca_type[MAX_CA_CERT_SIZE];
    int ca_type_length = extract_param_value(content, "cert_type=", ca_type, MAX_CA_CERT_SIZE);
    if (ca_type_length <= 0) {
        ESP_LOGE(TAG, "Failed to extract CA certificate type from the received data");
        free(content);
        free(html_template);
        free(html_output);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to extract certificate type");
        return ESP_FAIL;
    }

    // determine certificate key based on the type
    const char *ca_key = (strcmp(ca_type, "mqtts") == 0) ? "ca_cert_mqtts=" : "ca_cert_https=";
    ESP_LOGI(TAG, "Will use key (%s) to extract the certificate according to its type (%s)", ca_key, ca_type);

    // extract ca_cert from the output
    char *ca_cert = (char *)malloc(MAX_CA_CERT_SIZE);
    // Extract the certificate
    int cert_length = extract_param_value(content, ca_key, ca_cert, MAX_CA_CERT_SIZE);
    if (cert_length <= 0) {
        ESP_LOGE(TAG, "Failed to extract CA certificate from the received data");
        free(content);
        free(ca_cert);
        free(html_template);
        free(html_output);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to extract certificate");
        return ESP_FAIL;
    }

    // Decode the URL-encoded certificate
    url_decode(ca_cert);

    // Null-terminate the decoded certificate
    str_trunc_after(ca_cert, "-----END CERTIFICATE-----");

    // Save the certificate
    const char *ca_path = (strcmp(ca_type, "mqtts") == 0) ? CA_CERT_PATH_MQTTS : CA_CERT_PATH_HTTPS;
    ESP_LOGI(TAG, "Saving certificate to %s", ca_path);
    replace_placeholder(html_output, "{VAL_CA_PATH}", ca_path);

    esp_err_t err = save_ca_certificate(ca_cert, ca_path, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save CA certificate");
        free(content);
        free(ca_cert);
        free(html_template);
        free(html_output);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save certificate");
        return ESP_FAIL;
    }

    free(content);
    free(ca_cert); // Free the allocated memory after saving

    // Send a response indicating success
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html_output);
    ESP_LOGI(TAG, "CA certificate saving request processed successfully");

    free(html_template);
    free(html_output);
    return ESP_OK;
}

/**
 * @brief: Handler for the POST request to update the relay state
 * 
 * This function handles the POST request to update the relay state.
 * It extracts the relay ID and state from the request, updates the relay state, and sends a response back to the client.
 * 
 * @param[in] req: The HTTP request object
 * 
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t reboot_handler(httpd_req_t *req) {
    ESP_LOGI("Reboot", "Rebooting the device...");

    // Send HTML response with a redirect after 30 seconds
    const char *reboot_html = "<html>"
                                "<head>"
                                    "<title>Rebooting...</title>"
                                    "<meta http-equiv=\"refresh\" content=\"30;url=/\" />"
                                    "<script>"
                                        "setTimeout(function() { window.location.href = '/'; }, 30000);"
                                    "</script>"
                                "</head>"
                                "<body>"
                                    "<h2>Device is rebooting...</h2>"
                                    "<p>Please wait, you will be redirected to the <a href=\"/\">home page</a> in 30 seconds.</p>"
                                "</body>"
                              "</html>";

    // Send the response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, reboot_html, HTTPD_RESP_USE_STRLEN);

    // Delay a bit to allow the response to be sent
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Reboot the device
    system_reboot();
    return ESP_OK;
}

/**
 * @brief: Handler for the POST request to update the relay state
 * 
 * This function handles the POST request to update the relay state.
 * It extracts the relay ID and state from the request, updates the relay state, and sends a response back to the client.
 * 
 * @param[in] req: The HTTP request object
 * 
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t relays_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Processing relays web request");

    // Allocate memory dynamically for template and output
    char *html_template = (char *)malloc(MAX_TEMPLATE_SIZE);
    char *html_output = (char *)malloc(MAX_TEMPLATE_SIZE);
    char *relays_list_html = (char *)malloc(MAX_TEMPLATE_SIZE); // to hold the relays list
    relays_list_html[0] = '\0';  // Initialize with empty string
    char safe_pins[128];       // Buffer to hold the safe GPIO pins list
    // Populate the safe GPIO pins as a comma-separated list
    populate_safe_gpio_pins(safe_pins, sizeof(safe_pins));

    char *relay_table_header = (char *)malloc(MAX_TBL_ENTRY_SIZE);
    char *relay_table_entry_tpl = (char *)malloc(MAX_TBL_ENTRY_SIZE);
    char *relay_table_entry = (char *)malloc(MAX_TBL_ENTRY_SIZE);

    if (html_template == NULL || html_output == NULL || relay_table_header == NULL || relay_table_entry_tpl == NULL || relay_table_entry == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        if (html_template) free(html_template);
        if (html_output) free(html_output);
        if (relays_list_html) free(relays_list_html);
        if (relay_table_header) free(relay_table_header);
        if (relay_table_entry_tpl) free(relay_table_entry_tpl);
        if (relay_table_entry) free(relay_table_entry);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/relays.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open relays template file for reading");
        free(html_template);
        free(html_output);
        free(relay_table_header);
        free(relay_table_entry_tpl);
        free(relay_table_entry);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    // Load the template into html_template
    size_t len = fread(html_template, 1, MAX_TEMPLATE_SIZE, f);
    fclose(f);
    html_template[len] = '\0';  // Null-terminate the string
    // Copy template into html_output for modification
    strcpy(html_output, html_template);

    // Read the table header template from SPIFFS
    f = fopen("/spiffs/relay_table_header.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open relays table header template file for reading");
        free(html_template);
        free(html_output);
        free(relay_table_header);
        free(relay_table_entry_tpl);
        free(relay_table_entry);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    // Load the template into html_template
    len = fread(relay_table_header, 1, MAX_TBL_ENTRY_SIZE, f);
    fclose(f);
    relay_table_header[len] = '\0';  // Null-terminate the string

    // Read the table header template from SPIFFS
    f = fopen("/spiffs/relay_table_entry.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open relays table entry template file for reading");
        free(html_template);
        free(html_output);
        free(relay_table_header);
        free(relay_table_entry_tpl);
        free(relay_table_entry);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    // Load the template into html_template
    len = fread(relay_table_entry_tpl, 1, MAX_TBL_ENTRY_SIZE, f);
    fclose(f);
    relay_table_entry_tpl[len] = '\0';  // Null-terminate the string


    // Allocate memory for the strings you will retrieve from NVS
    char *device_id = NULL;
    char *device_serial = NULL;
    uint16_t relay_ch_count;
    uint16_t relay_sn_count;

    // Load settings from NVS (use default values if not set)
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_CHANNEL_COUNT, &relay_ch_count));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_CONTACT_SENSORS_COUNT, &relay_sn_count));


    // now, let's iterate via all relays stored in the memory and try load/initiate them
    for (int i_channel = 0; i_channel < relay_ch_count; i_channel++) {
        relay_unit_t *relay = malloc(sizeof(relay_unit_t));
        if (relay == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for relay entry object");
            return ESP_FAIL; // Handle error accordingly
        }

        char *relay_nvs_key = get_relay_nvs_key(i_channel);
        if (relay_nvs_key == NULL) {
            ESP_LOGE(TAG, "Failed to get NVS key for channel %d", i_channel);
            free(relay);
            return ESP_FAIL; // Handle error accordingly
        }

        if (load_relay_actuator_from_nvs(relay_nvs_key, relay) == ESP_OK) {
            ESP_LOGI(TAG, "Found relay channel %i stored in NVS at %s. PIN %i", i_channel, relay_nvs_key, relay->gpio_pin);

            // Step 1: Copy string relay_table_entry_tpl to relay_table_entry
            strcpy(relay_table_entry, relay_table_entry_tpl);

            // Step 2: Substitute entries using replace_placeholder()
            char channel_str[5], gpio_pin_str[5];
            snprintf(channel_str, sizeof(channel_str), "%d", relay->channel);
            snprintf(gpio_pin_str, sizeof(gpio_pin_str), "%d", relay->gpio_pin);

            replace_placeholder(relay_table_entry, "{RELAY_KEY}", relay_nvs_key);
            replace_placeholder(relay_table_entry, "{RELAY_CHANNEL}", channel_str);
            replace_placeholder(relay_table_entry, "{RELAY_GPIO_PIN}", gpio_pin_str);

            // Handle RELAY_INVERTED: Checkbox checked if inverted is true
            if (relay->inverted) {
                replace_placeholder(relay_table_entry, "{RELAY_INVERTED}", "checked");
            } else {
                replace_placeholder(relay_table_entry, "{RELAY_INVERTED}", "");
            }

            // Handle RELAY_ENABLED: Checkbox checked if enabled is true
            if (relay->enabled) {
                replace_placeholder(relay_table_entry, "{RELAY_ENABLED}", "checked");
            } else {
                replace_placeholder(relay_table_entry, "{RELAY_ENABLED}", "");
            }

            // Step 3: Append relay_table_entry to relays_list_html
            strcat(relays_list_html, relay_table_entry);

            // Step 4: Clear relay_table_entry for next iteration
            relay_table_entry[0] = '\0';
        } else {
            ESP_LOGW(TAG, "Unable to find relay channel %i stored in NVS at %s. Entry skipped.", i_channel, relay_nvs_key);
        }

        free(relay_nvs_key);
        free(relay);
    }

    // Insert the relays list into the main HTML template
    replace_placeholder(html_output, "{RELAYS_TABLE_HEADER}", relay_table_header);
    replace_placeholder(html_output, "{RELAYS_TABLE_BODY}", relays_list_html);


    // Now, fill in the sensors
    relays_list_html[0] = '\0';  // Initialize with empty string

    // now, let's iterate via all relays stored in the memory and try load/initiate them
    for (int i_channel = 0; i_channel < relay_sn_count; i_channel++) {
        relay_unit_t *relay = malloc(sizeof(relay_unit_t));
        if (relay == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for relay entry object");
            return ESP_FAIL; // Handle error accordingly
        }

        char *relay_nvs_key = get_contact_sensor_nvs_key(i_channel);
        if (relay_nvs_key == NULL) {
            ESP_LOGE(TAG, "Failed to get NVS key for contact sensor at channel %d", i_channel);
            free(relay);
            return ESP_FAIL; // Handle error accordingly
        }

        if (load_relay_sensor_from_nvs(relay_nvs_key, relay) == ESP_OK) {
            ESP_LOGI(TAG, "Found sensor channel %i stored in NVS at %s. PIN %i", i_channel, relay_nvs_key, relay->gpio_pin);

            // Step 1: Copy string relay_table_entry_tpl to relay_table_entry
            strcpy(relay_table_entry, relay_table_entry_tpl);

            // Step 2: Substitute entries using replace_placeholder()
            char channel_str[5], gpio_pin_str[5];
            snprintf(channel_str, sizeof(channel_str), "%d", relay->channel);
            snprintf(gpio_pin_str, sizeof(gpio_pin_str), "%d", relay->gpio_pin);

            replace_placeholder(relay_table_entry, "{RELAY_KEY}", relay_nvs_key);
            replace_placeholder(relay_table_entry, "{RELAY_CHANNEL}", channel_str);
            replace_placeholder(relay_table_entry, "{RELAY_GPIO_PIN}", gpio_pin_str);

            // Handle RELAY_INVERTED: Checkbox checked if inverted is true
            if (relay->inverted) {
                replace_placeholder(relay_table_entry, "{RELAY_INVERTED}", "checked");
            } else {
                replace_placeholder(relay_table_entry, "{RELAY_INVERTED}", "");
            }

            // Handle RELAY_ENABLED: Checkbox checked if enabled is true
            if (relay->enabled) {
                replace_placeholder(relay_table_entry, "{RELAY_ENABLED}", "checked");
            } else {
                replace_placeholder(relay_table_entry, "{RELAY_ENABLED}", "");
            }

            // Step 3: Append relay_table_entry to relays_list_html
            strcat(relays_list_html, relay_table_entry);

            // Step 4: Clear relay_table_entry for next iteration
            relay_table_entry[0] = '\0';
        } else {
            ESP_LOGW(TAG, "Unable to find sensor contact channel %i stored in NVS at %s. Entry skipped.", i_channel, relay_nvs_key);
        }

        free(relay_nvs_key);
        free(relay);
    }  

    // Insert the relays list into the main HTML template
    replace_placeholder(html_output, "{CONTACT_SENSORS_TABLE_HEADER}", relay_table_header);
    replace_placeholder(html_output, "{CONTACT_SENSORS_TABLE_BODY}", relays_list_html); 

    // Insert other placeholders (device ID, serial, etc.)
    replace_placeholder(html_output, "{VAL_DEVICE_ID}", device_id);
    replace_placeholder(html_output, "{VAL_DEVICE_SERIAL}", device_serial);
    replace_placeholder(html_output, "{VAL_GPIO_SAFE_PINS}", safe_pins);

    // replace static fields
    assign_static_page_variables(html_output);

    // Send the final HTML response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_output, strlen(html_output));

    // Free dynamically allocated memory
    free(html_template);
    free(html_output);
    free(relays_list_html);
    free(device_id);
    free(device_serial);
    free(relay_table_header);
    free(relay_table_entry_tpl);
    free(relay_table_entry);

    return ESP_OK;
}

/**
 * @brief Handler for /api/relay/update endpoint
 *
 * @param req HTTP request
 * @return ESP_OK or ESP_FAIL
 */
static esp_err_t update_relay_post_handler(httpd_req_t *req) {
    char content[512];
    esp_err_t err;

    // Get the POST data
    int total_len = req->content_len;
    int received = 0;
    if (total_len >= sizeof(content)) {
        ESP_LOGE(TAG, "Content size overflowing the buffer!");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    while (received < total_len) {
        int ret = httpd_req_recv(req, content + received, total_len - received);
        if (ret <= 0) {
            ESP_LOGE(TAG, "Unexpected error while reading from request: %i", ret);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        received += ret;
    }
    content[received] = '\0';

    // Parse the incoming JSON data
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Get the 'data' object from JSON
    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (data == NULL) {
        ESP_LOGE(TAG, "No 'data' object in JSON");
        cJSON_Delete(json);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Extract and validate the mandatory fields: device_serial and relay_key
    // Safely retrieve 'device_serial' from JSON
    cJSON *device_serial_item = cJSON_GetObjectItem(data, "device_serial");
    if (device_serial_item == NULL || !cJSON_IsString(device_serial_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'device_serial' in JSON data");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid 'device_serial'");
        cJSON_Delete(json);  // Don't forget to free the cJSON object
        return ESP_FAIL;
    }
    const char *device_serial = device_serial_item->valuestring;

    // Safely retrieve 'relay_key' from JSON
    cJSON *relay_key_item = cJSON_GetObjectItem(data, "relay_key");
    if (relay_key_item == NULL || !cJSON_IsString(relay_key_item)) {
        ESP_LOGE(TAG, "Invalid or missing 'relay_key' in the JSON data");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid 'relay_key'");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    const char *relay_key = relay_key_item->valuestring;

    if (device_serial == NULL || relay_key == NULL) {
        ESP_LOGE(TAG, "Missing mandatory fields: device_serial or relay_key");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing mandatory fields");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // Validate device serial from NVS
    char *device_serial_nvs = NULL;
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial_nvs));
    if (strcmp(device_serial, device_serial_nvs) != 0) {
        ESP_LOGE(TAG, "Device serial mismatch: provided: %s, actual: %s", device_serial, device_serial_nvs);
        free(device_serial_nvs);
        cJSON_Delete(json);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    free(device_serial_nvs);

    // Extract relay type or assume RELAY_TYPE_ACTUATOR if not provided
    cJSON *relay_type_item = cJSON_GetObjectItem(data, "relay_type");
    relay_type_t relay_type = RELAY_TYPE_ACTUATOR;  // Default type
    if (relay_type_item != NULL && cJSON_IsNumber(relay_type_item)) {
        relay_type = (relay_type_t)relay_type_item->valueint;
    }

    // Load relay from NVS based on the relay_key
    relay_unit_t relay;
    if (relay_type == RELAY_TYPE_SENSOR) {
        err = load_relay_sensor_from_nvs(relay_key, &relay);
    } else {
        err = load_relay_actuator_from_nvs(relay_key, &relay);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load relay from NVS");
        cJSON_Delete(json);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // capture old gpio_pin
    int gpio_pin_old = relay.gpio_pin;

    // Validate GPIO pin if provided in the JSON
    cJSON *relay_gpio_pin_item = cJSON_GetObjectItem(data, "relay_gpio_pin");
    if (relay_gpio_pin_item != NULL && cJSON_IsNumber(relay_gpio_pin_item)) {
        int gpio_pin = relay_gpio_pin_item->valueint;

        // Validate the GPIO pin against the safe list
        if (!is_gpio_safe(gpio_pin)) {
            ESP_LOGE(TAG, "Invalid GPIO pin: %d", gpio_pin);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid GPIO pin");
            cJSON_Delete(json);
            return ESP_FAIL;
        }

        // if we provide a new GPIO pin -- make sure it is not use
        if (gpio_pin_old != relay.gpio_pin && is_gpio_pin_in_use(gpio_pin)) {
            ESP_LOGE(TAG, "GPIO pin %d is in use", gpio_pin);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "GPIO pin is in use");
            cJSON_Delete(json);
            return ESP_FAIL;
        }
        relay.gpio_pin = gpio_pin;
    }

    // Update relay properties based on the JSON data (if provided)
    cJSON *relay_state_item = cJSON_GetObjectItem(data, "relay_state");
    if (relay_state_item != NULL && cJSON_IsBool(relay_state_item)) {
        relay.state = relay_state_item->valueint ? RELAY_STATE_ON : RELAY_STATE_OFF;
    }

    cJSON *relay_enabled_item = cJSON_GetObjectItem(data, "relay_enabled");
    if (relay_enabled_item != NULL && cJSON_IsBool(relay_enabled_item)) {
        relay.enabled = relay_enabled_item->valueint;
    }

    cJSON *relay_inverted_item = cJSON_GetObjectItem(data, "relay_inverted");
    if (relay_inverted_item != NULL && cJSON_IsBool(relay_inverted_item)) {
        relay.inverted = relay_inverted_item->valueint;
    }

    // save to NVS: actuators -- via setting the state, sensors -- just saving
    if (relay.type == RELAY_TYPE_ACTUATOR) {
        err = relay_set_state(&relay, relay.state, true);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set relay state and save it to NVS");
            cJSON_Delete(json);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        } 
    } else {
        // Save the updated sensor to NVS using relay_key
        err = save_relay_to_nvs(relay_key, &relay);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save relay to NVS");
            cJSON_Delete(json);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        // if pin changed -- re-assign the ISR
        if (gpio_pin_old != relay.gpio_pin) {
            err = relay_gpio_init(&relay);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to init new pin number %d when updating the sensor unit", relay.gpio_pin);
                cJSON_Delete(json);
                httpd_resp_send_500(req);
                return ESP_FAIL;
            } 
            err = relay_sensor_register_isr(&relay);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to register ISR for new pin number %d when updating the sensor unit", relay.gpio_pin);;
                cJSON_Delete(json);
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }           
        }
    }

    // Serialize updated relay data for the response
    char *relay_json_str = serialize_relay_unit(&relay);
    if (relay_json_str == NULL) {
        ESP_LOGE(TAG, "Failed to serialize updated relay");
        cJSON_Delete(json);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Create response JSON
    cJSON *response = cJSON_CreateObject();
    cJSON *status = cJSON_CreateObject();

    cJSON_AddStringToObject(status, "error", "OK");
    cJSON_AddNumberToObject(status, "code", 0);

    // Add relay data and status to the response
    cJSON_AddRawToObject(response, "data", relay_json_str);
    cJSON_AddItemToObject(response, "status", status);

    // Send the response
    const char *response_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, strlen(response_str));

    // Clean up
    cJSON_Delete(json);
    cJSON_Delete(response);
    free(relay_json_str);
    free((void *)response_str);
    return ESP_OK;
}

/**
 * @brief Handler for /api/relay/update endpoint
 *
 * @param req HTTP request
 * @return ESP_OK or ESP_FAIL
 */
static esp_err_t status_data_handler(httpd_req_t *req) {
    
    device_status_t device_status;
    ESP_ERROR_CHECK(device_status_init(&device_status));
    
    // Convert cJSON object to a string
    const char *json_response = serialize_all_device_data(&device_status);

    
    // Set the content type to JSON and send the response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));

    // Free allocated memory
    free((void *)json_response);

    return ESP_OK;
}

/**
 * @brief Handler for /api/relay/update endpoint
 *
 * @param req HTTP request
 * @return ESP_OK or ESP_FAIL
 */
static esp_err_t status_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Processing status web request");

    // Allocate memory dynamically for template and output
    char *html_template = (char *)malloc(MAX_TEMPLATE_SIZE);
    char *html_output = (char *)malloc(MAX_TEMPLATE_SIZE);

    if (html_template == NULL || html_output == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        if (html_template) free(html_template);
        if (html_output) free(html_output);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/status.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        free(html_template);
        free(html_output);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Load the template into html_template
    size_t len = fread(html_template, 1, MAX_TEMPLATE_SIZE, f);
    fclose(f);
    html_template[len] = '\0';  // Null-terminate the string

    // Copy template into html_output for modification
    strcpy(html_output, html_template);

    // Allocate memory for the strings you will retrieve from NVS
    char *device_id = NULL;
    char *device_serial = NULL;
    uint16_t relay_refr_int;

    // Load settings from NVS (use default values if not set)
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    ESP_ERROR_CHECK(nvs_read_uint16(S_NAMESPACE, S_KEY_RELAY_REFRESH_INTERVAL, &relay_refr_int));

    char sensor_intervl_str[10];
    snprintf(sensor_intervl_str, sizeof(sensor_intervl_str), "%i", (uint16_t) relay_refr_int);

    replace_placeholder(html_output, "{VAL_DEVICE_ID}", device_id);
    replace_placeholder(html_output, "{VAL_DEVICE_SERIAL}", device_serial);
    replace_placeholder(html_output, "{VAL_STATUS_READ_INTERVAL}", sensor_intervl_str);

    // replace static fields
    assign_static_page_variables(html_output);


    // Send the final HTML response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_output, strlen(html_output));

    // Free dynamically allocated memory
    free(html_template);
    free(html_output);
    free(device_id);
    free(device_serial);

    return ESP_OK;
}

/**
 * @brief Handler for /api/relay/update endpoint
 *
 * @param req HTTP request
 * @return ESP_OK or ESP_FAIL
 */
static esp_err_t relays_data_get_handler(httpd_req_t *req) {
    // Create a JSON object for the response
    cJSON *response = cJSON_CreateObject();
    if (response == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON response");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Create a JSON array to hold the list of relays and sensors
    cJSON *relay_array = cJSON_CreateArray();
    if (relay_array == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON relay array");
        cJSON_Delete(response);  // Free the response on error
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Add the relay array to the response as "data"
    cJSON_AddItemToObject(response, "data", relay_array);

    // Get all relays and sensors
    relay_unit_t *relay_list = NULL;
    uint16_t total_count = 0;
    esp_err_t err = get_all_relay_units(&relay_list, &total_count);
    if (err != ESP_OK) {
        cJSON_Delete(response);  // Free the response on error
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Serialize each relay_unit_t and add it to the JSON array
    for (int i = 0; i < total_count; i++) {
        char *serialized_relay = serialize_relay_unit(&relay_list[i]);
        if (serialized_relay != NULL) {
            cJSON *relay_json = cJSON_Parse(serialized_relay);
            if (relay_json != NULL) {
                cJSON_AddItemToArray(relay_array, relay_json);  // Add the parsed relay to the array
            } else {
                ESP_LOGE(TAG, "Failed to parse serialized relay");
            }
            free(serialized_relay);  // Free serialized relay string after usage
        } else {
            ESP_LOGE(TAG, "Failed to serialize relay unit");
        }
    }

    // Free the relay list memory
    // ESP_ERROR_CHECK(free_relays_array(relay_list, total_count));
    free(relay_list);

    // Create and add a status object to the response
    cJSON *status = cJSON_CreateObject();
    if (status == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON status object");
        cJSON_Delete(response);  // Free the response on error
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Add the status block to the response
    cJSON_AddItemToObject(response, "status", status);
    cJSON_AddNumberToObject(status, "count", total_count);
    cJSON_AddNumberToObject(status, "code", 0);  // 0 means success
    cJSON_AddStringToObject(status, "text", "ok");

    // Convert the response to a JSON string
    char *json_response = cJSON_Print(response);
    if (json_response == NULL) {
        ESP_LOGE(TAG, "Failed to convert JSON response to string");
        cJSON_Delete(response);  // Free the response on error
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Send the JSON response
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_response, strlen(json_response));

    // Clean up
    free(json_response);  // Free the generated JSON string
    cJSON_Delete(response);  // Free the root JSON object

    return ret;
}

/**
 * @brief HTTP GET handler to trigger OTA update via web interface.
 *
 * This function is registered as an HTTP GET handler that triggers an OTA update
 * using the URL stored in NVS. The OTA update process is started, and if successful,
 * the device will send a response indicating the update has started and reboot afterward.
 *
 * @param req The HTTP request object.
 * @return ESP_OK on successful request handling, or an error code otherwise.
 */
esp_err_t ota_post_handler(httpd_req_t *req) {
    char *ota_url = NULL;

    // Allocate memory dynamically for template and output
    char *html_template = (char *)malloc(MAX_TEMPLATE_SIZE);
    char *html_output = (char *)malloc(MAX_TEMPLATE_SIZE);

    if (html_template == NULL || html_output == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        if (html_template) free(html_template);
        if (html_output) free(html_output);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read the template from SPIFFS (assuming you're loading it from SPIFFS)
    FILE *f = fopen("/spiffs/firmware-updating.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        free(html_template);
        free(html_output);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Load the template into html_template
    size_t len = fread(html_template, 1, MAX_TEMPLATE_SIZE, f);
    fclose(f);
    html_template[len] = '\0';  // Null-terminate the string

    // Copy template into html_output for modification
    strcpy(html_output, html_template);
    
    // Get the OTA URL from NVS
    if (nvs_read_string(S_NAMESPACE, S_KEY_OTA_UPDATE_URL, &ota_url) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read OTA URL from NVS");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Starting OTA via Web with URL: %s", ota_url);

    // replace values
    replace_placeholder(html_output, "{VAL_SW_FIRMWARE_URL}", ota_url);
        
    // replace static fields
    assign_static_page_variables(html_output);


    // Allocate memory for the task parameter
    ota_update_param_t *update_param = malloc(sizeof(ota_update_param_t));
    if (update_param == NULL) {
        httpd_resp_send_500(req);  // Send error response in case of memory allocation failure
        return ESP_FAIL;
    }

    // Copy the OTA URL into the task parameter
    strcpy(update_param->ota_url, ota_url);

    // Create the OTA update task, passing the OTA URL as the task parameter
    if (xTaskCreate(ota_update_task, "ota_update_task", 8192, update_param, 5, NULL) != pdPASS) {
        free(update_param);  // Free memory if task creation failed
        free(ota_url);
        free(html_template);
        free(html_output);
        httpd_resp_send_500(req);  // Send error response if task creation fails
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_output, strlen(html_output));


    // Free dynamically allocated memory if needed
    free(ota_url);
    free(html_template);
    free(html_output);


    return ESP_OK;
}

/**
 * @brief HTTP POST handler to reset the device to factory settings.
 *
 * This function is registered as an HTTP POST handler that resets the device to factory settings.
 * It reads the device ID and serial from NVS, validates them against the request, and performs the reset action.
 * The device will reboot after the reset action is completed.
 *
 * @param req The HTTP request object.
 * @return ESP_OK on successful request handling, or an error code otherwise.
 */
esp_err_t reset_post_handler(httpd_req_t *req) {

    // Extract form data
    char buf[1024];
    memset(buf, 0, sizeof(buf));  // Initialize the buffer with zeros to avoid any garbage
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }

    char *device_id = NULL;
    char *device_serial = NULL;
    
    // Read stored device_id and device_serial
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id));
    ESP_ERROR_CHECK(nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial));
    
    // Extract parameters from request
    char received_device_id[DEVICE_ID_LENGTH+1];
    char received_device_serial[DEVICE_SERIAL_LENGTH+1];
    char action_str[8];
    int action;

    // Get device_id and device_serial from request
    extract_param_value(buf, "device_id=", received_device_id, sizeof(received_device_id));
    extract_param_value(buf, "device_serial=", received_device_serial, sizeof(received_device_serial));
    
    // Retrieve action and convert to integer
    extract_param_value(buf, "action=", action_str, sizeof(action_str));
    action = (int)atoi(action_str);

    // Validate device_serial and device_id
    if (strcmp(device_id, received_device_id) != 0 || strcmp(device_serial, received_device_serial) != 0) {
        ESP_LOGE(TAG, "Device validation failed: mismatched device_id or device_serial.");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ret = ESP_FAIL;

    // Perform the appropriate reset action
    switch (action) {
        case 0:
            ret = reset_factory_settings();
            break;
        case 1:
            ret = reset_device_settings();
            break;
        case 2:
            ret = reset_wifi_settings();
            break;
        default:
            ESP_LOGE(TAG, "Unknown action requested: %d", action);
            httpd_resp_send_500(req);
            return ESP_FAIL;
    }

    // Reboot the device if reset action was successful
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Reset action %d completed successfully. Rebooting...", action);
        return reboot_handler(req);
    } else {
        ESP_LOGE(TAG, "Reset action %d failed: %s", action, esp_err_to_name(ret));
        httpd_resp_send_500(req);
    }

    return ret;
}

/**
 * @brief Stop the HTTP server
 * 
 * This function stops the HTTP server.
 * 
 * @param[in] server: The HTTP server handle
 * 
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t stop_http_server(httpd_handle_t server) {
    // Stop the HTTP server
    if (server != NULL) {
        ESP_ERROR_CHECK(httpd_stop(server));
        server = NULL;
    } else {
        ESP_LOGW(TAG, "NULL HTTP server handle");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

/**
 * @brief Stop the HTTP server started in the "run_http_server" task
 * 
 * This function stops the HTTP server started in the "run_http_server" task.
 * 
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t http_stop(void) {
    return stop_http_server(server);
}