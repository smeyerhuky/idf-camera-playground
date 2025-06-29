#pragma once

#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// SD card configuration
#define SD_MOUNT_POINT          "/sdcard"
#define SD_MAX_FILES            5
#define SD_ALLOCATION_UNIT_SIZE 16 * 1024

// SD card pins for ESP32-S3 XIAO Sense (verified working configuration)
#define SD_PIN_MISO             GPIO_NUM_8
#define SD_PIN_MOSI             GPIO_NUM_9
#define SD_PIN_CLK              GPIO_NUM_7
#define SD_PIN_CS               GPIO_NUM_21

// SD card statistics
typedef struct {
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    uint32_t card_size_mb;
    bool card_present;
    bool initialized;
} sd_stats_t;

/**
 * @brief Initialize SD card
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_tester_init(void);

/**
 * @brief Deinitialize SD card
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_tester_deinit(void);

/**
 * @brief Check if SD card is available
 * 
 * @return true if SD card is available, false otherwise
 */
bool sd_tester_is_available(void);

/**
 * @brief Get SD card statistics
 * 
 * @param stats Pointer to stats structure to fill
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_tester_get_stats(sd_stats_t* stats);

/**
 * @brief Write stats to file on SD card
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_tester_write_stats_to_file(void);

/**
 * @brief List files on SD card root directory
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_tester_list_files(void);

/**
 * @brief Delete old stats files to free up space
 * 
 * @param max_files Maximum number of stats files to keep
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_tester_cleanup_old_stats(int max_files);

#ifdef __cplusplus
}
#endif