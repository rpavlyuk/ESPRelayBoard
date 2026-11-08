#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "string.h"
#include "ctype.h"
#include "stdio.h"
#include "stdlib.h"
#include "web_serve_static_files.h"
#include "p_web_utils.h"

static const char *TAG = "WEB_STATIC_STREAM";

/**
 * @brief HTTP handler to stream /static/<name> as /spiffs/static-<name> (SPIFFS has no dirs)
 * This handler streams static files from SPIFFS with on-the-fly placeholder replacement.
 *
 * Example:
 *   GET /static/script.js  ->  /spiffs/static-script.js
 *
 * Notes:
 * - Uses line-by-line streaming with placeholder replacement.
 * - For placeholders that might span lines, you'll need a real streaming placeholder engine.
 * - Adjust STREAM_READ_LINE_SZ and STREAM_LINE_BUF_SZ as needed.
 * 
 * @param req HTTP request
 * @return ESP_OK or ESP_FAIL
 */
esp_err_t http_static_stream_handler(httpd_req_t *req) {
    // Basic URI validation
    const char *uri = req->uri;                 // e.g. "/static/script.js"
    const char *prefix = "/static/";
    const size_t prefix_len = strlen(prefix);

    ESP_LOGI(TAG, "Static file request: %s", uri);

    if (strncmp(uri, prefix, prefix_len) != 0 || uri[prefix_len] == '\0') {
        ESP_LOGW(TAG, "Invalid static file request: %s", uri);
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    // Disallow parent traversal and weird paths
    // (we don't support subdirs anyway; map "a/b.js" -> "a-b.js")
    char mapped_name[128] = {0};
    {
        const char *name = uri + prefix_len;    // "script.js" or "dir/file.js"
        size_t j = 0;

        for (size_t i = 0; name[i] != '\0' && j < sizeof(mapped_name) - 1; i++) {
            char c = name[i];

            // Reject obvious traversal
            if (c == '.' && name[i + 1] == '.') {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid path");
                return ESP_OK;
            }

            // SPIFFS mapping: replace '/' with '-' (SPIFFS has no dirs)
            if (c == '/') c = '-';

            // Keep it conservative: allow alnum and a few safe symbols
            if (!(isalnum((unsigned char)c) || c=='-' || c=='_' || c=='.')) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid characters in path");
                return ESP_OK;
            }

            mapped_name[j++] = c;
        }

        mapped_name[j] = '\0';

        if (mapped_name[0] == '\0') {
            httpd_resp_send_404(req);
            return ESP_OK;
        }
    }

    // Build actual SPIFFS path: /spiffs/static-<mapped_name>
    char filepath[192];
    snprintf(filepath, sizeof(filepath), "%s%s", STATIC_PATH_PREFIX, mapped_name);

    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGW(TAG, "File not found: %s (uri=%s)", filepath, uri);
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    // Set content type based on requested URI extension (not SPIFFS name)
    httpd_resp_set_type(req, content_type_from_ext(uri));
#if ENABLE_STATIC_NOCACHE_HEADER
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
#else
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600"); // optional
#endif

    // Now we have two ways to stream: line-by-line (with placeholder replacement) or chunked (raw). 
    //We'll do line-by-line if:
    // 1. Placeholder replacement is enabled
    // 2. OR file is not minified (function bool is_minified_file(const char *file_name) returns false)
    // 2. OR mimetype is text-based (text/html, text/css, application/javascript, etc.), but not minified (function bool is_text_based_mimetype(const char *file_name) returns true)
    bool do_line_by_line = ENABLE_PLACEHOLDER_REPLACEMENT || !is_minified_file(filepath) || (is_text_based_mimetype(filepath) && !is_minified_file(filepath));

    if (do_line_by_line) {
        ESP_LOGI(TAG, "Streaming file line-by-line: %s", filepath);
        // Buffers
        char *read_line = (char *)malloc(STREAM_READ_LINE_SZ);
        char *work_line = (char *)malloc(STREAM_LINE_BUF_SZ);
        if (!read_line || !work_line) {
            fclose(f);
            free(read_line);
            free(work_line);
            httpd_resp_send_500(req);
            return ESP_OK;
        }

#if ENABLE_PLACEHOLDER_REPLACEMENT
        // NOTE: If you want these per-request, extract them from query params.
        // For now, keep placeholders consistent with the rest of your templating model.
        char *device_id = NULL;
        char *device_serial = NULL;

        esp_err_t err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_ID, &device_id);
        if (err != ESP_OK || !device_id) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read device_id from NVS");
            return ESP_FAIL;
        }

        err = nvs_read_string(S_NAMESPACE, S_KEY_DEVICE_SERIAL, &device_serial);
        if (err != ESP_OK || !device_serial) {
            free(device_id);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read device_serial from NVS");
            return ESP_FAIL;
        }
 #endif

        while (fgets(read_line, STREAM_READ_LINE_SZ, f) != NULL) {
            // If the line is longer than STREAM_READ_LINE_SZ-1, fgets returns a partial line
            // (no '\n' and not EOF). This approach cannot safely template such lines.
            size_t rl = strlen(read_line);
            if (rl == STREAM_READ_LINE_SZ - 1 && read_line[rl - 1] != '\n' && !feof(f)) {
                ESP_LOGE(TAG,
                        "Line too long in %s; increase STREAM_READ_LINE_SZ/STREAM_LINE_BUF_SZ "
                        "or avoid templating large/minified assets",
                        filepath);
                fclose(f);
                free(read_line);
                free(work_line);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "template line too long");
                return ESP_OK;
            }

            // Copy into a larger working buffer to give replacements room to expand
            strlcpy(work_line, read_line, STREAM_LINE_BUF_SZ);

