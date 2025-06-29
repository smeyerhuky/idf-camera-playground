#include "sd_tester.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdkconfig.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

static const char *TAG = "sd_tester";

// SD manager structure
typedef struct {
    sdmmc_card_t* card;
    sdmmc_host_t host;
    sdspi_device_config_t slot_config;
    esp_vfs_fat_mount_config_t mount_config;
    sd_stats_t stats;
    bool initialized;
    bool card_mounted;
} sd_tester_t;

static sd_tester_t sd_tester = {0};

// Internal function prototypes
static esp_err_t mount_sd_card(void);
static esp_err_t unmount_sd_card(void);
static esp_err_t update_stats(void);

esp_err_t sd_tester_init(void) {
    if (sd_tester.initialized) {
        ESP_LOGW(TAG, "SD tester already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SD card tester");

    // Initialize stats
    memset(&sd_tester.stats, 0, sizeof(sd_stats_t));

    // Configure SPI host
    sd_tester.host = (sdmmc_host_t)SDSPI_HOST_DEFAULT();
    sd_tester.host.slot = SPI2_HOST;
    sd_tester.host.max_freq_khz = 10000;  // 10MHz - conservative for XIAO ESP32S3 Sense

    // Configure device slot
    sd_tester.slot_config = (sdspi_device_config_t)SDSPI_DEVICE_CONFIG_DEFAULT();
    sd_tester.slot_config.gpio_cs = SD_PIN_CS;
    sd_tester.slot_config.host_id = sd_tester.host.slot;

    // Configure mount options
    sd_tester.mount_config.format_if_mount_failed = true;
    sd_tester.mount_config.max_files = SD_MAX_FILES;
    sd_tester.mount_config.allocation_unit_size = SD_ALLOCATION_UNIT_SIZE;

    // Attempt to mount SD card
    esp_err_t ret = mount_sd_card();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
        sd_tester.stats.card_present = false;
    } else {
        sd_tester.stats.card_present = true;
        
        // Update initial stats
        update_stats();
    }

    sd_tester.initialized = true;
    sd_tester.stats.initialized = true;
    ESP_LOGI(TAG, "SD tester initialized successfully");
    return ESP_OK;
}

