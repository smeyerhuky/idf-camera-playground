#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/rtc_io.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc.h"

#include "stats_engine.h"
#include "power_manager.h"
#include "camera_manager.h"
#include "audio_capture.h"
#include "sd_manager.h"
#include "wifi_manager.h"
#include "esp_now_comm.h"

static const char *TAG = "time-v1";

// Event group for system coordination
static EventGroupHandle_t system_events;
#define STATS_READY_BIT     BIT0
#define WIFI_READY_BIT      BIT1
#define SD_READY_BIT        BIT2
#define CAMERA_READY_BIT    BIT3
#define AUDIO_READY_BIT     BIT4
#define ESP_NOW_READY_BIT   BIT5

// System configuration
typedef struct {
    bool first_boot;
    uint32_t boot_count;
    uint64_t last_capture_time;
    uint32_t capture_interval;
} system_config_t;

// RTC memory for persistent data across deep sleep
RTC_DATA_ATTR system_config_t system_config = {
    .first_boot = true,
    .boot_count = 0,
    .last_capture_time = 0,
    .capture_interval = 300  // Default 5 minutes
};

// Function prototypes
static void system_init(void);
static void system_shutdown_prepare(void);
static void capture_timelapse(void);
static void enter_deep_sleep(void);
static bool should_capture_now(void);

void app_main(void) {
    ESP_LOGI(TAG, "=== Time-V1 Timelapse System Starting ===");
    ESP_LOGI(TAG, "Boot count: %lu", system_config.boot_count++);
    
    // Initialize system
    system_init();
    
    // Check if it's time to capture
    if (should_capture_now()) {
        ESP_LOGI(TAG, "Starting timelapse capture sequence");
        capture_timelapse();
    } else {
        ESP_LOGI(TAG, "Not time for capture, but staying awake for debugging");
    }
    
    // DEBUG MODE: Stay awake instead of deep sleep
    ESP_LOGI(TAG, "DEBUG MODE: Staying awake, sleeping for 10 seconds then repeating");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10 seconds
        ESP_LOGI(TAG, "DEBUG: Still alive, boot count: %lu", system_config.boot_count);
        
        // Run capture sequence every minute instead of deep sleep
        capture_timelapse();
    }
}

static void system_init(void) {
    // Create event group for system coordination
    system_events = xEventGroupCreate();
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize components in order of importance
    ESP_LOGI(TAG, "Initializing stats engine...");
    ESP_ERROR_CHECK(stats_engine_init());
    xEventGroupSetBits(system_events, STATS_READY_BIT);
    
    // Log boot event
    stats_engine_log_event("system.boot", system_config.boot_count);
    
    ESP_LOGI(TAG, "Initializing power manager...");
    ESP_ERROR_CHECK(power_manager_init());
    
    ESP_LOGI(TAG, "Initializing SD card manager...");
    if (sd_manager_init() == ESP_OK) {
        xEventGroupSetBits(system_events, SD_READY_BIT);
        stats_engine_log_event("storage.ready", 1);
    } else {
        ESP_LOGW(TAG, "SD card initialization failed, continuing without storage");
        stats_engine_log_event("storage.failed", 1);
    }
    
    // Initialize WiFi only on first boot or for time sync
    if (system_config.first_boot || (system_config.boot_count % 10 == 0)) {
        ESP_LOGI(TAG, "Initializing WiFi for time sync...");
        if (wifi_manager_init() == ESP_OK) {
            xEventGroupSetBits(system_events, WIFI_READY_BIT);
            stats_engine_log_event("wifi.connected", 1);
        } else {
            ESP_LOGW(TAG, "WiFi initialization failed");
            stats_engine_log_event("wifi.failed", 1);
        }
    }
    
    ESP_LOGI(TAG, "Initializing camera manager...");
    if (camera_manager_init() == ESP_OK) {
        xEventGroupSetBits(system_events, CAMERA_READY_BIT);
        stats_engine_log_event("camera.ready", 1);
    } else {
        ESP_LOGE(TAG, "Camera initialization failed!");
        stats_engine_log_event("camera.failed", 1);
        // Cannot continue without camera
        esp_restart();
    }
    
    ESP_LOGI(TAG, "Initializing audio capture...");
    if (audio_capture_init() == ESP_OK) {
        xEventGroupSetBits(system_events, AUDIO_READY_BIT);
        stats_engine_log_event("audio.ready", 1);
    } else {
        ESP_LOGW(TAG, "Audio initialization failed, continuing without audio");
        stats_engine_log_event("audio.failed", 1);
    }
    
    ESP_LOGI(TAG, "Initializing ESP-NOW communication...");
    if (esp_now_comm_init() == ESP_OK) {
        xEventGroupSetBits(system_events, ESP_NOW_READY_BIT);
        stats_engine_log_event("esp_now.ready", 1);
    } else {
        ESP_LOGW(TAG, "ESP-NOW initialization failed");
        stats_engine_log_event("esp_now.failed", 1);
    }
    
    // Take initial snapshot if first boot
    if (system_config.first_boot) {
        ESP_LOGI(TAG, "First boot - taking initial snapshot");
        camera_manager_take_snapshot("/sdcard/init");
        system_config.first_boot = false;
    }
    
    ESP_LOGI(TAG, "System initialization complete");
}

