#ifndef WIFI_WEB_PROVISIONING_H
#define WIFI_WEB_PROVISIONING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_http_server.h"


/**
 * Default values for the global constants defined in settings.h
 */
#if !defined(MAX_LARGE_TEMPLATE_SIZE)
#define MAX_LARGE_TEMPLATE_SIZE 24576
#endif

#if !defined(MAX_TEMPLATE_SIZE)
#define MAX_TEMPLATE_SIZE 17408
#endif

#if !defined(MAX_SMALL_TEMPLATE_SIZE)
#define MAX_SMALL_TEMPLATE_SIZE 8192
#endif

#if !defined(S_NAMESPACE)
#define S_NAMESPACE         "settings"
#endif

#if !defined(WIFI_NAMESPACE)
#define WIFI_NAMESPACE     "nvs.net80211"
#endif

#if !defined(S_KEY_DEVICE_ID)
#define S_KEY_DEVICE_ID         "device_id"
#endif

#if !defined(S_KEY_DEVICE_SERIAL)
#define S_KEY_DEVICE_SERIAL     "device_serial"
#endif

#ifndef MAX_JSON_BUFFER_SIZE
#define MAX_JSON_BUFFER_SIZE    2048
#endif

esp_err_t wifi_provision_get_handler(httpd_req_t *req);
esp_err_t wifi_provision_post_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

#endif // WIFI_WEB_PROVISIONING_H