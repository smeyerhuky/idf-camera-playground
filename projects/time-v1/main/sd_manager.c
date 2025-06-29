#include "sd_manager.h"
#include "stats_engine.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdkconfig.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

static const char *TAG = "sd_manager";

// SD manager structure
typedef struct {
    sdmmc_card_t* card;
    sdmmc_host_t host;
    sdspi_device_config_t slot_config;
    esp_vfs_fat_mount_config_t mount_config;
    sd_stats_t stats;
    SemaphoreHandle_t mutex;
    bool initialized;
    bool card_mounted;
} sd_manager_t;

static sd_manager_t sd_mgr = {0};

// Directory paths
static const char* sd_directories[] = {
    [SD_DIR_ROOT] = SD_MOUNT_POINT,
    [SD_DIR_TIMELAPSE] = SD_MOUNT_POINT "/timelapse",
    [SD_DIR_AUDIO] = SD_MOUNT_POINT "/audio", 
    [SD_DIR_INIT] = SD_MOUNT_POINT "/init",
    [SD_DIR_STATS] = SD_MOUNT_POINT "/stats",
    [SD_DIR_LOGS] = SD_MOUNT_POINT "/logs",
    [SD_DIR_CONFIG] = SD_MOUNT_POINT "/config"
};

// Internal function prototypes
static esp_err_t mount_sd_card(void);
static esp_err_t unmount_sd_card(void);
static esp_err_t update_stats(void);
static void calculate_transfer_speed(size_t bytes, uint64_t start_time, bool is_write);

