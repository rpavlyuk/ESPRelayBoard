/**
 * @file p_web_utils.c
 * @brief Web utility functions for handling placeholders and file checks.
 */

#include <string.h>
#include "esp_err.h"
#include "esp_http_server.h"
#include "p_utils.h"
#include "p_web_utils.h"

/**
 * @brief: Helper function to replace placeholders in the template with actual values
 * 
 * This function replaces the placeholders in the HTML template with the actual values.
 * 
 * @param[in,out] html_output: The HTML template to modify
 * @param[in] placeholder: The placeholder to replace
 * @param[in] value: The value to insert
 * 
 * @return void No return value
 */ 
void replace_placeholder(char *html_output, const char *placeholder, const char *value) {
    char *pos;
    while ((pos = strstr(html_output, placeholder)) != NULL) {
        size_t len_placeholder = strlen(placeholder);
        size_t len_value = strlen(value);
        size_t len_after = strlen(pos + len_placeholder);

        // Shift the rest of the string to make space for the replacement value
        memmove(pos + len_value, pos + len_placeholder, len_after + 1);

        // Copy the replacement value into the position of the placeholder
        memcpy(pos, value, len_value);
    }
}

/**
 * @brief Replace placeholders in a bounded (sized) buffer.
 *
 * Safer alternative to replace_placeholder(): prevents buffer overflow and
 * avoids unbounded strlen() scans past the buffer capacity.
 *
 * Behavior:
 * - Modifies @p buf in place.
 * - Replaces ALL occurrences of @p placeholder with @p value.
 * - If expansion would exceed @p cap (including final '\0'), returns ESP_ERR_NO_MEM.
 *
 * @param[in,out] buf         Buffer containing a NUL-terminated string.
 * @param[in]     cap         Total buffer capacity in bytes (including space for '\0').
 * @param[in]     placeholder Placeholder substring to replace (must be non-empty).
 * @param[in]     value       Replacement string (NULL treated as "").
 *
 * @return
 *   ESP_OK on success
 *   ESP_ERR_INVALID_ARG on bad arguments or non-terminated input within cap
 *   ESP_ERR_NO_MEM if output would exceed cap
 */
esp_err_t replace_placeholder_sized(char *buf, size_t cap,
                                   const char *placeholder,
                                   const char *value) {
    if (!buf || cap == 0 || !placeholder) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!value) {
        value = "";
    }

    const size_t ph_len = strlen(placeholder);
    const size_t val_len = strlen(value);

    if (ph_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Ensure buf is NUL-terminated within cap
    size_t cur_len = strnlen(buf, cap);
    if (cur_len >= cap) {
        return ESP_ERR_INVALID_ARG;
    }

    // Search from the beginning each time, but always bounded by cap via strnlen checks
    char *pos = buf;

    while (true) {
        // strstr() is fine as long as buf is terminated within cap (checked above)
        pos = strstr(pos, placeholder);
        if (!pos) {
            break;
        }

        // Re-evaluate current length (still bounded)
        cur_len = strnlen(buf, cap);
        if (cur_len >= cap) {
            return ESP_ERR_INVALID_ARG;
        }

        const size_t head_off = (size_t)(pos - buf);

        // Sanity: placeholder must fully reside within the current string
        if (head_off + ph_len > cur_len) {
            return ESP_ERR_INVALID_ARG;
        }

        // Tail length after the placeholder (bytes after it, not including '\0')
        const size_t tail_len = cur_len - (head_off + ph_len);

        // New length after replacement
        const size_t new_len = cur_len - ph_len + val_len;

        // Need space for '\0' too
        if (new_len >= cap) {
            return ESP_ERR_NO_MEM;
        }

        // Move tail (including '\0') to its new position
        memmove(buf + head_off + val_len,
                buf + head_off + ph_len,
                tail_len + 1);

        // Copy in replacement value
        if (val_len > 0) {
            memcpy(buf + head_off, value, val_len);
        }

        // Continue searching *after* the inserted value to avoid infinite loops
        pos = buf + head_off + val_len;
    }

    return ESP_OK;
}

/** File handling utility functions **/

/**
 * @brief Checks if a file is minified based on its name: returns true if it contains suffix ".min."
 * @param file_name The name of the file to check.
 * @return true if the file is minified, false otherwise.
 */
bool is_minified_file(const char *file_name) {
    if (file_name == NULL) return false;
    return strstr(file_name, ".min.") != NULL;
}

/**
 * @brief Determines the content type based on the file extension.
 *
 * This function takes a file path as input and returns the corresponding
 * MIME content type based on the file extension. If the extension is not
 * recognized, it defaults to "application/octet-stream".
 *
 * @param path The file path to analyze.
 * @return The corresponding content type as a string.
 */
