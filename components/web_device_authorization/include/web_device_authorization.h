#ifndef WEB_DEVICE_AUTHORIZATION_H
#define WEB_DEVICE_AUTHORIZATION_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "cJSON.h"

#ifndef S_NAMESPACE
#define S_NAMESPACE         "settings"
#endif

#ifndef S_KEY_DEVICE_ID
#define S_KEY_DEVICE_ID         "device_id"
#endif

#ifndef S_KEY_DEVICE_SERIAL
#define S_KEY_DEVICE_SERIAL     "device_serial"
#endif

#ifndef DEVICE_SERIAL_LENGTH
#define DEVICE_SERIAL_LENGTH    32
#endif

#ifndef DEVICE_ID_LENGTH
#define DEVICE_ID_LENGTH        12
#endif

esp_err_t validate_device_identity_from_get_query(httpd_req_t *req);
esp_err_t validate_device_identity_from_json(const cJSON *json);

#endif // WEB_DEVICE_AUTHORIZATION_H