esp_err_t sd_tester_deinit(void) {
    if (!sd_tester.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing SD tester");

    // Unmount SD card
    unmount_sd_card();

    sd_tester.initialized = false;
    sd_tester.stats.initialized = false;
    return ESP_OK;
}

bool sd_tester_is_available(void) {
    return sd_tester.initialized && sd_tester.card_mounted && sd_tester.stats.card_present;
}

esp_err_t sd_tester_get_stats(sd_stats_t* stats) {
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    update_stats();
    memcpy(stats, &sd_tester.stats, sizeof(sd_stats_t));

    return ESP_OK;
}

esp_err_t sd_tester_write_stats_to_file(void) {
    if (!sd_tester_is_available()) {
        ESP_LOGW(TAG, "SD card not available, cannot write stats");
        return ESP_ERR_INVALID_STATE;
    }

    // Update stats before writing
    update_stats();

    // Check if there's enough free space (need at least 1KB for stats file)
    if (sd_tester.stats.free_bytes < 1024) {
        ESP_LOGW(TAG, "SD card is full! Free: %" PRIu64 " bytes. Cannot write stats file.", sd_tester.stats.free_bytes);
        ESP_LOGW(TAG, "Consider freeing up space or formatting the SD card.");
        return ESP_ERR_NO_MEM;
    }

    // Create filename with timestamp
    time_t now;
    time(&now);
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/sd_stats_%lld.txt", SD_MOUNT_POINT, (long long)now);

    FILE* file = fopen(filename, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to create stats file: %s", filename);
        ESP_LOGE(TAG, "This might be due to insufficient free space (%" PRIu64 " bytes available)", sd_tester.stats.free_bytes);
        return ESP_ERR_INVALID_STATE;
    }

    // Write stats to file
    fprintf(file, "SD Card Statistics\n");
    fprintf(file, "==================\n");
    fprintf(file, "Timestamp: %lld\n", (long long)now);
    fprintf(file, "Card Present: %s\n", sd_tester.stats.card_present ? "Yes" : "No");
    fprintf(file, "Initialized: %s\n", sd_tester.stats.initialized ? "Yes" : "No");
    fprintf(file, "Card Size (MB): %" PRIu32 "\n", sd_tester.stats.card_size_mb);
    fprintf(file, "Total Bytes: %" PRIu64 "\n", sd_tester.stats.total_bytes);
    fprintf(file, "Used Bytes: %" PRIu64 "\n", sd_tester.stats.used_bytes);
    fprintf(file, "Free Bytes: %" PRIu64 "\n", sd_tester.stats.free_bytes);
    
    if (sd_tester.stats.total_bytes > 0) {
        float usage_percent = (float)sd_tester.stats.used_bytes / sd_tester.stats.total_bytes * 100.0f;
        fprintf(file, "Usage Percentage: %.2f%%\n", usage_percent);
    }

    fclose(file);

    ESP_LOGI(TAG, "Stats written to file: %s", filename);
    return ESP_OK;
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

    esp_err_t ret = spi_bus_initialize(sd_tester.host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Mount filesystem
    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &sd_tester.host, &sd_tester.slot_config, 
                                 &sd_tester.mount_config, &sd_tester.card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        spi_bus_free(sd_tester.host.slot);
        return ret;
    }

    sd_tester.card_mounted = true;
    
    // Print card info
    ESP_LOGI(TAG, "SD card mounted successfully");
    ESP_LOGI(TAG, "Card name: %s", sd_tester.card->cid.name);
    ESP_LOGI(TAG, "Card type: %s", (sd_tester.card->ocr & (1UL << 30)) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI(TAG, "Card speed: %s", (sd_tester.card->csd.tr_speed > 25000000) ? "high speed" : "default speed");
    ESP_LOGI(TAG, "Card size: %" PRIu64 "MB", ((uint64_t) sd_tester.card->csd.capacity) * sd_tester.card->csd.sector_size / (1024 * 1024));

    sd_tester.stats.card_size_mb = ((uint64_t) sd_tester.card->csd.capacity) * sd_tester.card->csd.sector_size / (1024 * 1024);

    return ESP_OK;
}

static esp_err_t unmount_sd_card(void) {
    if (!sd_tester.card_mounted) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Unmounting SD card");
    
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd_tester.card);
    if (ret == ESP_OK) {
        spi_bus_free(sd_tester.host.slot);
        sd_tester.card_mounted = false;
    }

    return ret;
}

static esp_err_t update_stats(void) {
    if (!sd_tester_is_available()) {
        return ESP_ERR_INVALID_STATE;
    }

    // Try multiple methods to get filesystem stats
    esp_err_t ret = ESP_OK;
    
    // Method 1: Try statvfs (more standard POSIX approach)
    struct statvfs vfs_stat;
    if (statvfs(SD_MOUNT_POINT, &vfs_stat) == 0) {
        uint64_t total_blocks = vfs_stat.f_blocks;
        uint64_t free_blocks = vfs_stat.f_bavail;  // Available to non-root
        uint64_t block_size = vfs_stat.f_frsize;   // Fragment size
        
        sd_tester.stats.total_bytes = total_blocks * block_size;
        sd_tester.stats.free_bytes = free_blocks * block_size;
        sd_tester.stats.used_bytes = sd_tester.stats.total_bytes - sd_tester.stats.free_bytes;
        
        ESP_LOGI(TAG, "statvfs: blocks=%llu, free=%llu, size=%llu", 
                (unsigned long long)total_blocks, (unsigned long long)free_blocks, (unsigned long long)block_size);
        ESP_LOGI(TAG, "statvfs result - Total: %" PRIu64 " MB, Used: %" PRIu64 " MB, Free: %" PRIu64 " MB", 
                sd_tester.stats.total_bytes / (1024*1024), 
                sd_tester.stats.used_bytes / (1024*1024), 
                sd_tester.stats.free_bytes / (1024*1024));
    } else {
        ESP_LOGW(TAG, "statvfs failed, trying esp_vfs_fat_info");
        
        // Method 2: Fall back to ESP-IDF specific function
        uint64_t total_bytes, used_bytes;
        ret = esp_vfs_fat_info(SD_MOUNT_POINT, &total_bytes, &used_bytes);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "esp_vfs_fat_info - Total: %" PRIu64 ", Used: %" PRIu64, total_bytes, used_bytes);
            
            // Check for obviously wrong values that might indicate FAT reporting issues
            if (used_bytes > total_bytes) {
                ESP_LOGW(TAG, "Filesystem reporting error: used > total. Using card capacity instead.");
                // Fall back to using card capacity
                uint64_t card_total = (uint64_t)sd_tester.stats.card_size_mb * 1024 * 1024;
                sd_tester.stats.total_bytes = card_total;
                sd_tester.stats.used_bytes = 0; // We can't determine this reliably
                sd_tester.stats.free_bytes = card_total;
            } else {
                sd_tester.stats.total_bytes = total_bytes;
                sd_tester.stats.used_bytes = used_bytes;
                sd_tester.stats.free_bytes = total_bytes - used_bytes;
            }
        } else {
            ESP_LOGW(TAG, "esp_vfs_fat_info failed: %s", esp_err_to_name(ret));
        }
    }

    return ret;
}