const char *content_type_from_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";

    if (strcasecmp(dot, ".html") == 0) return "text/html";
    if (strcasecmp(dot, ".css")  == 0) return "text/css";
    if (strcasecmp(dot, ".js")   == 0) return "application/javascript";
    if (strcasecmp(dot, ".json") == 0) return "application/json";
    if (strcasecmp(dot, ".svg")  == 0) return "image/svg+xml";
    if (strcasecmp(dot, ".png")  == 0) return "image/png";
    if (strcasecmp(dot, ".jpg")  == 0 || strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(dot, ".ico")  == 0) return "image/x-icon";
    if (strcasecmp(dot, ".txt")  == 0) return "text/plain";

    return "application/octet-stream";
}

/**
 * @brief Determines if the MIME type of a file is text-based.
 *
 * This function checks the content type of the given file path and returns true
 * if it is a text-based MIME type (e.g., text/html, text/css, application/javascript, application/json).
 *
 * @param path The file path to analyze.
 * @return true if the MIME type is text-based, false otherwise.
 */
bool is_text_based_mimetype(const char *path) {
    const char *ctype = content_type_from_ext(path);
    return ctype && (strstr(ctype, "text/") == ctype || strcmp(ctype, "application/javascript") == 0 || strcmp(ctype, "application/json") == 0);
}

/**
 * @brief URL decoding function
 * 
 * This function decodes a URL-encoded string in place.
 * It converts percent-encoded characters to their corresponding ASCII characters
 * and replaces '+' with space.
 *
 * @param src The URL-encoded string to decode.
 * @return void
 */
void url_decode(char *src) {
    char *dst = src;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            // Convert hex to character
            *dst = (char)((hex_to_dec(src[1]) << 4) | hex_to_dec(src[2]));
            src += 2;
        } else if (*src == '+') {
            // Replace '+' with space
            *dst = ' ';
        } else {
            *dst = *src;
        }
        src++;
        dst++;
    }
    *dst = '\0'; // Null-terminate the decoded string
}

/* Extract data from HTTP requests */
/**
 * @brief Extracts the value of a specified parameter from a buffer (POST request).
 *
 * This function searches for a parameter name within a given buffer and extracts its corresponding value.
 * The extracted value is then stored in the provided output buffer.
 *
 * @param buf The buffer containing the parameters.
 * @param param_name The name of the parameter to search for.
 * @param output The buffer where the extracted parameter value will be stored.
 * @param output_size The size of the output buffer.
 * @return An integer providing the length of the extracted value, or 0 if the parameter is not found.
 */
int extract_param_value(const char *buf, const char *param_name, char *output, size_t output_size) {
    char *start = strstr(buf, param_name);
    if (start != NULL) {
        start += strlen(param_name);  // Move to the value part
        char *end = strchr(start, '&');  // Find the next '&'
        if (end == NULL) {
            end = start + strlen(start);  // If no '&' found, take till the end of the string
        }
        size_t len = end - start;
        if (len >= output_size) {
            len = output_size - 1;  // Ensure we don't overflow the output buffer
        }
        strncpy(output, start, len);
        output[len] = '\0';  // Null-terminate the result
        return len;  // Return the length of the extracted value
    } else {
        output[0] = '\0';  // If not found, return an empty string
        return 0;  // Return 0 length when not found
    }
}

/**
 * @brief Retrieves the value of a GET query parameter from an HTTP request.
 *
 * This function extracts the value of a specified query parameter from the URL of an HTTP request.
 * It allocates memory for the query string, retrieves it, and then searches for the specified parameter.
 * The extracted value is stored in the provided output buffer.
 *
 * @param req Pointer to the HTTP request structure.
 * @param name The name of the query parameter to retrieve.
 * @param out Buffer where the extracted parameter value will be stored.
 * @param out_sz Size of the output buffer.
 * @return ESP_OK if the parameter is found and extracted successfully,
 *         ESP_ERR_NOT_FOUND if the parameter is not found,
 *         ESP_ERR_INVALID_ARG if any argument is invalid,
 *         ESP_ERR_NO_MEM if memory allocation fails.
 */
esp_err_t extract_param_value_from_get_query(httpd_req_t *req,
                                     const char *name,
                                     char *out,
                                     size_t out_sz)
{
    if (!req || !name || !out || out_sz == 0) return ESP_ERR_INVALID_ARG;

    out[0] = '\0';

    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0) return ESP_ERR_NOT_FOUND;

    char *q = malloc(qlen + 1);
    if (!q) return ESP_ERR_NO_MEM;

    esp_err_t err = httpd_req_get_url_query_str(req, q, qlen + 1);
    if (err != ESP_OK) {
        free(q);
        return err;
    }

    err = httpd_query_key_value(q, name, out, out_sz);
    free(q);

    return err; // ESP_OK or ESP_ERR_NOT_FOUND
}