static bool should_capture_now(void) {
    uint64_t current_time = esp_timer_get_time() / 1000000; // Convert to seconds
    
    // Always capture on first boot
    if (system_config.boot_count == 1) {
        return true;
    }
    
    // Check if interval has elapsed
    if ((current_time - system_config.last_capture_time) >= system_config.capture_interval) {
        return true;
    }
    
    return false;
}

static void capture_timelapse(void) {
    ESP_LOGI(TAG, "Starting timelapse capture");
    uint64_t start_time = esp_timer_get_time();
    
    // Update last capture time
    system_config.last_capture_time = esp_timer_get_time() / 1000000;
    
    // Start audio capture
    EventBits_t bits = xEventGroupGetBits(system_events);
    if (bits & AUDIO_READY_BIT) {
        ESP_LOGI(TAG, "Starting audio capture");
        audio_capture_start(5);  // Default 5 seconds
    }
    
    // Capture video
    if (bits & CAMERA_READY_BIT) {
        ESP_LOGI(TAG, "Starting video capture");
        char filename[64];
        snprintf(filename, sizeof(filename), "/sdcard/timelapse/%llu.mjpeg", 
                system_config.last_capture_time);
        
        esp_err_t ret = camera_manager_record_video(filename, 5);  // Default 5 seconds
        if (ret == ESP_OK) {
            stats_engine_log_event("capture.success", 1);
        } else {
            stats_engine_log_event("capture.failed", 1);
        }
    }
    
    // Stop audio capture and save
    if (bits & AUDIO_READY_BIT) {
        char audio_filename[64];
        snprintf(audio_filename, sizeof(audio_filename), "/sdcard/audio/%llu.wav", 
                system_config.last_capture_time);
        audio_capture_stop_and_save(audio_filename);
    }
    
    // Calculate capture duration
    uint64_t duration_ms = (esp_timer_get_time() - start_time) / 1000;
    stats_engine_log_timing("capture.duration_ms", duration_ms);
    
    // Transmit metrics via ESP-NOW
    if (bits & ESP_NOW_READY_BIT) {
        ESP_LOGI(TAG, "Transmitting metrics via ESP-NOW");
        esp_now_comm_send_metrics();
    }
    
    ESP_LOGI(TAG, "Timelapse capture complete in %llu ms", duration_ms);
}

static void system_shutdown_prepare(void) {
    ESP_LOGI(TAG, "Preparing for deep sleep");
    
    // Flush stats to SD card
    stats_engine_flush_to_storage();
    
    // Shutdown components in reverse order
    esp_now_comm_deinit();
    audio_capture_deinit();
    camera_manager_deinit();
    wifi_manager_deinit();
    sd_manager_deinit();
    
    // Log sleep event
    stats_engine_log_event("system.sleep", 1);
    stats_engine_deinit();
    
    ESP_LOGI(TAG, "System shutdown complete");
}

static void enter_deep_sleep(void) {
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", system_config.capture_interval);
    
    // Configure wake up timer
    esp_sleep_enable_timer_wakeup(system_config.capture_interval * 1000000ULL);
    
    // Enable wake up from external sources if needed
    // esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
    
    // Enter deep sleep (let ESP-IDF handle power domain configuration automatically)
    esp_deep_sleep_start();
}