#ifndef P_WEB_UTILS_H
#define P_WEB_UTILS_H

#include "esp_err.h"
#include "esp_http_server.h"

/* Functions declared but must be defined in the upstream component or application */
void assign_static_page_variables(char *html_output);

/* Define functions wintin the p_web_utils component */
void replace_placeholder(char *html_output, const char *placeholder, const char *value);
esp_err_t replace_placeholder_sized(char *buf, size_t cap,
                                   const char *placeholder,
                                   const char *value);


bool is_minified_file(const char *file_name);
const char *content_type_from_ext(const char *path);
bool is_text_based_mimetype(const char *path);

int extract_param_value(const char *buf, const char *param_name, char *output, size_t output_size);
esp_err_t extract_param_value_from_get_query(httpd_req_t *req,
                                            const char *name,
                                            char *out,
                                            size_t out_sz);

void url_decode(char *src);

#endif // P_WEB_UTILS_H