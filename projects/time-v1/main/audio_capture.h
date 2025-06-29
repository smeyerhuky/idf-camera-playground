#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize audio capture
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t audio_capture_init(void);

/**
 * @brief Deinitialize audio capture
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t audio_capture_deinit(void);

/**
 * @brief Start audio capture
 * 
 * @param duration_sec Duration in seconds
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t audio_capture_start(uint32_t duration_sec);

/**
 * @brief Stop audio capture and save
 * 
 * @param filename Output filename
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t audio_capture_stop_and_save(const char* filename);

#ifdef __cplusplus
}
#endif