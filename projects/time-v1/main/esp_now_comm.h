#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ESP-NOW communication
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_now_comm_init(void);

/**
 * @brief Deinitialize ESP-NOW communication
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_now_comm_deinit(void);

/**
 * @brief Send metrics via ESP-NOW
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_now_comm_send_metrics(void);

#ifdef __cplusplus
}
#endif