#include "camera_manager.h"
#include "esp_log.h"

static const char *TAG = "camera_manager";

esp_err_t camera_manager_init(void) {
    ESP_LOGI(TAG, "Camera manager initialized (stub)");
    return ESP_OK;
}

esp_err_t camera_manager_deinit(void) {
    ESP_LOGI(TAG, "Camera manager deinitialized (stub)");
    return ESP_OK;
}

esp_err_t camera_manager_take_snapshot(const char* directory) {
    ESP_LOGI(TAG, "Taking snapshot to %s (stub)", directory);
    return ESP_OK;
}

esp_err_t camera_manager_record_video(const char* filename, uint32_t duration_sec) {
    ESP_LOGI(TAG, "Recording video to %s for %lu seconds (stub)", filename, duration_sec);
    return ESP_OK;
}