idf_component_register()

# Copy static files to the project's SPIFFS directory
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/spiffs/static-bootstrap.bundle.min.js"
    "${PROJECT_DIR}/spiffs/static-bootstrap.bundle.min.js"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/spiffs/static-bootstrap.min.css"
    "${PROJECT_DIR}/spiffs/static-bootstrap.min.css"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/spiffs/static-docsearch.min.css"
    "${PROJECT_DIR}/spiffs/static-docsearch.min.css"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/spiffs/static-jquery.min.js"
    "${PROJECT_DIR}/spiffs/static-jquery.min.js"
    COPYONLY
)