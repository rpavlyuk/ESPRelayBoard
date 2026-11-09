idf_component_register(
                    SRCS "src/system_ota_update.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_client esp_https_ota spiffs app_update ca_cert_manager
                    )