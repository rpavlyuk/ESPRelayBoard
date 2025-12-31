/*
    UDP Client - hardened for logging use

    Key changes:
    - no LWIP_ASSERT() on normal network errors
    - supports hostname resolution (gethostbyname)
    - recreates socket on sendto() failures with backoff
    - always returns ringbuffer items
*/

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
#include "freertos/ringbuf.h"
#else
#include "freertos/message_buffer.h"
#endif

#include "esp_log.h"
#include "lwip/sockets.h"
#include "netdb.h"

#include "net_logging.h"

static const char *TAG = "netlog_udp";

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
extern RingbufHandle_t xRingBufferUDP;
#else
extern MessageBufferHandle_t xMessageBufferUDP;
#endif

// Resolve IPv4 dotted string or hostname into sockaddr_in
static int resolve_dest_addr(const char *host, uint16_t port, struct sockaddr_in *out)
{
    memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    out->sin_port = htons(port);

    // Try literal IPv4 first
    out->sin_addr.s_addr = inet_addr(host);
    if (out->sin_addr.s_addr != 0xffffffff) {
        return 0;
    }

    // Fallback to DNS
    struct hostent *hp = gethostbyname(host);
    if (!hp || !hp->h_addr) {
        return -1;
    }

    struct ip4_addr *ip4_addr = (struct ip4_addr *)hp->h_addr;
    out->sin_addr.s_addr = ip4_addr->addr;
    return 0;
}

static int open_udp_socket(void)
{
    int fd = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        ESP_LOGW(TAG, "socket() failed: errno=%d", errno);
        return -1;
    }

    // Optional: avoid any surprising long blocks
    struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    return fd;
}

void udp_client(void *pvParameters)
{
    PARAMETER_t param;
    memcpy(&param, pvParameters, sizeof(PARAMETER_t));

    ESP_LOGI(TAG, "Start: %s:%u", param.ipv4, (unsigned)param.port);

    struct sockaddr_in addr;
    if (resolve_dest_addr(param.ipv4, param.port, &addr) != 0) {
        ESP_LOGE(TAG, "DNS/addr resolve failed for host '%s'", param.ipv4);
        vTaskDelete(NULL);
        return;
    }

    // Create socket with backoff
    int fd = -1;
    int backoff_ms = 250;
    const int backoff_max_ms = 5000;

    while (fd < 0) {
        fd = open_udp_socket();
        if (fd >= 0) break;

        vTaskDelay(pdMS_TO_TICKS(backoff_ms));
        if (backoff_ms < backoff_max_ms) backoff_ms *= 2;
        if (backoff_ms > backoff_max_ms) backoff_ms = backoff_max_ms;
    }

    // Ready to receive
    xTaskNotifyGive(param.taskHandle);

    int send_fail_streak = 0;

    while (1) {
#if CONFIG_NET_LOGGING_USE_RINGBUFFER
        size_t received = 0;
        char *buffer = (char *)xRingbufferReceive(xRingBufferUDP, &received, portMAX_DELAY);
        if (!buffer || received == 0) {
            continue;
        }
#else
        char buffer[xItemSize];
        size_t received = xMessageBufferReceive(xMessageBufferUDP, buffer, sizeof(buffer), portMAX_DELAY);
        if (received == 0) {
            continue;
        }
#endif

        // UDP: sendto() should send full datagram or fail, but we still handle short returns safely
        int ret = lwip_sendto(fd, buffer, received, 0, (struct sockaddr *)&addr, sizeof(addr));

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
        // Always return the item regardless of send result
        vRingbufferReturnItem(xRingBufferUDP, (void *)buffer);
#endif

        if (ret == (int)received) {
            send_fail_streak = 0;
            continue;
        }

        if (ret < 0) {
            ESP_LOGW(TAG, "sendto() failed: errno=%d (streak=%d)", errno, send_fail_streak + 1);
        } else {
            ESP_LOGW(TAG, "sendto() short write: ret=%d expected=%u (streak=%d)",
                     ret, (unsigned)received, send_fail_streak + 1);
        }

        send_fail_streak++;

        // Close and recreate socket on failures
        if (fd != -1) {
            (void)lwip_close(fd);
            fd = -1;
        }

        // If failures persist, re-resolve hostname periodically (helps with DNS/IP changes)
        if ((send_fail_streak % 10) == 0) {
            if (resolve_dest_addr(param.ipv4, param.port, &addr) != 0) {
                ESP_LOGW(TAG, "Re-resolve failed for '%s' (will keep retrying)", param.ipv4);
            } else {
                ESP_LOGI(TAG, "Re-resolved destination '%s'", param.ipv4);
            }
        }

        // Recreate socket with backoff
        int bo = 250;
        while (fd < 0) {
            fd = open_udp_socket();
            if (fd >= 0) break;
            vTaskDelay(pdMS_TO_TICKS(bo));
            if (bo < backoff_max_ms) bo *= 2;
            if (bo > backoff_max_ms) bo = backoff_max_ms;
        }
    }
}