esp_err_t sd_tester_list_files(void) {
    if (!sd_tester_is_available()) {
        ESP_LOGW(TAG, "SD card not available");
        return ESP_ERR_INVALID_STATE;
    }

    DIR *dir = opendir(SD_MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", SD_MOUNT_POINT);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Files on SD card:");
    ESP_LOGI(TAG, "==================");
    
    struct dirent *entry;
    int file_count = 0;
    int total_entries = 0;
    while ((entry = readdir(dir)) != NULL) {
        total_entries++;
        ESP_LOGD(TAG, "Found entry: '%s', type: %d", entry->d_name, entry->d_type);
        
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        if (entry->d_type == DT_REG) {  // Regular file
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, entry->d_name);
            
            struct stat file_stat;
            if (stat(full_path, &file_stat) == 0) {
                ESP_LOGI(TAG, "  %s (%ld bytes)", entry->d_name, file_stat.st_size);
                file_count++;
            } else {
                ESP_LOGW(TAG, "  %s (stat failed)", entry->d_name);
            }
        } else if (entry->d_type == DT_DIR) {
            ESP_LOGI(TAG, "  %s/ (directory)", entry->d_name);
        } else {
            ESP_LOGI(TAG, "  %s (type: %d)", entry->d_name, entry->d_type);
        }
    }
    
    closedir(dir);
    ESP_LOGI(TAG, "Total entries found: %d, Regular files: %d", total_entries, file_count);
    return ESP_OK;
}

esp_err_t sd_tester_cleanup_old_stats(int max_files) {
    if (!sd_tester_is_available()) {
        ESP_LOGW(TAG, "SD card not available");
        return ESP_ERR_INVALID_STATE;
    }

    DIR *dir = opendir(SD_MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", SD_MOUNT_POINT);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Cleaning up old stats files (keeping %d newest)", max_files);
    
    // Count stats files
    struct dirent *entry;
    int stats_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, "sd_stats_") != NULL) {
            stats_count++;
        }
    }
    
    rewinddir(dir);
    
    if (stats_count <= max_files) {
        ESP_LOGI(TAG, "Only %d stats files found, no cleanup needed", stats_count);
        closedir(dir);
        return ESP_OK;
    }
    
    // Delete oldest stats files
    int files_to_delete = stats_count - max_files;
    int deleted = 0;
    
    while ((entry = readdir(dir)) != NULL && deleted < files_to_delete) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, "sd_stats_") != NULL) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", SD_MOUNT_POINT, entry->d_name);
            
            if (unlink(full_path) == 0) {
                ESP_LOGI(TAG, "Deleted old stats file: %s", entry->d_name);
                deleted++;
            } else {
                ESP_LOGW(TAG, "Failed to delete: %s", entry->d_name);
            }
        }
    }
    
    closedir(dir);
    ESP_LOGI(TAG, "Cleanup complete: deleted %d files", deleted);
    return ESP_OK;
}