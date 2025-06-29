#include "wifi_manager.h"
#include "stats_engine.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "wifi_manager";

// Event bits
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT          BIT1
#define NTP_SYNCED_BIT         BIT2

// WiFi manager structure
typedef struct {
    EventGroupHandle_t event_group;
    esp_netif_t* netif;
    wifi_state_t wifi_state;
    ntp_state_t ntp_state;
    wifi_stats_t stats;
    bool initialized;
    uint64_t connect_start_time;
} wifi_manager_t;

static wifi_manager_t wifi_mgr = {0};

// Internal function prototypes
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data);
static void ntp_sync_time_cb(struct timeval *tv);
static esp_err_t configure_wifi_power_save(void);

esp_err_t wifi_manager_init(void) {
    if (wifi_mgr.initialized) {
        ESP_LOGW(TAG, "WiFi manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WiFi manager");

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create event group
    wifi_mgr.event_group = xEventGroupCreate();
    if (!wifi_mgr.event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi station
    wifi_mgr.netif = esp_netif_create_default_wifi_sta();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler, NULL));

    // Set WiFi mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Start WiFi before configuring power settings
    ESP_ERROR_CHECK(esp_wifi_start());

    // Configure power save mode
    configure_wifi_power_save();

    // Initialize stats
    memset(&wifi_mgr.stats, 0, sizeof(wifi_stats_t));
    wifi_mgr.wifi_state = WIFI_STATE_DISCONNECTED;
    wifi_mgr.ntp_state = NTP_STATE_NOT_SYNCED;
    wifi_mgr.initialized = true;

    ESP_LOGI(TAG, "WiFi manager initialized successfully");
    return ESP_OK;
}

