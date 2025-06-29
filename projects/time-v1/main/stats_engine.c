#include "stats_engine.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sd_manager.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "stats_engine";

// Stats engine structure
typedef struct stats_engine_s {
    stats_entry_t* buffer;
    uint32_t buffer_size;
    uint32_t write_index;
    uint32_t read_index;
    uint32_t entry_count;
    SemaphoreHandle_t mutex;
    bool initialized;
} stats_engine_t;

static stats_engine_t stats_engine = {0};

// Internal function prototypes
static esp_err_t add_entry(const char* name, float value, stats_type_t type);

esp_err_t stats_engine_init(void) {
    if (stats_engine.initialized) {
        ESP_LOGW(TAG, "Stats engine already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing stats engine with buffer size: %d", STATS_MAX_BUFFER_SIZE);

    // Allocate buffer in PSRAM if available, otherwise in DRAM
    stats_engine.buffer = heap_caps_malloc(
        STATS_MAX_BUFFER_SIZE * sizeof(stats_entry_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    
    if (!stats_engine.buffer) {
        ESP_LOGW(TAG, "PSRAM allocation failed, trying DRAM");
        stats_engine.buffer = malloc(STATS_MAX_BUFFER_SIZE * sizeof(stats_entry_t));
    }

    if (!stats_engine.buffer) {
        ESP_LOGE(TAG, "Failed to allocate stats buffer");
        return ESP_ERR_NO_MEM;
    }

    // Initialize stats engine
    stats_engine.buffer_size = STATS_MAX_BUFFER_SIZE;
    stats_engine.write_index = 0;
    stats_engine.read_index = 0;
    stats_engine.entry_count = 0;

    // Create mutex for thread safety
    stats_engine.mutex = xSemaphoreCreateMutex();
    if (!stats_engine.mutex) {
        free(stats_engine.buffer);
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    stats_engine.initialized = true;

    // Log initialization
    stats_engine_log_event("stats.initialized", 1);
    stats_engine_log_gauge("stats.buffer_size", STATS_MAX_BUFFER_SIZE);

    ESP_LOGI(TAG, "Stats engine initialized successfully");
    return ESP_OK;
}

esp_err_t stats_engine_deinit(void) {
    if (!stats_engine.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing stats engine");

    // Flush remaining entries
    stats_engine_flush_to_storage();

    // Clean up resources
    if (stats_engine.mutex) {
        vSemaphoreDelete(stats_engine.mutex);
        stats_engine.mutex = NULL;
    }

    if (stats_engine.buffer) {
        free(stats_engine.buffer);
        stats_engine.buffer = NULL;
    }

    stats_engine.initialized = false;
    return ESP_OK;
}

static esp_err_t add_entry(const char* name, float value, stats_type_t type) {
    if (!stats_engine.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(stats_engine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Check if buffer is full
    if (stats_engine.entry_count >= stats_engine.buffer_size) {
        ESP_LOGW(TAG, "Stats buffer full, dropping oldest entry");
        // Move read index to make space
        stats_engine.read_index = (stats_engine.read_index + 1) % stats_engine.buffer_size;
        stats_engine.entry_count--;
    }

    // Add new entry
    stats_entry_t* entry = &stats_engine.buffer[stats_engine.write_index];
    entry->timestamp_us = esp_timer_get_time();
    strncpy(entry->name, name, STATS_METRIC_NAME_LEN - 1);
    entry->name[STATS_METRIC_NAME_LEN - 1] = '\0';
    entry->value = value;
    entry->type = type;
    entry->tags = 0; // TODO: implement tagging system

    stats_engine.write_index = (stats_engine.write_index + 1) % stats_engine.buffer_size;
    stats_engine.entry_count++;

    xSemaphoreGive(stats_engine.mutex);
    return ESP_OK;
}

esp_err_t stats_engine_log_counter(const char* name, float value) {
    return add_entry(name, value, STATS_TYPE_COUNTER);
}

esp_err_t stats_engine_log_gauge(const char* name, float value) {
    return add_entry(name, value, STATS_TYPE_GAUGE);
}

esp_err_t stats_engine_log_timing(const char* name, uint64_t duration_ms) {
    return add_entry(name, (float)duration_ms, STATS_TYPE_TIMING);
}

esp_err_t stats_engine_log_event(const char* name, uint32_t count) {
    return add_entry(name, (float)count, STATS_TYPE_EVENT);
}

esp_err_t stats_engine_log_power(float current_ma, float voltage_mv) {
    esp_err_t ret = ESP_OK;
    ret |= stats_engine_log_gauge("power.current_ma", current_ma);
    ret |= stats_engine_log_gauge("power.voltage_mv", voltage_mv);
    ret |= stats_engine_log_gauge("power.power_mw", current_ma * voltage_mv / 1000.0f);
    return ret;
}

esp_err_t stats_engine_log_memory(uint32_t heap_free, uint32_t heap_total) {
    esp_err_t ret = ESP_OK;
    ret |= stats_engine_log_gauge("memory.heap_free", (float)heap_free);
    ret |= stats_engine_log_gauge("memory.heap_total", (float)heap_total);
    ret |= stats_engine_log_gauge("memory.heap_usage_pct", 
           100.0f * (heap_total - heap_free) / heap_total);
    return ret;
}

esp_err_t stats_engine_flush_to_storage(void) {
    if (!stats_engine.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Flushing %lu stats entries to storage", stats_engine.entry_count);

    if (xSemaphoreTake(stats_engine.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take mutex for flush");
        return ESP_ERR_TIMEOUT;
    }

    // Open stats file for writing
    char filename[64];
    uint64_t timestamp = esp_timer_get_time() / 1000000;
    snprintf(filename, sizeof(filename), "/sdcard/stats/stats_%llu.bin", timestamp);

    FILE* file = fopen(filename, "wb");
    if (!file) {
        ESP_LOGW(TAG, "Failed to open stats file: %s", filename);
        xSemaphoreGive(stats_engine.mutex);
        return ESP_ERR_INVALID_STATE;
    }

    // Write header
    uint32_t version = 1;
    uint32_t entry_count = stats_engine.entry_count;
    fwrite(&version, sizeof(version), 1, file);
    fwrite(&entry_count, sizeof(entry_count), 1, file);

    // Write entries
    uint32_t written = 0;
    while (stats_engine.entry_count > 0) {
        stats_entry_t* entry = &stats_engine.buffer[stats_engine.read_index];
        
        if (fwrite(entry, sizeof(stats_entry_t), 1, file) != 1) {
            ESP_LOGW(TAG, "Failed to write stats entry");
            break;
        }

        stats_engine.read_index = (stats_engine.read_index + 1) % stats_engine.buffer_size;
        stats_engine.entry_count--;
        written++;
    }

    fclose(file);
    xSemaphoreGive(stats_engine.mutex);

    ESP_LOGI(TAG, "Flushed %lu stats entries to %s", written, filename);
    return ESP_OK;
}

esp_err_t stats_engine_get_buffer(uint8_t* buffer, size_t buffer_size, uint32_t* entry_count) {
    if (!stats_engine.initialized || !buffer || !entry_count) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(stats_engine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Calculate how many entries can fit in buffer
    size_t entry_size = sizeof(stats_entry_t);
    uint32_t max_entries = buffer_size / entry_size;
    uint32_t entries_to_copy = (stats_engine.entry_count < max_entries) ? 
                               stats_engine.entry_count : max_entries;

    // Copy entries
    uint32_t copied = 0;
    uint32_t read_idx = stats_engine.read_index;
    
    for (uint32_t i = 0; i < entries_to_copy; i++) {
        memcpy(buffer + (i * entry_size), 
               &stats_engine.buffer[read_idx], 
               entry_size);
        read_idx = (read_idx + 1) % stats_engine.buffer_size;
        copied++;
    }

    *entry_count = copied;
    xSemaphoreGive(stats_engine.mutex);

    return ESP_OK;
}

esp_err_t stats_engine_clear_buffer(void) {
    if (!stats_engine.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(stats_engine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    stats_engine.write_index = 0;
    stats_engine.read_index = 0;
    stats_engine.entry_count = 0;

    xSemaphoreGive(stats_engine.mutex);
    return ESP_OK;
}

uint32_t stats_engine_get_buffer_usage(void) {
    if (!stats_engine.initialized) {
        return 0;
    }

    uint32_t usage = 0;
    if (xSemaphoreTake(stats_engine.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        usage = stats_engine.entry_count;
        xSemaphoreGive(stats_engine.mutex);
    }

    return usage;
}

bool stats_engine_is_buffer_full(void) {
    return stats_engine_get_buffer_usage() >= stats_engine.buffer_size;
}