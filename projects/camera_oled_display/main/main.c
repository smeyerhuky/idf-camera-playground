#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_camera.h"
#include "ssd1306_driver.h"
#include "lvgl_port.h"
#include "camera_preview.h"

static const char *TAG = "camera_oled_display";

// XIAO ESP32S3 Sense camera pins
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    10
#define CAM_PIN_SIOD    40
#define CAM_PIN_SIOC    39
#define CAM_PIN_D7      48
#define CAM_PIN_D6      11
#define CAM_PIN_D5      12
#define CAM_PIN_D4      14
#define CAM_PIN_D3      16
#define CAM_PIN_D2      18
#define CAM_PIN_D1      17
#define CAM_PIN_D0      15
#define CAM_PIN_VSYNC   38
#define CAM_PIN_HREF    47
#define CAM_PIN_PCLK    13

// XIAO ESP32S3 I2C pins for Qwiic connector
#define I2C_SDA_PIN     5
#define I2C_SCL_PIN     6
#define I2C_PORT        0
#define I2C_FREQ_HZ     400000

static ssd1306_handle_t oled_handle = {0};
static camera_preview_t preview = {0};

static esp_err_t init_camera(void)
{
    camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_RGB565,
        .frame_size = FRAMESIZE_QQVGA,  // 160x120
        .jpeg_quality = 12,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };
    
    esp_err_t ret = esp_camera_init(&camera_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", ret);
        return ret;
    }
    
    // Get camera sensor and configure
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        // Flip horizontally and vertically for better orientation
        s->set_hmirror(s, 1);
        s->set_vflip(s, 1);
        
        // Adjust camera settings for OLED display
        s->set_brightness(s, 1);
        s->set_contrast(s, 1);
        s->set_saturation(s, 0);
        s->set_quality(s, 10);
        s->set_colorbar(s, 0);
        s->set_whitebal(s, 1);
        s->set_gain_ctrl(s, 1);
        s->set_exposure_ctrl(s, 1);
        s->set_aec2(s, 0);
        s->set_awb_gain(s, 1);
        s->set_agc_gain(s, 0);
        s->set_aec_value(s, 300);
        s->set_special_effect(s, 0);
        s->set_wb_mode(s, 0);
        s->set_ae_level(s, 0);
    }
    
    ESP_LOGI(TAG, "Camera initialized successfully");
    return ESP_OK;
}

static esp_err_t init_oled(void)
{
    ssd1306_config_t oled_config = {
        .i2c_port = I2C_PORT,
        .i2c_addr = SSD1306_I2C_ADDR_PRIMARY,
        .sda_pin = I2C_SDA_PIN,
        .scl_pin = I2C_SCL_PIN,
        .clk_speed = I2C_FREQ_HZ,
    };
    
    esp_err_t ret = ssd1306_init(&oled_handle, &oled_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed with error 0x%x", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "OLED initialized successfully");
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Camera OLED Display Starting ===");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize camera
    ret = init_camera();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize camera");
        return;
    }
    
    // Initialize OLED display
    ret = init_oled();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OLED");
        esp_camera_deinit();
        return;
    }
    
    // Initialize LVGL port
    ret = lvgl_port_init(&oled_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LVGL port");
        ssd1306_deinit(&oled_handle);
        esp_camera_deinit();
        return;
    }
    
    // Wait for LVGL to initialize
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Initialize camera preview
    lvgl_port_lock(-1);
    
    camera_preview_config_t preview_config = {
        .preview_width = 128,
        .preview_height = 64,
        .show_info = true,
        .refresh_rate_ms = 100,  // 10 FPS
    };
    
    ret = camera_preview_init(&preview, &preview_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize camera preview");
        lvgl_port_unlock();
        lvgl_port_deinit();
        ssd1306_deinit(&oled_handle);
        esp_camera_deinit();
        return;
    }
    
    // Start camera preview
    camera_preview_start(&preview);
    
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "System initialized successfully! Camera preview running on OLED display.");
    
    // Main loop - monitor system status
    while (1) {
        ESP_LOGI(TAG, "Camera OLED display running...");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
