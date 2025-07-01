#include "time_sync.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/apps/sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "time_sync";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static time_sync_config_t s_config = {0};
static int s_retry_num = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started, attempting connection...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi disconnected, reason: %d", event->reason);
        
        if (s_retry_num < s_config.max_retry) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry %d/%d to connect to AP: %s", s_retry_num, s_config.max_retry, s_config.ssid);
        } else {
            ESP_LOGE(TAG, "Failed to connect to AP after %d attempts", s_config.max_retry);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WiFi connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


esp_err_t time_sync_init(const time_sync_config_t *config)
{
    if (!config || !config->ssid || !config->password) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&s_config, config, sizeof(time_sync_config_t));
    
    s_wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    
    return ESP_OK;
}

esp_err_t time_sync_connect_wifi(void)
{
    ESP_LOGI(TAG, "Connecting to WiFi...");
    
    // Configure for WPA/WPA2 networks (most common)
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .bssid_set = false,
            .channel = 0,
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, s_config.ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, s_config.password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
    
    ESP_LOGI(TAG, "Connecting to SSID: '%s' (password length: %d)", s_config.ssid, strlen(s_config.password));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Set WiFi power save mode to improve connection stability
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Perform a quick scan to check if our network is visible
    ESP_LOGI(TAG, "Scanning for available networks...");
    wifi_scan_config_t scan_config = {
        .ssid = (uint8_t*)s_config.ssid,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };
    
    esp_err_t scan_ret = esp_wifi_scan_start(&scan_config, true);
    if (scan_ret == ESP_OK) {
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        ESP_LOGI(TAG, "Found %d access points", ap_count);
        
        if (ap_count > 0) {
            wifi_ap_record_t *ap_list = malloc(ap_count * sizeof(wifi_ap_record_t));
            if (ap_list) {
                esp_wifi_scan_get_ap_records(&ap_count, ap_list);
                bool found_target = false;
                for (int i = 0; i < ap_count; i++) {
                    ESP_LOGI(TAG, "AP[%d]: %s (RSSI: %d, Auth: %d, Ch: %d)", 
                             i, ap_list[i].ssid, ap_list[i].rssi, ap_list[i].authmode, ap_list[i].primary);
                    if (strcmp((char*)ap_list[i].ssid, s_config.ssid) == 0) {
                        found_target = true;
                        ESP_LOGI(TAG, "*** Target network '%s' found! RSSI: %d, Auth: %d ***", 
                                 s_config.ssid, ap_list[i].rssi, ap_list[i].authmode);
                    }
                }
                if (!found_target) {
                    ESP_LOGW(TAG, "Target network '%s' not found in scan results!", s_config.ssid);
                }
                free(ap_list);
            }
        }
    } else {
        ESP_LOGW(TAG, "WiFi scan failed: %s", esp_err_to_name(scan_ret));
    }
    
    ESP_LOGI(TAG, "wifi_init_sta finished.");
    
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            30000 / portTICK_PERIOD_MS);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", s_config.ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", s_config.ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return ESP_FAIL;
    }
}

esp_err_t time_sync_sync_time(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, s_config.ntp_server ? s_config.ntp_server : "pool.ntp.org");
    sntp_init();
    
    setenv("TZ", "MST7MDT,M3.2.0,M11.1.0", 1);
    tzset();
    
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
        
        if (timeinfo.tm_year >= (2016 - 1900)) {
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
            ESP_LOGI(TAG, "Time synchronized: %s", time_str);
            break;
        }
    }
    
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGE(TAG, "Time sync failed");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Time synchronized successfully");
    return ESP_OK;
}

esp_err_t time_sync_disconnect_wifi(void)
{
    ESP_LOGI(TAG, "Disconnecting WiFi to save power");
    sntp_stop();
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    return ESP_OK;
}

esp_err_t time_sync_get_local_time(struct tm *timeinfo)
{
    if (!timeinfo) {
        return ESP_ERR_INVALID_ARG;
    }
    
    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);
    
    if (timeinfo->tm_year < (2016 - 1900)) {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

bool time_sync_is_time_set(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_year >= (2016 - 1900);
}

esp_err_t time_sync_format_timestamp(char *buffer, size_t buffer_size, const char *format)
{
    if (!buffer || !format) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct tm timeinfo;
    esp_err_t ret = time_sync_get_local_time(&timeinfo);
    if (ret != ESP_OK) {
        return ret;
    }
    
    strftime(buffer, buffer_size, format, &timeinfo);
    return ESP_OK;
}

esp_err_t time_sync_get_date_path(char *buffer, size_t buffer_size)
{
    if (!buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return time_sync_format_timestamp(buffer, buffer_size, "timelapse_data/%Y/%m/%d");
}