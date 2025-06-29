#include "esp_now_comm.h"
#include "esp_log.h"

static const char *TAG = "esp_now_comm";

esp_err_t esp_now_comm_init(void) {
    ESP_LOGI(TAG, "ESP-NOW communication initialized (stub)");
    return ESP_OK;
}

esp_err_t esp_now_comm_deinit(void) {
    ESP_LOGI(TAG, "ESP-NOW communication deinitialized (stub)");
    return ESP_OK;
}

esp_err_t esp_now_comm_send_metrics(void) {
    ESP_LOGI(TAG, "Sending metrics via ESP-NOW (stub)");
    return ESP_OK;
}