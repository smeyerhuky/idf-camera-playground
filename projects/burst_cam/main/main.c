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
#include "audio_recorder.h"
#include "wifi_config.h"

static const char *TAG = "timelapse_camera";

#define SD_MISO_GPIO    8
#define SD_MOSI_GPIO    9
#define SD_SCLK_GPIO    7
#define SD_CS_GPIO      21

#define CAPTURE_INTERVAL_MS  10000
#define CAPTURE_DURATION_MS  3000
#define MAX_FILES           100
#define SYNC_TIME_HOURS     24

#define AUDIO_PDM_CLK_GPIO  42
#define AUDIO_PDM_DATA_GPIO 41
#define AUDIO_SAMPLE_RATE   16000
#define AUDIO_BUFFER_SIZE   1024

static void timelapse_capture_task(void *pvParameters)
{
    int video_index = 0;
    char filename[64];
    char date_path[64];
    char full_path[128];
    TickType_t last_sync_time = 0;
    
    while (1) {
        TickType_t current_time = xTaskGetTickCount();
        
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
        
        if (!time_sync_is_time_set()) {
            ESP_LOGW(TAG, "Time not set, capturing with sequential naming");
            snprintf(date_path, sizeof(date_path), "timelapse_data/no_time");
        } else {
            time_sync_get_date_path(date_path, sizeof(date_path));
        }
        
        esp_err_t ret = sdcard_module_create_dir(date_path);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to create directory: %s", date_path);
        }
        
        char current_time_str[64];
        if (time_sync_is_time_set()) {
            time_sync_format_timestamp(current_time_str, sizeof(current_time_str), "%Y-%m-%d %H:%M:%S");
            ESP_LOGI(TAG, "Starting 3-second timelapse capture with audio at %s...", current_time_str);
        } else {
            ESP_LOGI(TAG, "Starting 3-second timelapse capture with audio (no time sync)...");
        }
        
        char audio_filename[64];
        char audio_full_path[128];
        
        if (time_sync_is_time_set()) {
            char timestamp[32];
            time_sync_format_timestamp(timestamp, sizeof(timestamp), "%H%M%S");
            snprintf(audio_filename, sizeof(audio_filename), "audio_%s_%04d.wav", timestamp, video_index);
        } else {
            snprintf(audio_filename, sizeof(audio_filename), "audio_%04d.wav", video_index);
        }
        
        snprintf(audio_full_path, sizeof(audio_full_path), "%s/%s", date_path, audio_filename);
        
        esp_err_t audio_ret = audio_recorder_start_recording();
        if (audio_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start audio recording: %s", esp_err_to_name(audio_ret));
        }
        
        TickType_t capture_start = xTaskGetTickCount();
        int frame_count = 0;
        size_t total_audio_bytes = 0;
        int16_t *audio_buffer = malloc(AUDIO_BUFFER_SIZE);
        int16_t *audio_storage = malloc(AUDIO_SAMPLE_RATE * 2 * 4);
        size_t audio_storage_size = 0;
        
        if (!audio_buffer || !audio_storage) {
            ESP_LOGE(TAG, "Failed to allocate audio buffers");
        }
        
        while ((xTaskGetTickCount() - capture_start) < pdMS_TO_TICKS(CAPTURE_DURATION_MS)) {
            camera_fb_t *fb = camera_module_capture();
            if (!fb) {
                ESP_LOGE(TAG, "Camera capture failed");
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            
            if (frame_count == 0) {
                if (time_sync_is_time_set()) {
                    char timestamp[32];
                    time_sync_format_timestamp(timestamp, sizeof(timestamp), "%H%M%S");
                    snprintf(filename, sizeof(filename), "clip_%s_%04d.jpg", timestamp, video_index);
                } else {
                    snprintf(filename, sizeof(filename), "clip_%04d_frame_%03d.jpg", video_index, frame_count);
                }
                
                snprintf(full_path, sizeof(full_path), "%s/%s", date_path, filename);
                
                ret = sdcard_module_save_jpeg(fb->buf, fb->len, full_path);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "Saved frame: %s (size: %zu bytes)", full_path, fb->len);
                    
                    char relative_path[64];
                    if (time_sync_is_time_set()) {
                        time_sync_format_timestamp(relative_path, sizeof(relative_path), "%Y/%m/%d");
                    } else {
                        snprintf(relative_path, sizeof(relative_path), "no_time");
                    }
                    
                    manifest_add_video(relative_path, filename, fb->len, CAPTURE_DURATION_MS);
                    manifest_save_to_sd();
                    
                    uint64_t free_bytes, total_bytes;
                    if (sdcard_module_get_free_space(&free_bytes, &total_bytes) == ESP_OK) {
                        ESP_LOGI(TAG, "SD Card - Free: %llu MB / Total: %llu MB", 
                                 free_bytes / (1024 * 1024), total_bytes / (1024 * 1024));
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to save frame to SD card");
                }
            }
            
            camera_module_return_fb(fb);
            frame_count++;
            
            if (audio_buffer && audio_storage && audio_ret == ESP_OK) {
                size_t bytes_read = 0;
                esp_err_t read_ret = audio_recorder_read_samples(audio_buffer, AUDIO_BUFFER_SIZE, &bytes_read);
                if (read_ret == ESP_OK && bytes_read > 0) {
                    if (audio_storage_size + bytes_read < AUDIO_SAMPLE_RATE * 2 * 4) {
                        memcpy((uint8_t*)audio_storage + audio_storage_size, audio_buffer, bytes_read);
                        audio_storage_size += bytes_read;
                        total_audio_bytes += bytes_read;
                    }
                }
            }
            
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        audio_recorder_stop_recording();
        
        if (audio_storage && audio_storage_size > 0) {
            wav_header_t wav_header;
            audio_recorder_create_wav_header(&wav_header, AUDIO_SAMPLE_RATE, 1, audio_storage_size);
            
            esp_err_t wav_ret = sdcard_module_write_file(audio_full_path, &wav_header, sizeof(wav_header));
            if (wav_ret == ESP_OK) {
                wav_ret = sdcard_module_append_file(audio_full_path, audio_storage, audio_storage_size);
                if (wav_ret == ESP_OK) {
                    ESP_LOGI(TAG, "Saved audio file: %s (%zu bytes)", audio_full_path, audio_storage_size);
                } else {
                    ESP_LOGE(TAG, "Failed to append audio data");
                }
            } else {
                ESP_LOGE(TAG, "Failed to save audio file header");
            }
        }
        
        if (audio_buffer) {
            free(audio_buffer);
        }
        if (audio_storage) {
            free(audio_storage);
        }
        
        video_index++;
        ESP_LOGI(TAG, "Completed 3-second capture session with %d frames and %zu audio bytes", frame_count, total_audio_bytes);
        
        vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS - CAPTURE_DURATION_MS));
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
    
    ESP_LOGI(TAG, "Initializing audio recorder...");
    audio_config_t audio_config = {
        .pdm_clk_gpio = AUDIO_PDM_CLK_GPIO,
        .pdm_data_gpio = AUDIO_PDM_DATA_GPIO,
        .sample_rate = AUDIO_SAMPLE_RATE,
        .buffer_count = 4,
        .buffer_len = AUDIO_BUFFER_SIZE
    };
    
    ret = audio_recorder_init(&audio_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio recorder init failed: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "Initializing manifest manager...");
    ret = manifest_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Manifest init failed");
    }
    
    ESP_LOGI(TAG, "Starting timelapse capture task...");
    xTaskCreate(timelapse_capture_task, "timelapse_task", 8192, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Timelapse camera system ready!");
    ESP_LOGI(TAG, "Capturing 3-second clips every 10 seconds");
    ESP_LOGI(TAG, "Files organized in timelapse_data/YYYY/MM/DD/");
    ESP_LOGI(TAG, "Manifest available at timelapse_data/manifest.json");
}