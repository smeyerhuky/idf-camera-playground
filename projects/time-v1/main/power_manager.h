#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize power manager
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t power_manager_init(void);

/**
 * @brief Deinitialize power manager
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t power_manager_deinit(void);

#ifdef __cplusplus
}
#endif