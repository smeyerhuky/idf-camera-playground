#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "oled_ui.h"
#include "input_handler.h"

static const char *TAG = "CAMERA_OLED_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Camera OLED UI Demo");
    
    ssd1306_config_t oled_config = {
        .sda_gpio = 5,   // GPIO 5 for SDA on XIAO ESP32S3
        .scl_gpio = 6,   // GPIO 6 for SCL on XIAO ESP32S3  
        .address = SSD1306_I2C_ADDRESS
    };
    
    oled_ui_handle_t *ui_handle;
    esp_err_t ret = oled_ui_init(&oled_config, &ui_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OLED UI: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "OLED UI initialized successfully");
    
    input_handler_config_t input_config = {
        .button_gpio = 0,     // Boot button on XIAO ESP32S3
        .encoder_a_gpio = 1,  // Optional encoder pin A
        .encoder_b_gpio = 2,  // Optional encoder pin B
        .debounce_ms = 50
    };
    
    ret = input_handler_init(&input_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize input handler: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "Input handler initialized");
    
    xTaskCreate(oled_ui_task, "oled_ui_task", 4096, NULL, 5, NULL);
    xTaskCreate(input_handler_task, "input_task", 2048, NULL, 4, NULL);
    
    ESP_LOGI(TAG, "All tasks started successfully");
    
    while (1) {
        camera_settings_t *settings = camera_control_ui_get_settings();
        ESP_LOGI(TAG, "Current settings - Mode: %d, Brightness: %d, Quality: %d, Mic: %s", 
                settings->mode, settings->brightness, settings->jpeg_quality,
                settings->microphone_enabled ? "ON" : "OFF");
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}