#ifndef WEB_SERVE_STATIC_FILES_H
#define WEB_SERVE_STATIC_FILES_H
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_http_server.h"

#if !defined(STREAM_LINE_BUF_SZ)
#define STREAM_LINE_BUF_SZ      4096   // max line length we accept (incl. expansions)
#endif
#if !defined(STREAM_READ_LINE_SZ)
#define STREAM_READ_LINE_SZ     2048   // fgets read size; must be <= STREAM_LINE_BUF_SZ
#endif
#if !defined(STATIC_PATH_PREFIX)
#define STATIC_PATH_PREFIX      "/spiffs/static-"  // /static/x.js -> /spiffs/static-x.js
#endif
#if !defined(ENABLE_PLACEHOLDER_REPLACEMENT)
#define ENABLE_PLACEHOLDER_REPLACEMENT false  // set to true to enable placeholder replacement in static files
#endif
#if !defined(SCRATCH_BUFSIZE)
#define SCRATCH_BUFSIZE  8192
#endif
#if !defined(ENABLE_STATIC_NOCACHE_HEADER)
#define ENABLE_STATIC_NOCACHE_HEADER  false
#endif

struct http_file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

esp_err_t http_static_stream_handler(httpd_req_t *req);

#endif // WEB_SERVE_STATIC_FILES_H