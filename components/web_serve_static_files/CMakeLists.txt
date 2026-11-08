idf_component_register(
    SRCS "src/web_serve_static_files.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server vfs web_static_files p_web_utils 
)