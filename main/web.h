#ifndef _WEB_H
#define _WEB_H

#include "esp_http_server.h"

#define MAX_TEMPLATE_SIZE       24576
#define MAX_TBL_ENTRY_SIZE      2048
#define MAX_CA_CERT_SIZE        8192


void run_http_server(void *param);

static esp_err_t config_get_handler(httpd_req_t *req);
static esp_err_t submit_post_handler(httpd_req_t *req);
static esp_err_t reboot_handler(httpd_req_t *req);
static esp_err_t status_data_handler(httpd_req_t *req);
static esp_err_t status_get_handler(httpd_req_t *req);
static esp_err_t ca_cert_post_handler(httpd_req_t *req);
static esp_err_t relays_get_handler(httpd_req_t *req);
static esp_err_t update_relay_post_handler(httpd_req_t *req);
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


void assign_static_page_variables(char *html_output);
void replace_placeholder(char *html_output, const char *placeholder, const char *value);
int extract_param_value(const char *buf, const char *param_name, char *output, size_t output_size);

int hex2dec(char c);
void url_decode(char *str);
void str_trunc_after(char *str, const char *lookup);



#endif