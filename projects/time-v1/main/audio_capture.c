#include "audio_capture.h"
#include "esp_log.h"

static const char *TAG = "audio_capture";

esp_err_t audio_capture_init(void) {
    ESP_LOGI(TAG, "Audio capture initialized (stub)");
    return ESP_OK;
}

esp_err_t audio_capture_deinit(void) {
    ESP_LOGI(TAG, "Audio capture deinitialized (stub)");
    return ESP_OK;
}

esp_err_t audio_capture_start(uint32_t duration_sec) {
    ESP_LOGI(TAG, "Starting audio capture for %lu seconds (stub)", duration_sec);
    return ESP_OK;
}

esp_err_t audio_capture_stop_and_save(const char* filename) {
    ESP_LOGI(TAG, "Stopping audio capture and saving to %s (stub)", filename);
    return ESP_OK;
}