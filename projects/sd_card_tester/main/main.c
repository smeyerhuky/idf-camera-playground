#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "sd_tester.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting SD Card Tester");
    
    // Enable debug logging for sd_tester
    esp_log_level_set("sd_tester", ESP_LOG_DEBUG);
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize SD card tester
    ret = sd_tester_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SD tester: %s", esp_err_to_name(ret));
    }

    // Check if SD card is available
    if (sd_tester_is_available()) {
        ESP_LOGI(TAG, "SD card is available");
        
        // Get and display stats
        sd_stats_t stats;
        ret = sd_tester_get_stats(&stats);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "SD Card Statistics:");
            ESP_LOGI(TAG, "  Card Present: %s", stats.card_present ? "Yes" : "No");
            ESP_LOGI(TAG, "  Initialized: %s", stats.initialized ? "Yes" : "No");
            ESP_LOGI(TAG, "  Card Size: %" PRIu32 " MB", stats.card_size_mb);
            ESP_LOGI(TAG, "  Total Bytes: %" PRIu64, stats.total_bytes);
            ESP_LOGI(TAG, "  Used Bytes: %" PRIu64, stats.used_bytes);
            ESP_LOGI(TAG, "  Free Bytes: %" PRIu64, stats.free_bytes);
            
            if (stats.total_bytes > 0) {
                float usage_percent = (float)stats.used_bytes / stats.total_bytes * 100.0f;
                ESP_LOGI(TAG, "  Usage: %.2f%%", usage_percent);
            }
        }
        
        // List current files on SD card
        ESP_LOGI(TAG, "Listing files on SD card...");
        sd_tester_list_files();
        
        // Try to write stats to file
        ret = sd_tester_write_stats_to_file();
        if (ret == ESP_ERR_NO_MEM) {
            ESP_LOGW(TAG, "SD card is full! Attempting to clean up old files...");
            ret = sd_tester_cleanup_old_stats(5); // Keep only 5 newest stats files
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Cleanup successful, retrying stats write...");
                ret = sd_tester_write_stats_to_file();
            }
        }
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Stats successfully written to SD card");
        } else {
            ESP_LOGE(TAG, "Failed to write stats to SD card: %s", esp_err_to_name(ret));
            if (ret == ESP_ERR_NO_MEM) {
                ESP_LOGE(TAG, "SD card is still full even after cleanup!");
                ESP_LOGE(TAG, "Consider manually freeing up space or formatting the card.");
            }
        }
        
    } else {
        ESP_LOGW(TAG, "SD card is not available");
    }

    // Keep the application running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // Wait 10 seconds
        
        // Periodically update and log stats
        if (sd_tester_is_available()) {
            sd_stats_t stats;
            if (sd_tester_get_stats(&stats) == ESP_OK) {
                ESP_LOGI(TAG, "Free space: %" PRIu64 " bytes (%.2f MB)", 
                        stats.free_bytes, (float)stats.free_bytes / (1024 * 1024));
            }
        }
    }
}