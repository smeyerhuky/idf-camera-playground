#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "camera_module.h"
#include "sdcard_module.h"

static const char *TAG = "simple_camera";

#define SD_MISO_GPIO    8
#define SD_MOSI_GPIO    9
#define SD_SCLK_GPIO    7
#define SD_CS_GPIO      21

#define CAPTURE_INTERVAL_MS  5000
#define MAX_FILES           5

static void capture_task(void *pvParameters)
{
    int photo_index = 0;
    char filename[64];
    
    while (1) {
        ESP_LOGI(TAG, "Taking picture...");
        
        camera_fb_t *fb = camera_module_capture();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
            continue;
        }
        
        snprintf(filename, sizeof(filename), "photo_%04d.jpg", photo_index++);
        
        esp_err_t err = sdcard_module_save_jpeg(fb->buf, fb->len, filename);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save photo to SD card");
        } else {
            ESP_LOGI(TAG, "Photo saved: %s (size: %zu bytes)", filename, fb->len);
            
            uint64_t free_bytes, total_bytes;
            if (sdcard_module_get_free_space(&free_bytes, &total_bytes) == ESP_OK) {
                ESP_LOGI(TAG, "SD Card - Free: %llu MB / Total: %llu MB", 
                         free_bytes / (1024 * 1024), total_bytes / (1024 * 1024));
            }
        }
        
        camera_module_return_fb(fb);
        
        vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Simple Camera Application Starting...");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    camera_config_params_t camera_params = {
        .frame_size = FRAMESIZE_UXGA,
        .pixel_format = PIXFORMAT_JPEG,
        .jpeg_quality = 10,
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
    
    ESP_LOGI(TAG, "Starting capture task...");
    xTaskCreate(capture_task, "capture_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Camera system ready! Taking photos every %d seconds", CAPTURE_INTERVAL_MS / 1000);
}