#include "freertos/FreeRTOS.h"   // must be first
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"      // if you use queues

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"  // Use the appropriate scheme for provisioning
#include "esp_event.h"

#include "wifi.h"
#include "flags.h"

char softap_ssid[32];       // Buffer for the generated SSID
char softap_password[64];   // Buffer for the generated password

esp_netif_t *esp_netif_sta;


static const char *wifi_disc_reason_to_str(uint8_t r){
    switch (r) {
    case WIFI_REASON_UNSPECIFIED: return "UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE: return "AUTH_LEAVE";
    case WIFI_REASON_ASSOC_EXPIRE: return "ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_TOOMANY: return "ASSOC_TOOMANY";
    case WIFI_REASON_NOT_AUTHED: return "NOT_AUTHED";
    case WIFI_REASON_NOT_ASSOCED: return "NOT_ASSOCED";
    case WIFI_REASON_ASSOC_LEAVE: return "ASSOC_LEAVE";
    case WIFI_REASON_ASSOC_NOT_AUTHED: return "ASSOC_NOT_AUTHED";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD: return "DISASSOC_PWRCAP_BAD";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD: return "DISASSOC_SUPCHAN_BAD";
    case WIFI_REASON_IE_INVALID: return "IE_INVALID";
    case WIFI_REASON_MIC_FAILURE: return "MIC_FAILURE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "GROUP_KEY_UPDATE_TIMEOUT";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS: return "IE_IN_4WAY_DIFFERS";
    case WIFI_REASON_GROUP_CIPHER_INVALID: return "GROUP_CIPHER_INVALID";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID: return "PAIRWISE_CIPHER_INVALID";
    case WIFI_REASON_AKMP_INVALID: return "AKMP_INVALID";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION: return "UNSUPP_RSN_IE_VERSION";
    case WIFI_REASON_INVALID_RSN_IE_CAP: return "INVALID_RSN_IE_CAP";
    case WIFI_REASON_802_1X_AUTH_FAILED: return "802_1X_AUTH_FAILED";
    case WIFI_REASON_CIPHER_SUITE_REJECTED: return "CIPHER_SUITE_REJECTED";
    case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
    default: return "UNKNOWN";
    }
}

void generate_softap_credentials() {
    uint8_t mac[6];  // Array to hold the MAC address
    esp_read_mac(mac, ESP_MAC_WIFI_STA);  // Use esp_read_mac to get the MAC address

    // Extract the last 3 bytes (6 characters in hexadecimal) of the MAC address
    snprintf(softap_ssid, sizeof(softap_ssid), "PROV_AP_%02X%02X%02X", mac[3], mac[4], mac[5]);

    // Ensure the password buffer is large enough to accommodate the SSID and "1234"
    snprintf(softap_password, sizeof(softap_password), "%s1234", softap_ssid);

    ESP_LOGI(TAG, "Generated SSID: %s", softap_ssid);
    ESP_LOGI(TAG, "Generated Password: %s", softap_password);
}

/**
 * @brief Log the currently configured STA credentials stored in NVS (Wi-Fi driver config)
 */
esp_err_t log_sta_credentials(void) {
    wifi_config_t cfg = {0};
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_get_config(WIFI_IF_STA) failed: %s", esp_err_to_name(err));
        return err;
    }

    // SSID/pass are not guaranteed to be null-terminated in all cases; make safe copies
    char ssid[33] = {0};      // 32 + NUL
    char pass[65] = {0};      // 64 + NUL

    memcpy(ssid, cfg.sta.ssid, sizeof(cfg.sta.ssid));
    memcpy(pass, cfg.sta.password, sizeof(cfg.sta.password));
    ssid[32] = '\0';
    pass[64] = '\0';

    // ⚠️ Strong recommendation: DO NOT log the password in production.
    ESP_LOGI(TAG, "Connecting to SSID '%s'%s", ssid, pass[0] ? " (password set)" : " (open network)");
    ESP_LOGI(TAG, "STA cfg: authmode=%d pmf_cap=%d pmf_req=%d",
         cfg.sta.threshold.authmode,
         cfg.sta.pmf_cfg.capable,
         cfg.sta.pmf_cfg.required);
    // If you *really* want it for debugging, gate it behind a debug flag:
    // ESP_LOGI(TAG, "Connecting to SSID '%s' with key '%s'", ssid, pass);

    return ESP_OK;
}

