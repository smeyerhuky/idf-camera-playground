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

// SD card pins for ESP32-S3 XIAO Sense
#define SD_PIN_MISO             GPIO_NUM_9
#define SD_PIN_MOSI             GPIO_NUM_10
#define SD_PIN_CLK              GPIO_NUM_8
#define SD_PIN_CS               GPIO_NUM_7

// File operation types
typedef enum {
    SD_OP_READ,
    SD_OP_WRITE,
    SD_OP_APPEND,
    SD_OP_DELETE
} sd_operation_t;

// SD card statistics
typedef struct {
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    uint32_t total_files;
    uint32_t read_operations;
    uint32_t write_operations;
    uint32_t error_count;
    float write_speed_kbps;
    float read_speed_kbps;
    bool card_present;
    uint32_t card_size_mb;
} sd_stats_t;

// Async operation callback
typedef void (*sd_callback_t)(esp_err_t result, void* user_data);

// Directory structure for organized storage
typedef enum {
    SD_DIR_ROOT,
    SD_DIR_TIMELAPSE,
    SD_DIR_AUDIO,
    SD_DIR_INIT,
    SD_DIR_STATS,
    SD_DIR_LOGS,
    SD_DIR_CONFIG
} sd_directory_t;

/**
 * @brief Initialize SD card manager
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_init(void);

/**
 * @brief Deinitialize SD card manager
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_deinit(void);

/**
 * @brief Check if SD card is available
 * 
 * @return true if SD card is available, false otherwise
 */
bool sd_manager_is_available(void);

/**
 * @brief Get SD card statistics
 * 
 * @param stats Pointer to stats structure to fill
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_get_stats(sd_stats_t* stats);

/**
 * @brief Create directory structure
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_create_directories(void);

/**
 * @brief Get directory path for given directory type
 * 
 * @param dir_type Directory type
 * @param path Buffer to store path
 * @param path_size Size of path buffer
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_get_directory_path(sd_directory_t dir_type, char* path, size_t path_size);

/**
 * @brief Write data to file
 * 
 * @param filename Full file path
 * @param data Data buffer
 * @param size Data size
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_write_file(const char* filename, const void* data, size_t size);

/**
 * @brief Append data to file
 * 
 * @param filename Full file path
 * @param data Data buffer
 * @param size Data size
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_append_file(const char* filename, const void* data, size_t size);

/**
 * @brief Read data from file
 * 
 * @param filename Full file path
 * @param buffer Buffer to store data
 * @param buffer_size Buffer size
 * @param bytes_read Actual bytes read
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_read_file(const char* filename, void* buffer, size_t buffer_size, size_t* bytes_read);

/**
 * @brief Delete file
 * 
 * @param filename Full file path
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_delete_file(const char* filename);

/**
 * @brief Check if file exists
 * 
 * @param filename Full file path
 * @return true if file exists, false otherwise
 */
bool sd_manager_file_exists(const char* filename);

/**
 * @brief Get file size
 * 
 * @param filename Full file path
 * @param size Pointer to store file size
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_get_file_size(const char* filename, size_t* size);

/**
 * @brief Write data to file asynchronously
 * 
 * @param filename Full file path
 * @param data Data buffer (must remain valid until callback)
 * @param size Data size
 * @param callback Completion callback
 * @param user_data User data for callback
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_write_file_async(const char* filename, const void* data, size_t size,
                                     sd_callback_t callback, void* user_data);

/**
 * @brief Create timestamped filename
 * 
 * @param dir_type Directory type
 * @param prefix File prefix
 * @param extension File extension (without dot)
 * @param buffer Buffer to store filename
 * @param buffer_size Buffer size
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_create_timestamped_filename(sd_directory_t dir_type, const char* prefix,
                                                const char* extension, char* buffer, size_t buffer_size);

/**
 * @brief Sync filesystem to ensure data is written
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_sync(void);

/**
 * @brief List files in directory
 * 
 * @param dir_path Directory path
 * @param files Array to store filenames
 * @param max_files Maximum number of files to list
 * @param file_count Number of files found
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_list_files(const char* dir_path, char files[][256], size_t max_files, size_t* file_count);

/**
 * @brief Cleanup old files based on age or count
 * 
 * @param dir_type Directory to clean
 * @param max_age_days Maximum age in days (0 = no age limit)
 * @param max_files Maximum number of files (0 = no file limit)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_cleanup_old_files(sd_directory_t dir_type, uint32_t max_age_days, uint32_t max_files);

/**
 * @brief Get free space on SD card
 * 
 * @param free_bytes Pointer to store free bytes
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_get_free_space(uint64_t* free_bytes);

/**
 * @brief Enable/disable low power mode for SD card
 * 
 * @param enable Enable/disable low power mode
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_manager_set_low_power_mode(bool enable);

#ifdef __cplusplus
}
#endif