#if ENABLE_PLACEHOLDER_REPLACEMENT
            // Replace placeholders safely (bounded)
            esp_err_t r;

            r = replace_placeholder_sized(work_line, STREAM_LINE_BUF_SZ,
                                        "{VAL_DEVICE_ID}", device_id);
            if (r != ESP_OK) {
                ESP_LOGE(TAG, "Template expansion overflow in %s (device_id): %s",
                        filepath, esp_err_to_name(r));
                fclose(f);
                free(read_line);
                free(work_line);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "template expansion overflow");
                return ESP_OK;
            }

            r = replace_placeholder_sized(work_line, STREAM_LINE_BUF_SZ,
                                        "{VAL_DEVICE_SERIAL}", device_serial);
            if (r != ESP_OK) {
                ESP_LOGE(TAG, "Template expansion overflow in %s (device_serial): %s",
                        filepath, esp_err_to_name(r));
                fclose(f);
                free(read_line);
                free(work_line);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "template expansion overflow");
                return ESP_OK;
            }
#endif
            // Stream it out
            esp_err_t err = httpd_resp_send_chunk(req, work_line, HTTPD_RESP_USE_STRLEN);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "send_chunk failed (err=0x%x)", (unsigned)err);
                break;
            }

            // Optional: yield a tick if you ever template larger pages to avoid WDT starvation
            // vTaskDelay(1);
        }

        free(read_line);
        free(work_line);

#if ENABLE_PLACEHOLDER_REPLACEMENT
        free(device_id);
        free(device_serial);
#endif

        httpd_resp_set_hdr(req, "Connection", "close");
        // End chunked response
        httpd_resp_send_chunk(req, NULL, 0);

    } else {
        ESP_LOGI(TAG, "Streaming file in raw chunks: %s", filepath);
        /* Retrieve the pointer to scratch buffer for temporary storage */
        char *chunk = ((struct http_file_server_data *)req->user_ctx)->scratch;
        size_t chunksize;
        do {
            /* Read file in chunks into the scratch buffer */
            chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, f);

            if (chunksize > 0) {
                /* Send the buffer contents as HTTP response chunk */
                if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                    fclose(f);
                    ESP_LOGE(TAG, "File sending failed!");
                    /* Abort sending file */
                    httpd_resp_sendstr_chunk(req, NULL);
                    /* Respond with 500 Internal Server Error */
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
            }

            /* Keep looping till the whole file is sent */
        } while (chunksize != 0);

        ESP_LOGI(TAG, "File sending complete");

        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send_chunk(req, NULL, 0);


    }
    
    fclose(f);
    return ESP_OK;
}