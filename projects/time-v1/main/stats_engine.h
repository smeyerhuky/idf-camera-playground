#pragma once

#include "esp_err.h"
#include "esp_timer.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stats engine configuration
#define STATS_MAX_BUFFER_SIZE   1000
#define STATS_METRIC_NAME_LEN   32
#define STATS_FLUSH_INTERVAL_MS (60 * 1000)  // 60 seconds

// Metric types
typedef enum {
    STATS_TYPE_COUNTER,    // Incrementing counter
    STATS_TYPE_GAUGE,      // Current value
    STATS_TYPE_TIMING,     // Timing measurement
    STATS_TYPE_EVENT       // Event occurrence
} stats_type_t;

// Metric entry structure
typedef struct {
    uint64_t timestamp_us;
    char name[STATS_METRIC_NAME_LEN];
    float value;
    stats_type_t type;
    uint8_t tags;  // Bitfield for tags
} stats_entry_t;

// Stats engine handle
typedef struct stats_engine_s* stats_engine_handle_t;

/**
 * @brief Initialize the stats engine
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t stats_engine_init(void);

/**
 * @brief Deinitialize the stats engine
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t stats_engine_deinit(void);

/**
 * @brief Log a counter metric
 * 
 * @param name Metric name
 * @param value Counter value
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t stats_engine_log_counter(const char* name, float value);

/**
 * @brief Log a gauge metric
 * 
 * @param name Metric name
 * @param value Gauge value
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t stats_engine_log_gauge(const char* name, float value);

/**
 * @brief Log a timing metric
 * 
 * @param name Metric name
 * @param duration_ms Duration in milliseconds
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t stats_engine_log_timing(const char* name, uint64_t duration_ms);

/**
 * @brief Log an event metric
 * 
 * @param name Event name
 * @param count Event count
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t stats_engine_log_event(const char* name, uint32_t count);

/**
 * @brief Log power consumption metric
 * 
 * @param current_ma Current consumption in mA
 * @param voltage_mv Voltage in mV
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t stats_engine_log_power(float current_ma, float voltage_mv);

/**
 * @brief Log memory usage metric
 * 
 * @param heap_free Free heap in bytes
 * @param heap_total Total heap in bytes
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t stats_engine_log_memory(uint32_t heap_free, uint32_t heap_total);

/**
 * @brief Flush stats buffer to storage
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t stats_engine_flush_to_storage(void);

/**
 * @brief Get stats buffer for transmission
 * 
 * @param buffer Output buffer pointer
 * @param buffer_size Size of output buffer
 * @param entry_count Number of entries copied
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t stats_engine_get_buffer(uint8_t* buffer, size_t buffer_size, uint32_t* entry_count);

/**
 * @brief Clear stats buffer
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t stats_engine_clear_buffer(void);

/**
 * @brief Get current buffer usage
 * 
 * @return Number of entries in buffer
 */
uint32_t stats_engine_get_buffer_usage(void);

/**
 * @brief Check if buffer is full
 * 
 * @return true if buffer is full, false otherwise
 */
bool stats_engine_is_buffer_full(void);

#ifdef __cplusplus
}
#endif