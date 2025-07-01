#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "camera_module.h"
#include "sdcard_module.h"
#include "time_sync.h"
#include "manifest_manager.h"
// #include "audio_recorder.h" - removed for video-only recording
#include "avi_recorder.h"
#include "wifi_config.h"

static const char *TAG = "timelapse_camera";

#define SD_MISO_GPIO    8
#define SD_MOSI_GPIO    9
#define SD_SCLK_GPIO    7
#define SD_CS_GPIO      21

#define VIDEO_DURATION_SEC   10
#define VIDEO_INTERVAL_SEC   90
#define MAX_FILES           100
#define SYNC_TIME_HOURS     24
#define VIDEO_FPS           10
#define VIDEO_WIDTH         640
#define VIDEO_HEIGHT        480

// Audio constants removed - using video-only recording

#define CAPTURE_DURATION_MS  (VIDEO_DURATION_SEC * 1000)
#define CAPTURE_INTERVAL_MS  (10 * 1000)

static void video_recording_task(void *pvParameters)
{
    int video_index = 0;
    char avi_filename[64];
    char date_path[64];
    char avi_full_path[128];
    TickType_t last_sync_time = 0;
    
    while (1) {
        TickType_t current_time = xTaskGetTickCount();
        
        // Periodic time resync
        if ((current_time - last_sync_time) > pdMS_TO_TICKS(SYNC_TIME_HOURS * 60 * 60 * 1000)) {
            ESP_LOGI(TAG, "Attempting time resync...");
            esp_err_t ret = time_sync_connect_wifi();
            if (ret == ESP_OK) {
                time_sync_sync_time();
                time_sync_disconnect_wifi();
                last_sync_time = current_time;
                ESP_LOGI(TAG, "Time resync completed");
            } else {
                ESP_LOGW(TAG, "Time resync failed, continuing with current time");
            }
        }
        
        // Create date-based directory structure
        if (!time_sync_is_time_set()) {
            ESP_LOGW(TAG, "Time not set, using sequential naming");
            snprintf(date_path, sizeof(date_path), "timelapse_data/no_time");
        } else {
            time_sync_get_date_path(date_path, sizeof(date_path));
        }
        
        esp_err_t ret = sdcard_module_create_dir(date_path);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to create directory: %s", date_path);
        }
        
        // Generate filename with timestamp
        if (time_sync_is_time_set()) {
            char timestamp[32];
            time_sync_format_timestamp(timestamp, sizeof(timestamp), "%H%M%S");
            snprintf(avi_filename, sizeof(avi_filename), "video_%s_%04d.avi", timestamp, video_index);
        } else {
            snprintf(avi_filename, sizeof(avi_filename), "video_%04d.avi", video_index);
        }
        
        snprintf(avi_full_path, sizeof(avi_full_path), "%s/%s", date_path, avi_filename);
        
        ESP_LOGI(TAG, "Starting %d-second video recording: %s", VIDEO_DURATION_SEC, avi_full_path);
        
        // Start AVI recording
        ret = avi_recorder_start(avi_full_path);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start AVI recording: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        // Recording loop
        TickType_t recording_start = xTaskGetTickCount();
        int frame_count = 0;
        
        while ((xTaskGetTickCount() - recording_start) < pdMS_TO_TICKS(VIDEO_DURATION_SEC * 1000)) {
            // Capture and add video frame
            camera_fb_t *fb = camera_module_capture();
            if (fb) {
                esp_err_t frame_ret = avi_recorder_add_frame(fb->buf, fb->len);
                if (frame_ret == ESP_OK) {
                    frame_count++;
                } else {
                    ESP_LOGW(TAG, "Failed to add frame to AVI");
                }
                camera_module_return_fb(fb);
            } else {
                ESP_LOGW(TAG, "Camera capture failed");
            }
            
            // Frame rate control
            vTaskDelay(pdMS_TO_TICKS(1000 / VIDEO_FPS));
        }
        
        // Stop recording
        avi_recorder_stop();
        
        // Add to manifest
        char relative_path[64];
        if (time_sync_is_time_set()) {
            time_sync_format_timestamp(relative_path, sizeof(relative_path), "%Y/%m/%d");
        } else {
            snprintf(relative_path, sizeof(relative_path), "no_time");
        }
        
        avi_recording_state_t *avi_state = avi_recorder_get_state();
        manifest_add_video(relative_path, avi_filename, avi_state->total_bytes, VIDEO_DURATION_SEC * 1000);
        manifest_save_to_sd();
        
        // Show storage info
        uint64_t free_bytes, total_bytes;
        if (sdcard_module_get_free_space(&free_bytes, &total_bytes) == ESP_OK) {
            ESP_LOGI(TAG, "SD Card - Free: %llu MB / Total: %llu MB", 
                     free_bytes / (1024 * 1024), total_bytes / (1024 * 1024));
        }
        
        video_index++;
        ESP_LOGI(TAG, "Completed video #%d: %d frames", video_index, frame_count);
        
        // Sleep for 250 seconds before next recording
        ESP_LOGI(TAG, "Sleeping for %d seconds before next recording...", VIDEO_INTERVAL_SEC);
        vTaskDelay(pdMS_TO_TICKS(VIDEO_INTERVAL_SEC * 1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Timelapse Camera Application Starting...");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "Initializing time sync...");
    time_sync_config_t time_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
        .ntp_server = NTP_SERVER,
        .gmt_offset_sec = GMT_OFFSET_SEC,
        .daylight_offset_sec = DAYLIGHT_OFFSET_SEC,
        .max_retry = WIFI_MAX_RETRY
    };
    
    ret = time_sync_init(&time_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Connecting to WiFi for initial time sync...");
        ret = time_sync_connect_wifi();
        if (ret == ESP_OK) {
            esp_err_t sync_ret = time_sync_sync_time();
            if (sync_ret == ESP_OK) {
                ESP_LOGI(TAG, "Initial time sync completed successfully");
            } else {
                ESP_LOGW(TAG, "Time sync failed, but WiFi connected");
            }
            time_sync_disconnect_wifi();
        } else {
            ESP_LOGW(TAG, "WiFi connection failed, continuing without time sync");
            ESP_LOGW(TAG, "Camera will use sequential numbering instead of timestamps");
        }
    } else {
        ESP_LOGE(TAG, "Time sync init failed, continuing without time sync");
    }
    
    camera_config_params_t camera_params = {
        .frame_size = FRAMESIZE_VGA,
        .pixel_format = PIXFORMAT_JPEG,
        .jpeg_quality = 12,
        .fb_count = 2
    };
    
    ESP_LOGI(TAG, "Initializing camera...");
    ret = camera_module_init(&camera_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed");
        return;
    }
    
    camera_module_set_brightness(0);
    camera_module_set_contrast(0);
    camera_module_set_saturation(0);
    
    sdcard_config_t sd_config = {
        .miso_gpio = SD_MISO_GPIO,
        .mosi_gpio = SD_MOSI_GPIO,
        .sclk_gpio = SD_SCLK_GPIO,
        .cs_gpio = SD_CS_GPIO,
        .max_files = MAX_FILES
    };
    
    ESP_LOGI(TAG, "Initializing SD card...");
    ret = sdcard_module_init(&sd_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card init failed");
        camera_module_deinit();
        return;
    }
    
    ESP_LOGI(TAG, "Creating timelapse_data directory...");
    sdcard_module_create_dir("timelapse_data");
    
    // Audio recording removed - using video-only recording
    
    ESP_LOGI(TAG, "Initializing manifest manager...");
    ret = manifest_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Manifest init failed");
    }
    
    ESP_LOGI(TAG, "Starting timelapse capture task...");
    ESP_LOGI(TAG, "Initializing AVI recorder...");
    ret = avi_recorder_init(VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AVI recorder init failed: %s", esp_err_to_name(ret));
    }
    
    xTaskCreate(video_recording_task, "video_task", 8192, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Video recording system ready!");
    ESP_LOGI(TAG, "Recording %d-second MJPEG AVI videos every %d seconds", VIDEO_DURATION_SEC, VIDEO_INTERVAL_SEC);
    ESP_LOGI(TAG, "Frame rate: %d FPS, Resolution: %dx%d", VIDEO_FPS, VIDEO_WIDTH, VIDEO_HEIGHT);
    ESP_LOGI(TAG, "Files organized in timelapse_data/YYYY/MM/DD/");
    ESP_LOGI(TAG, "Manifest available at timelapse_data/manifest.json");
}