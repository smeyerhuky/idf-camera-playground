#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize camera manager
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_manager_init(void);

/**
 * @brief Deinitialize camera manager
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_manager_deinit(void);

/**
 * @brief Take a snapshot
 * 
 * @param directory Directory to save snapshot
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_manager_take_snapshot(const char* directory);

/**
 * @brief Record video
 * 
 * @param filename Video filename
 * @param duration_sec Duration in seconds
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_manager_record_video(const char* filename, uint32_t duration_sec);

#ifdef __cplusplus
}
#endif