// Function to handle Wi-Fi and IP events
static void wifi_provisioning_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_START) {
        ESP_LOGI(TAG, "Provisioning started");
    } else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_RECV) {
        wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
        ESP_LOGI(TAG, "Received Wi-Fi credentials - SSID: %s, Password: %s",
                 (const char *) wifi_sta_cfg->ssid,
                 (const char *) wifi_sta_cfg->password);
    } else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_END) {
        wifi_prov_mgr_deinit();
        ESP_LOGI(TAG, "Wi-Fi Provisioning completed. Restarting the device now.");
        esp_restart();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {  // Corrected event
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        xEventGroupSetBits(g_sys_events, BIT_WIFI_CONNECTED);
        log_network_configuration(event->esp_netif);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "STA disconnected: reason=%u (%s)", d->reason, wifi_disc_reason_to_str(d->reason));
        ESP_LOGW(TAG, "SSID: %.*s", d->ssid_len, (const char *)d->ssid);
        xEventGroupClearBits(g_sys_events, BIT_WIFI_CONNECTED);
        xEventGroupClearBits(g_sys_events, BIT_MQTT_CONNECTED);
        xEventGroupClearBits(g_sys_events, BIT_MQTT_READY);
        dump_sys_bits("WIFI_EVENT_STA_DISCONNECTED");
        vTaskDelay(2000 / portTICK_PERIOD_MS);  // Delay for 2 seconds before reconnecting
        ESP_LOGI(TAG, "Calling WiFi reconnecting logic...");
        ESP_ERROR_CHECK(esp_wifi_connect());    
    }
}

// initialize wifi
void initialize_wifi() {
   // Initialize Wi-Fi in station mode
    ESP_ERROR_CHECK(esp_netif_init());

    // Register event handlers for Wi-Fi and Provisioning
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_provisioning_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_provisioning_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_provisioning_event_handler, NULL));  // Corrected registration

    esp_netif_sta = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

// Function to start Wi-Fi
void start_wifi(bool provisioned) {
    if (!provisioned) {
        esp_netif_create_default_wifi_ap();

        generate_softap_credentials();

        ESP_LOGI(TAG, "Starting provisioning");

        // Start Wi-Fi provisioning with SoftAP mode
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, NULL, softap_ssid, softap_password));
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi");

        // Log the stored STA credentials
        ESP_ERROR_CHECK(log_sta_credentials());
        
        // Start Wi-Fi with stored credentials
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
        ESP_ERROR_CHECK(esp_wifi_start());
        // ESP_ERROR_CHECK(esp_wifi_connect());

        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");  // Set Google's DNS server
        esp_netif_set_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns);

    }

}


void log_network_configuration(esp_netif_t *esp_netif_sta) {
    esp_netif_ip_info_t ip_info;

    ESP_LOGI(TAG, "+---- WIFI Connection Information ----+");
    // Get IP information (IP, netmask, and gateway)
    if (esp_netif_get_ip_info(esp_netif_sta, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
        ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
    } else {
        ESP_LOGE(TAG, "Failed to get IP information");
    }

    // get DNS
    esp_netif_dns_info_t dns_info;
    esp_err_t err = esp_netif_get_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns_info);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "DNS IP: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    } else {
        ESP_LOGE(TAG, "Failed to retrieve DNS info: %s", esp_err_to_name(err));
    }

    // Get default route (i.e., the default gateway)
    esp_ip6_addr_t ip6_info;
    if (esp_netif_get_ip6_linklocal(esp_netif_sta, &ip6_info) == ESP_OK) {
        ESP_LOGI(TAG, "IPv6 Address: " IPV6STR, IPV62STR(ip6_info));
    } else {
        ESP_LOGE(TAG, "Failed to get IPv6 information");
    }
}