esp_err_t sd_manager_init(void) {
    if (sd_mgr.initialized) {
        ESP_LOGW(TAG, "SD manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SD card manager");

    // Create mutex for thread safety
    sd_mgr.mutex = xSemaphoreCreateMutex();
    if (!sd_mgr.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Initialize stats
    memset(&sd_mgr.stats, 0, sizeof(sd_stats_t));

    // Configure SPI host
    sd_mgr.host = (sdmmc_host_t)SDSPI_HOST_DEFAULT();
    sd_mgr.host.slot = SPI2_HOST;

    // Configure device slot
    sd_mgr.slot_config = (sdspi_device_config_t)SDSPI_DEVICE_CONFIG_DEFAULT();
    sd_mgr.slot_config.gpio_cs = SD_PIN_CS;
    sd_mgr.slot_config.host_id = sd_mgr.host.slot;

    // Configure mount options
    sd_mgr.mount_config.format_if_mount_failed = true;
    sd_mgr.mount_config.max_files = SD_MAX_FILES;
    sd_mgr.mount_config.allocation_unit_size = SD_ALLOCATION_UNIT_SIZE;

    // Attempt to mount SD card
    esp_err_t ret = mount_sd_card();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card mount failed, continuing without storage");
        sd_mgr.stats.card_present = false;
        stats_engine_log_event("sd.mount_failed", 1);
    } else {
        sd_mgr.stats.card_present = true;
        stats_engine_log_event("sd.mounted", 1);
        
        // Create directory structure
        sd_manager_create_directories();
        
        // Update initial stats
        update_stats();
    }

    sd_mgr.initialized = true;
    ESP_LOGI(TAG, "SD manager initialized successfully");
    return ESP_OK;
}

esp_err_t sd_manager_deinit(void) {
    if (!sd_mgr.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing SD manager");

    // Sync filesystem
    sd_manager_sync();

    // Unmount SD card
    unmount_sd_card();

    // Cleanup mutex
    if (sd_mgr.mutex) {
        vSemaphoreDelete(sd_mgr.mutex);
        sd_mgr.mutex = NULL;
    }

    sd_mgr.initialized = false;
    return ESP_OK;
}

bool sd_manager_is_available(void) {
    return sd_mgr.initialized && sd_mgr.card_mounted && sd_mgr.stats.card_present;
}

esp_err_t sd_manager_get_stats(sd_stats_t* stats) {
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(sd_mgr.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    update_stats();
    memcpy(stats, &sd_mgr.stats, sizeof(sd_stats_t));

    xSemaphoreGive(sd_mgr.mutex);
    return ESP_OK;
}

esp_err_t sd_manager_create_directories(void) {
    if (!sd_manager_is_available()) {
        ESP_LOGW(TAG, "SD card not available, skipping directory creation");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Creating directory structure");

    for (int i = SD_DIR_TIMELAPSE; i <= SD_DIR_CONFIG; i++) {
        struct stat st = {0};
        if (stat(sd_directories[i], &st) == -1) {
            if (mkdir(sd_directories[i], 0755) != 0) {
                ESP_LOGW(TAG, "Failed to create directory: %s", sd_directories[i]);
            } else {
                ESP_LOGI(TAG, "Created directory: %s", sd_directories[i]);
            }
        }
    }

    return ESP_OK;
}

esp_err_t sd_manager_get_directory_path(sd_directory_t dir_type, char* path, size_t path_size) {
    if (!path || dir_type > SD_DIR_CONFIG) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(path, sd_directories[dir_type], path_size - 1);
    path[path_size - 1] = '\0';
    return ESP_OK;
}

esp_err_t sd_manager_write_file(const char* filename, const void* data, size_t size) {
    if (!filename || !data || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sd_manager_is_available()) {
        ESP_LOGW(TAG, "SD card not available, skipping write");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(sd_mgr.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    uint64_t start_time = esp_timer_get_time();
    FILE* file = fopen(filename, "wb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", filename);
        sd_mgr.stats.error_count++;
        xSemaphoreGive(sd_mgr.mutex);
        return ESP_ERR_INVALID_STATE;
    }

    size_t written = fwrite(data, 1, size, file);
    fclose(file);

    if (written != size) {
        ESP_LOGE(TAG, "Write size mismatch: expected %zu, wrote %zu", size, written);
        sd_mgr.stats.error_count++;
        xSemaphoreGive(sd_mgr.mutex);
        return ESP_ERR_INVALID_SIZE;
    }

    sd_mgr.stats.write_operations++;
    calculate_transfer_speed(size, start_time, true);

    xSemaphoreGive(sd_mgr.mutex);
    
    ESP_LOGD(TAG, "Wrote %zu bytes to %s", size, filename);
    return ESP_OK;
}

esp_err_t sd_manager_append_file(const char* filename, const void* data, size_t size) {
    if (!filename || !data || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sd_manager_is_available()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(sd_mgr.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    uint64_t start_time = esp_timer_get_time();
    FILE* file = fopen(filename, "ab");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for appending: %s", filename);
        sd_mgr.stats.error_count++;
        xSemaphoreGive(sd_mgr.mutex);
        return ESP_ERR_INVALID_STATE;
    }

    size_t written = fwrite(data, 1, size, file);
    fclose(file);

    if (written != size) {
        ESP_LOGE(TAG, "Append size mismatch: expected %zu, wrote %zu", size, written);
        sd_mgr.stats.error_count++;
        xSemaphoreGive(sd_mgr.mutex);
        return ESP_ERR_INVALID_SIZE;
    }

    sd_mgr.stats.write_operations++;
    calculate_transfer_speed(size, start_time, true);

    xSemaphoreGive(sd_mgr.mutex);
    return ESP_OK;
}

esp_err_t sd_manager_read_file(const char* filename, void* buffer, size_t buffer_size, size_t* bytes_read) {
    if (!filename || !buffer || !bytes_read) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sd_manager_is_available()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(sd_mgr.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    uint64_t start_time = esp_timer_get_time();
    FILE* file = fopen(filename, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", filename);
        sd_mgr.stats.error_count++;
        xSemaphoreGive(sd_mgr.mutex);
        return ESP_ERR_INVALID_STATE;
    }

    *bytes_read = fread(buffer, 1, buffer_size, file);
    fclose(file);

    sd_mgr.stats.read_operations++;
    calculate_transfer_speed(*bytes_read, start_time, false);

    xSemaphoreGive(sd_mgr.mutex);
    
    ESP_LOGD(TAG, "Read %zu bytes from %s", *bytes_read, filename);
    return ESP_OK;
}

bool sd_manager_file_exists(const char* filename) {
    if (!filename || !sd_manager_is_available()) {
        return false;
    }

    struct stat st;
    return (stat(filename, &st) == 0);
}

esp_err_t sd_manager_create_timestamped_filename(sd_directory_t dir_type, const char* prefix,
                                                const char* extension, char* buffer, size_t buffer_size) {
    if (!prefix || !extension || !buffer || dir_type > SD_DIR_CONFIG) {
        return ESP_ERR_INVALID_ARG;
    }

    time_t now;
    time(&now);
    
    snprintf(buffer, buffer_size, "%s/%s_%lld.%s", 
            sd_directories[dir_type], prefix, (long long)now, extension);
    
    return ESP_OK;
}

esp_err_t sd_manager_sync(void) {
    if (!sd_manager_is_available()) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Syncing filesystem");
    // Note: ESP-IDF VFS doesn't require explicit sync
    return ESP_OK;
}

esp_err_t sd_manager_get_free_space(uint64_t* free_bytes) {
    if (!free_bytes) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sd_manager_is_available()) {
        *free_bytes = 0;
        return ESP_ERR_INVALID_STATE;
    }

    uint64_t total_bytes, used_bytes;
    esp_err_t ret = esp_vfs_fat_info(SD_MOUNT_POINT, &total_bytes, &used_bytes);
    if (ret == ESP_OK) {
        *free_bytes = total_bytes - used_bytes;
        sd_mgr.stats.total_bytes = total_bytes;
        sd_mgr.stats.used_bytes = used_bytes;
        sd_mgr.stats.free_bytes = *free_bytes;
    }

    return ret;
}

// Internal functions
static esp_err_t mount_sd_card(void) {
    ESP_LOGI(TAG, "Mounting SD card");

    // Initialize SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(sd_mgr.host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Mount filesystem
    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &sd_mgr.host, &sd_mgr.slot_config, 
                                 &sd_mgr.mount_config, &sd_mgr.card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        spi_bus_free(sd_mgr.host.slot);
        return ret;
    }

    sd_mgr.card_mounted = true;
    
    // Print card info
    ESP_LOGI(TAG, "SD card mounted successfully");
    ESP_LOGI(TAG, "Card name: %s", sd_mgr.card->cid.name);
    ESP_LOGI(TAG, "Card type: %s", (sd_mgr.card->ocr & (1UL << 30)) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI(TAG, "Card speed: %s", (sd_mgr.card->csd.tr_speed > 25000000) ? "high speed" : "default speed");
    ESP_LOGI(TAG, "Card size: %lluMB", ((uint64_t) sd_mgr.card->csd.capacity) * sd_mgr.card->csd.sector_size / (1024 * 1024));

    sd_mgr.stats.card_size_mb = ((uint64_t) sd_mgr.card->csd.capacity) * sd_mgr.card->csd.sector_size / (1024 * 1024);

    return ESP_OK;
}

static esp_err_t unmount_sd_card(void) {
    if (!sd_mgr.card_mounted) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Unmounting SD card");
    
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd_mgr.card);
    if (ret == ESP_OK) {
        spi_bus_free(sd_mgr.host.slot);
        sd_mgr.card_mounted = false;
    }

    return ret;
}

static esp_err_t update_stats(void) {
    if (!sd_manager_is_available()) {
        return ESP_ERR_INVALID_STATE;
    }

    // Update filesystem stats
    uint64_t free_bytes;
    esp_err_t ret = sd_manager_get_free_space(&free_bytes);
    if (ret == ESP_OK) {
        stats_engine_log_gauge("sd.free_bytes", (float)sd_mgr.stats.free_bytes);
        stats_engine_log_gauge("sd.used_bytes", (float)sd_mgr.stats.used_bytes);
        stats_engine_log_gauge("sd.usage_percent", 
                             100.0f * sd_mgr.stats.used_bytes / sd_mgr.stats.total_bytes);
    }

    return ret;
}

static void calculate_transfer_speed(size_t bytes, uint64_t start_time, bool is_write) {
    uint64_t duration_us = esp_timer_get_time() - start_time;
    if (duration_us > 0) {
        float speed_kbps = (bytes * 1000.0f) / duration_us;
        
        if (is_write) {
            sd_mgr.stats.write_speed_kbps = speed_kbps;
            stats_engine_log_gauge("sd.write_speed_kbps", speed_kbps);
        } else {
            sd_mgr.stats.read_speed_kbps = speed_kbps;
            stats_engine_log_gauge("sd.read_speed_kbps", speed_kbps);
        }
    }
}