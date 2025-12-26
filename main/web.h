#ifndef _WEB_H
#define _WEB_H

#include "esp_http_server.h"

#define MAX_TEMPLATE_SIZE       17408
#define MAX_LARGE_TEMPLATE_SIZE       24576
#define MAX_SMALL_TEMPLATE_SIZE       8192
#define MAX_TBL_ENTRY_SIZE      1024
#define MAX_CA_CERT_SIZE        8192
#define MAX_JSON_BUFFER_SIZE    2048

#define STREAM_LINE_BUF_SZ      4096   // max line length we accept (incl. expansions)
#define STREAM_READ_LINE_SZ     2048   // fgets read size; must be <= STREAM_LINE_BUF_SZ
#define STATIC_PATH_PREFIX      "/spiffs/static-"  // /static/x.js -> /spiffs/static-x.js
#define ENABLE_PLACEHOLDER_REPLACEMENT false  // set to true to enable placeholder replacement in static files



void run_http_server(void *param);
esp_err_t stop_http_server(httpd_handle_t server);
esp_err_t http_stop(void);

static esp_err_t config_get_handler(httpd_req_t *req);
static esp_err_t submit_config_handler(httpd_req_t *req);
static esp_err_t reboot_handler(httpd_req_t *req);
static esp_err_t status_data_handler(httpd_req_t *req);
static esp_err_t status_get_handler(httpd_req_t *req);
static esp_err_t ca_cert_post_handler(httpd_req_t *req);
static esp_err_t relays_get_handler(httpd_req_t *req);
static esp_err_t update_relay_post_handler(httpd_req_t *req);
static esp_err_t set_setting_value_post_handler(httpd_req_t *req);
static esp_err_t get_settings_all_handler(httpd_req_t *req);
static esp_err_t get_setting_one_handler(httpd_req_t *req);
static esp_err_t get_ca_certificate_handler(httpd_req_t *req);
/**
 * @brief HTTP GET handler to return a JSON list of all relays and contact sensors.
 * 
 * This function handles an HTTP GET request by retrieving the list of all relay actuators 
 * and contact sensors, serializing them into JSON format, and sending the response back 
 * to the client. It also includes status information such as the total count of relays 
 * and sensors, and a success or error code.
 * 
 * @param[in] req HTTP request pointer.
 * 
 * @return ESP_OK on success, or an error code on failure.
 */
static esp_err_t relays_data_get_handler(httpd_req_t *req);
esp_err_t ota_post_handler(httpd_req_t *req);
esp_err_t reset_post_handler(httpd_req_t *req);
static esp_err_t static_stream_handler(httpd_req_t *req);


void assign_static_page_variables(char *html_output);
void replace_placeholder(char *html_output, const char *placeholder, const char *value);
esp_err_t replace_placeholder_sized(char *buf, size_t cap,
                                   const char *placeholder,
                                   const char *value);
int extract_param_value(const char *buf, const char *param_name, char *output, size_t output_size);
static esp_err_t extract_param_value_from_get_query(httpd_req_t *req, const char *param_name, char *output, size_t output_size);
static esp_err_t validate_device_identity_from_get_query(httpd_req_t *req);

static void json_value_to_string(const cJSON *v, char *out, size_t out_sz);

int hex_to_dec(char c);
void url_decode(char *str);
void str_trunc_after(char *str, const char *lookup);
static const char *content_type_from_ext(const char *path);



#endif