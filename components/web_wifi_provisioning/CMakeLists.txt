idf_component_register(
    SRCS "src/web_wifi_provisioning.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esp_wifi ESP32_NVS p_utils p_web_utils web_static_files web_device_authorization
    )

# Copy the HTML template to the project's SPIFFS directory
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/html/wifi.html"
    "${PROJECT_DIR}/spiffs/wifi.html"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/html/static-wifi.js"
    "${PROJECT_DIR}/spiffs/static-wifi.js"
    COPYONLY
)
