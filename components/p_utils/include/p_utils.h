#ifndef P_UTILS_H
#define P_UTILS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "cJSON.h"

/* JSON Helper Functions */
bool attach_owned(cJSON *root, const char *key, cJSON *child);
char *print_and_delete(cJSON *root);
void json_value_to_string(const cJSON *v, char *out, size_t out_sz);

/* FreeRTOS Helper Functions */
TickType_t nonzero_ticks(uint32_t ms);

/* Mathematical Helper Functions */
int calculate_median(int* data, int size);

/* WiFi Provisioning Helper Functions */
void wifi_provision_clear(void *buffer, size_t length);

/* String Helper Functions */
void str_trunc_after(char *str, const char *lookup);
int hex_to_dec(char c);

#endif // P_UTILS_H