esp_err_t wifi_manager_deinit(void) {
    if (!wifi_mgr.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing WiFi manager");

    // Disconnect if connected
    wifi_manager_disconnect();

    // Stop WiFi
    esp_wifi_stop();
    esp_wifi_deinit();

    // Cleanup event handlers
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);

    // Cleanup netif
    if (wifi_mgr.netif) {
        esp_netif_destroy_default_wifi(wifi_mgr.netif);
        wifi_mgr.netif = NULL;
    }

    // Cleanup event group
    if (wifi_mgr.event_group) {
        vEventGroupDelete(wifi_mgr.event_group);
        wifi_mgr.event_group = NULL;
    }

    wifi_mgr.initialized = false;
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char* ssid, const char* password, uint32_t timeout_ms) {
    if (!wifi_mgr.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Connecting to WiFi network: %s", ssid);
    wifi_mgr.connect_start_time = esp_timer_get_time();
    wifi_mgr.stats.connect_attempts++;

    // Configure WiFi
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    wifi_mgr.wifi_state = WIFI_STATE_CONNECTING;

    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(wifi_mgr.event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi network: %s", ssid);
        wifi_mgr.wifi_state = WIFI_STATE_CONNECTED;
        
        uint64_t connect_time = (esp_timer_get_time() - wifi_mgr.connect_start_time) / 1000;
        wifi_mgr.stats.total_connect_time += connect_time;
        wifi_mgr.stats.last_connect_time = esp_timer_get_time() / 1000000;
        
        stats_engine_log_timing("wifi.connect_time_ms", connect_time);
        stats_engine_log_event("wifi.connected", 1);
        
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi network: %s", ssid);
        wifi_mgr.wifi_state = WIFI_STATE_FAILED;
        wifi_mgr.stats.connect_failures++;
        stats_engine_log_event("wifi.connect_failed", 1);
        return ESP_ERR_WIFI_CONN;
    } else {
        ESP_LOGE(TAG, "WiFi connection timeout");
        wifi_mgr.wifi_state = WIFI_STATE_FAILED;
        wifi_mgr.stats.connect_failures++;
        stats_engine_log_event("wifi.connect_timeout", 1);
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t wifi_manager_disconnect(void) {
    if (!wifi_mgr.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (wifi_mgr.wifi_state == WIFI_STATE_CONNECTED) {
        ESP_LOGI(TAG, "Disconnecting from WiFi");
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        wifi_mgr.stats.disconnect_count++;
        stats_engine_log_event("wifi.disconnected", 1);
    }

    wifi_mgr.wifi_state = WIFI_STATE_DISCONNECTED;
    return ESP_OK;
}

bool wifi_manager_is_connected(void) {
    return wifi_mgr.wifi_state == WIFI_STATE_CONNECTED;
}

wifi_state_t wifi_manager_get_state(void) {
    return wifi_mgr.wifi_state;
}

esp_err_t wifi_manager_get_stats(wifi_stats_t* stats) {
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(stats, &wifi_mgr.stats, sizeof(wifi_stats_t));
    return ESP_OK;
}

esp_err_t wifi_manager_sync_time(const char* server_url, uint32_t timeout_ms) {
    if (!wifi_mgr.initialized || !wifi_manager_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Synchronizing time with NTP server");
    wifi_mgr.ntp_state = NTP_STATE_SYNCING;

    // Configure SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    if (server_url) {
        esp_sntp_setservername(0, server_url);
    } else {
        esp_sntp_setservername(0, "pool.ntp.org");
    }
    sntp_set_time_sync_notification_cb(ntp_sync_time_cb);
    esp_sntp_init();

    // Wait for time sync
    uint32_t start_time = xTaskGetTickCount();
    while (!sntp_get_sync_status() && 
           (xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout_ms)) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (sntp_get_sync_status()) {
        ESP_LOGI(TAG, "Time synchronized successfully");
        wifi_mgr.ntp_state = NTP_STATE_SYNCED;
        stats_engine_log_event("ntp.synced", 1);
        
        // Log current time
        time_t now;
        time(&now);
        stats_engine_log_gauge("time.unix_timestamp", (float)now);
        
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Time synchronization failed");
        wifi_mgr.ntp_state = NTP_STATE_FAILED;
        stats_engine_log_event("ntp.failed", 1);
        return ESP_ERR_TIMEOUT;
    }
}

ntp_state_t wifi_manager_get_ntp_state(void) {
    return wifi_mgr.ntp_state;
}

bool wifi_manager_is_time_synced(void) {
    return wifi_mgr.ntp_state == NTP_STATE_SYNCED;
}

uint64_t wifi_manager_get_timestamp(void) {
    time_t now;
    time(&now);
    return (uint64_t)now;
}

int8_t wifi_manager_get_rssi(void) {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        wifi_mgr.stats.rssi = ap_info.rssi;
        return ap_info.rssi;
    }
    return -100; // Invalid RSSI
}

esp_err_t wifi_manager_quick_time_sync(void) {
    if (!wifi_mgr.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Performing quick time sync");
    uint64_t start_time = esp_timer_get_time();

    // Connect to WiFi with shorter timeout
    esp_err_t ret = wifi_manager_connect("pianomaster", "happyness", 10000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Quick WiFi connection failed");
        return ret;
    }

    // Sync time with shorter timeout
    ret = wifi_manager_sync_time(NULL, 5000);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Quick time sync failed, but WiFi connected");
    }

    // Disconnect to save power
    wifi_manager_disconnect();

    uint64_t total_time = (esp_timer_get_time() - start_time) / 1000;
    stats_engine_log_timing("wifi.quick_sync_time_ms", total_time);
    
    ESP_LOGI(TAG, "Quick time sync completed in %llu ms", total_time);
    return ret;
}

esp_err_t wifi_manager_set_low_power_mode(bool enable) {
    if (!wifi_mgr.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_ps_type_t ps_type = enable ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE;
    return esp_wifi_set_ps(ps_type);
}

// Internal functions
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected");
        wifi_mgr.wifi_state = WIFI_STATE_DISCONNECTED;
        xEventGroupSetBits(wifi_mgr.event_group, WIFI_FAIL_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_mgr.wifi_state = WIFI_STATE_CONNECTED;
        xEventGroupSetBits(wifi_mgr.event_group, WIFI_CONNECTED_BIT);
    }
}

static void ntp_sync_time_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "NTP time synchronized");
    wifi_mgr.ntp_state = NTP_STATE_SYNCED;
    xEventGroupSetBits(wifi_mgr.event_group, NTP_SYNCED_BIT);
}

static esp_err_t configure_wifi_power_save(void) {
    // Configure power save mode for battery operation
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    
    // Set lower transmit power to save energy
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(44)); // 11 dBm (reduced from 20 dBm)
    
    return ESP_OK;
}