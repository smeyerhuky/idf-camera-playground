#include "power_manager.h"
#include "esp_log.h"

static const char *TAG = "power_manager";

esp_err_t power_manager_init(void) {
    ESP_LOGI(TAG, "Power manager initialized (stub)");
    return ESP_OK;
}

esp_err_t power_manager_deinit(void) {
    ESP_LOGI(TAG, "Power manager deinitialized (stub)");
    return ESP_OK;
}