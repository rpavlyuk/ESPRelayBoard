idf_component_register(
    SRCS "src/web_device_authorization.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server ESP32_NVS p_utils p_web_utils
    )

