#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi manager configuration
#define WIFI_CONNECT_TIMEOUT_MS     15000
#define WIFI_RECONNECT_ATTEMPTS     3
#define NTP_SYNC_TIMEOUT_MS         10000
#define NTP_RETRY_ATTEMPTS          3

// WiFi connection status
typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} wifi_state_t;

// NTP sync status
typedef enum {
    NTP_STATE_NOT_SYNCED,
    NTP_STATE_SYNCING,
    NTP_STATE_SYNCED,
    NTP_STATE_FAILED
} ntp_state_t;

// WiFi statistics
typedef struct {
    uint32_t connect_attempts;
    uint32_t connect_failures;
    uint32_t disconnect_count;
    int8_t rssi;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint64_t last_connect_time;
    uint64_t total_connect_time;
} wifi_stats_t;

/**
 * @brief Initialize WiFi manager
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Deinitialize WiFi manager
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief Connect to WiFi network
 * 
 * @param ssid WiFi network SSID
 * @param password WiFi network password
 * @param timeout_ms Connection timeout in milliseconds
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_connect(const char* ssid, const char* password, uint32_t timeout_ms);

/**
 * @brief Disconnect from WiFi network
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get current WiFi state
 * 
 * @return Current WiFi state
 */
wifi_state_t wifi_manager_get_state(void);

/**
 * @brief Get WiFi connection statistics
 * 
 * @param stats Pointer to stats structure to fill
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_get_stats(wifi_stats_t* stats);

/**
 * @brief Synchronize time with NTP server
 * 
 * @param server_url NTP server URL (NULL for default)
 * @param timeout_ms Sync timeout in milliseconds
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_sync_time(const char* server_url, uint32_t timeout_ms);

/**
 * @brief Get NTP sync status
 * 
 * @return Current NTP sync state
 */
ntp_state_t wifi_manager_get_ntp_state(void);

/**
 * @brief Check if time is synchronized
 * 
 * @return true if time is synced, false otherwise
 */
bool wifi_manager_is_time_synced(void);

/**
 * @brief Get current time as Unix timestamp
 * 
 * @return Unix timestamp in seconds
 */
uint64_t wifi_manager_get_timestamp(void);

/**
 * @brief Get WiFi signal strength (RSSI)
 * 
 * @return RSSI value in dBm
 */
int8_t wifi_manager_get_rssi(void);

/**
 * @brief Perform quick WiFi connection for time sync only
 * 
 * This function optimizes for minimal connection time and power usage
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_quick_time_sync(void);

/**
 * @brief Set low power mode for WiFi
 * 
 * @param enable Enable/disable low power mode
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_set_low_power_mode(bool enable);

#ifdef __cplusplus
}
#endif