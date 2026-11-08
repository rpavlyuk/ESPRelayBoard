#ifndef _WEB_H
#define _WEB_H

#include "esp_http_server.h"

#define MAX_TEMPLATE_SIZE       17408
#define MAX_LARGE_TEMPLATE_SIZE       24576
#define MAX_SMALL_TEMPLATE_SIZE       8192
#define MAX_TBL_ENTRY_SIZE      1536
#define MAX_CA_CERT_SIZE        8192
#define MAX_JSON_BUFFER_SIZE    2048

#define STREAM_LINE_BUF_SZ      4096   // max line length we accept (incl. expansions)
#define STREAM_READ_LINE_SZ     2048   // fgets read size; must be <= STREAM_LINE_BUF_SZ
#define STATIC_PATH_PREFIX      "/spiffs/static-"  // /static/x.js -> /spiffs/static-x.js
#define ENABLE_PLACEHOLDER_REPLACEMENT false  // set to true to enable placeholder replacement in static files
#define ENABLE_STATIC_NOCACHE_HEADER   true  // set to true to add no-cache headers to static file responses
#define ENABLE_CONFIG_PLACEHOLDER_REPLACEMENT false  // set to true to enable placeholder replacement in config.html

#define STATIC_BASE_PATH        "/spiffs"  // Base path for SPIFFS file system
#define SCRATCH_BUFSIZE  8192

#define WEB_ENABLE_REBOOT_ON_OUT_OF_MEMORY  true  // set to true to enable automatic reboot on out-of-memory situations
#define WEB_ON_OUT_OF_MEMORY_FUNCTION       abort  // Function to call when out of memory. Options: esp_restart, abort, or custom function.      


void run_http_server(void *param);
esp_err_t stop_http_server(httpd_handle_t server);
esp_err_t http_stop(void);

// WEB Handlers
static esp_err_t config_get_handler(httpd_req_t *req);
static esp_err_t submit_config_handler(httpd_req_t *req);
static esp_err_t reboot_handler(httpd_req_t *req);
static esp_err_t status_get_handler(httpd_req_t *req);
static esp_err_t relays_get_handler(httpd_req_t *req);
static esp_err_t ca_cert_post_handler(httpd_req_t *req);
static esp_err_t ota_post_handler(httpd_req_t *req);
static esp_err_t reset_post_handler(httpd_req_t *req);

// API Handlers
static esp_err_t status_data_handler(httpd_req_t *req);
static esp_err_t update_relay_post_handler(httpd_req_t *req);
static esp_err_t set_setting_value_post_handler(httpd_req_t *req);
static esp_err_t get_settings_all_handler(httpd_req_t *req);
static esp_err_t get_setting_one_handler(httpd_req_t *req);
static esp_err_t get_ca_certificate_handler(httpd_req_t *req);
static esp_err_t relays_data_get_handler(httpd_req_t *req);
static esp_err_t api_control_handler(httpd_req_t *req);


void assign_static_page_variables(char *html_output);

#endif