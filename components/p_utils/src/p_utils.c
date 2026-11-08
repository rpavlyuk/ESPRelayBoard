/**
 * @file p-utils.c
 * @brief Utility functions library
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "p_utils.h"

/** JSON utility functions library for attaching and printing cJSON objects **/

/**
 * @brief Attaches a child cJSON object to a parent cJSON object with a specified key.
 * @param root The parent cJSON object.
 * @param key The key under which the child will be attached.
 * @param child The child cJSON object to attach.
 * @return true if the child was successfully attached, false otherwise.
 * 
 * @note The child object will be deleted if it cannot be attached to the parent.
 * @see cJSON_AddItemToObject
 */
bool attach_owned(cJSON *root, const char *key, cJSON *child)
{
    if (child == NULL) return false;
    if (!cJSON_AddItemToObject(root, key, child)) {
        cJSON_Delete(child);
        return false;
    }
    return true;
}

/**
 * @brief Prints a cJSON object to a string and deletes the cJSON object.
 * @param root The cJSON object to print and delete.
 * @return A string representation of the cJSON object, or NULL if the object is NULL.
 * 
 * @note The caller is responsible for freeing the returned string.
 * @see cJSON_Print
 */
char *print_and_delete(cJSON *root)
{
    if (root == NULL) return NULL;
    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    return text;
}

/**
 * @brief Converts a cJSON value to its string representation.
 *
 * This function takes a cJSON value and converts it to a string representation.
 * The resulting string is stored in the provided output buffer.
 *
 * @param v Pointer to the cJSON value to convert.
 * @param out Output buffer where the string representation will be stored.
 * @param out_sz Size of the output buffer.
 */
void json_value_to_string(const cJSON *v, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';

    if (!v) {
        strlcpy(out, "<null>", out_sz);
        return;
    }

    if (cJSON_IsString(v)) {
        strlcpy(out, v->valuestring ? v->valuestring : "", out_sz);
    } else if (cJSON_IsNumber(v)) {
        // cJSON numbers are double internally; valueint is OK for ints
        // Use integer formatting if it looks integer-ish
        double d = v->valuedouble;
        if ((double)((int64_t)d) == d) {
            snprintf(out, out_sz, "%lld", (long long)((int64_t)d));
        } else {
            snprintf(out, out_sz, "%.6f", d);
        }
    } else if (cJSON_IsBool(v)) {
        strlcpy(out, cJSON_IsTrue(v) ? "true" : "false", out_sz);
    } else if (cJSON_IsNull(v)) {
        strlcpy(out, "null", out_sz);
    } else {
        // object/array/blob-ish: store compact JSON
        char *tmp = cJSON_PrintUnformatted((cJSON *)v);
        if (tmp) {
            strlcpy(out, tmp, out_sz);
            free(tmp);
        } else {
            strlcpy(out, "<unprintable>", out_sz);
        }
    }
}


/** FreeRTOS utility helper functions **/
/**
 * @brief Converts milliseconds to FreeRTOS ticks, ensuring a non-zero value.
 * @param ms The time in milliseconds.
 * @return The equivalent time in FreeRTOS ticks, with a minimum of 1 tick.
 */
TickType_t nonzero_ticks(uint32_t ms) {
    TickType_t ticks = pdMS_TO_TICKS(ms);
    return ticks != 0 ? ticks : 1;
}

/** Mathematical utility functions **/

/**
 * @brief Calculates the median of an array of integers.
 * @param data The array of integers.
 * @param size The number of elements in the array.
 * @return The median value of the array, or 0 if the array is NULL or empty.
 */
int calculate_median(int* data, int size) {
    if (data == NULL || size <= 0) return 0;
    for (int i = 0; i < size - 1; ++i) {
        for (int j = i + 1; j < size; ++j) {
            if (data[i] > data[j]) {
                int tmp = data[i]; data[i] = data[j]; data[j] = tmp;
            }
        }
    }
    if (size % 2 != 0) return data[size / 2];
    return (int)(((int64_t)data[size / 2 - 1] + data[size / 2]) / 2);
}

/** WFI helper functions **/

/** 
 * @brief Clear temporary credential buffers before returning from the HTTP handler.
 * @param buffer The buffer to clear.
 * @param length The length of the buffer.
 */
void wifi_provision_clear(void *buffer, size_t length) {
    volatile unsigned char *p = buffer;
    while (length--) *p++ = 0;
}

/* String utility helper functions */
/**
 * @brief: Helper function to truncate a string after the last occurrence of a substring
 * 
 * This function truncates a string after the last occurrence of a substring.
 * 
 * @param[in,out] str: The string to truncate
 * @param[in] lookup: The substring to search for
 * 
 * @return void No return value
 */
void str_trunc_after(char *str, const char *lookup) {
    if (str == NULL || lookup == NULL) {
        return; // Return if either input is NULL
    }

    char *last_occurrence = NULL;
    char *current_position = str;

    // Find the last occurrence of the substring 'lookup' in 'str'
    while ((current_position = strstr(current_position, lookup)) != NULL) {
        last_occurrence = current_position;
        current_position += strlen(lookup); // Move past the current match
    }

    // If the 'lookup' substring was found, truncate the string after its last occurrence
    if (last_occurrence != NULL) {
        last_occurrence[strlen(lookup)] = '\0'; // Null-terminate after the last occurrence
    }
}

/**
 * @brief: Helper function to convert a hexadecimal character to its decimal value
 * 
 * This function takes a single hexadecimal character and converts it to its decimal value.
 * It supports both uppercase and lowercase hexadecimal characters.
 * 
 * @param[in] c: The hexadecimal character to convert
 * 
 * @return int: The decimal value of the hexadecimal character, or -1 if the input is not a valid hexadecimal character
 */
int hex_to_dec(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}