#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "time-v1-safe";

void app_main(void) {
    ESP_LOGI(TAG, "=== Time-V1 Safe Mode Starting ===");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "System initialized successfully!");
    
    while (1) {
        ESP_LOGI(TAG, "Running in safe mode...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}