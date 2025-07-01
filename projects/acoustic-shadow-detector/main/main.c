#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"

static const char *TAG = "acoustic-shadow-detector";

// I2S Configuration for INMP441 MEMS Microphone
#define I2S_SAMPLE_RATE     44100
#define I2S_NUM             I2S_NUM_0
#define I2S_BCK_PIN         26
#define I2S_WS_PIN          25
#define I2S_DATA_PIN        27
#define I2S_CHANNEL_NUM     1
#define I2S_BIT_WIDTH       32
#define DMA_BUF_COUNT       8
#define DMA_BUF_LEN         1024

// Audio Processing Parameters
#define SAMPLES_PER_FRAME   1024
#define CALIBRATION_OFFSET  94.0f  // Microphone sensitivity calibration
#define REF_PRESSURE        20e-6f // 20 µPa reference pressure
#define SHADOW_THRESHOLD    6.0f   // dB difference for shadow detection

// ESP-NOW Configuration
#define ESP_NOW_CHANNEL     1
#define MAX_NODES           20
#define BROADCAST_INTERVAL  100    // ms

// Node data structure
typedef struct {
    uint8_t mac_addr[6];
    float spl_value;
    int64_t timestamp;
    float x_pos;
    float y_pos;
} node_data_t;

// Audio packet for ESP-NOW
typedef struct __attribute__((packed)) {
    uint8_t msg_type;
    uint8_t node_id;
    float spl_value;
    float freq_bands[8];
    uint32_t timestamp;
} audio_packet_t;

// Global variables
static i2s_chan_handle_t rx_handle = NULL;
static QueueHandle_t audio_queue;
static node_data_t node_registry[MAX_NODES];
static int active_nodes = 0;
static float current_spl = 0.0f;
static uint8_t node_id;

// Function prototypes
static esp_err_t i2s_init(void);
static float calculate_spl(int32_t *samples, int num_samples);
static void audio_processing_task(void *pvParameters);
static void network_task(void *pvParameters);
static esp_err_t espnow_init(void);
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len);
static void detect_acoustic_shadows(void);

// Initialize I2S for INMP441 microphone
static esp_err_t i2s_init(void) {
    ESP_LOGI(TAG, "Initializing I2S for INMP441 microphone");
    
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));
    
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_BIT_WIDTH, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DATA_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    
    ESP_LOGI(TAG, "I2S initialized successfully");
    return ESP_OK;
}

// Calculate SPL from audio samples with calibration
static float calculate_spl(int32_t *samples, int num_samples) {
    double sum_squares = 0.0;
    
    // Calculate RMS value
    for (int i = 0; i < num_samples; i++) {
        // Convert 32-bit I2S data to normalized float
        // INMP441 outputs data in upper 18 bits
        double sample = (double)(samples[i] >> 14) / 131072.0;
        sum_squares += sample * sample;
    }
    
    double rms = sqrt(sum_squares / num_samples);
    
    // Prevent log of zero
    if (rms < 1e-10) rms = 1e-10;
    
    // Calculate SPL: 20 * log10(P/Pref) + calibration
    float spl = 20.0f * log10f(rms / REF_PRESSURE) + CALIBRATION_OFFSET;
    
    return spl;
}

// Audio processing task - runs on Core 1
static void audio_processing_task(void *pvParameters) {
    ESP_LOGI(TAG, "Audio processing task started on core %d", xPortGetCoreID());
    
    int32_t *samples = heap_caps_malloc(SAMPLES_PER_FRAME * sizeof(int32_t), MALLOC_CAP_DMA);
    if (!samples) {
        ESP_LOGE(TAG, "Failed to allocate sample buffer");
        vTaskDelete(NULL);
        return;
    }
    
    size_t bytes_read;
    audio_packet_t packet;
    
    while (1) {
        // Read audio data from I2S
        esp_err_t ret = i2s_channel_read(rx_handle, samples, 
                                         SAMPLES_PER_FRAME * sizeof(int32_t), 
                                         &bytes_read, portMAX_DELAY);
        
        if (ret == ESP_OK && bytes_read > 0) {
            int num_samples = bytes_read / sizeof(int32_t);
            
            // Calculate SPL
            current_spl = calculate_spl(samples, num_samples);
            
            // Prepare packet for broadcast
            packet.msg_type = 0x01;  // Audio data packet
            packet.node_id = node_id;
            packet.spl_value = current_spl;
            packet.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
            
            // Simple frequency band analysis (placeholder)
            for (int i = 0; i < 8; i++) {
                packet.freq_bands[i] = current_spl - (i * 2.0f);
            }
            
            // Send to network queue
            xQueueSend(audio_queue, &packet, 0);
            
            // Log SPL value
            if ((packet.timestamp % 1000) < BROADCAST_INTERVAL) {
                ESP_LOGI(TAG, "SPL: %.1f dB", current_spl);
            }
        }
        
        // Process at ~10Hz for real-time response
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    heap_caps_free(samples);
    vTaskDelete(NULL);
}

// Network task - handles ESP-NOW communication
static void network_task(void *pvParameters) {
    ESP_LOGI(TAG, "Network task started on core %d", xPortGetCoreID());
    
    audio_packet_t packet;
    
    while (1) {
        // Check for outgoing packets
        if (xQueueReceive(audio_queue, &packet, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Broadcast to all peers
            esp_err_t result = esp_now_send(NULL, (uint8_t *)&packet, sizeof(packet));
            if (result != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send ESP-NOW packet: %s", esp_err_to_name(result));
            }
        }
        
        // Periodically check for acoustic shadows
        static int64_t last_check = 0;
        int64_t now = esp_timer_get_time();
        if (now - last_check > 500000) {  // Every 500ms
            detect_acoustic_shadows();
            last_check = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ESP-NOW receive callback
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
    if (data_len != sizeof(audio_packet_t)) {
        return;
    }
    
    audio_packet_t *packet = (audio_packet_t *)data;
    
    // Update node registry
    bool node_found = false;
    for (int i = 0; i < active_nodes; i++) {
        if (memcmp(node_registry[i].mac_addr, recv_info->src_addr, 6) == 0) {
            node_registry[i].spl_value = packet->spl_value;
            node_registry[i].timestamp = esp_timer_get_time();
            node_found = true;
            break;
        }
    }
    
    // Add new node if not found
    if (!node_found && active_nodes < MAX_NODES) {
        memcpy(node_registry[active_nodes].mac_addr, recv_info->src_addr, 6);
        node_registry[active_nodes].spl_value = packet->spl_value;
        node_registry[active_nodes].timestamp = esp_timer_get_time();
        active_nodes++;
        
        ESP_LOGI(TAG, "New node registered: %02X:%02X:%02X:%02X:%02X:%02X",
                 recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
                 recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
    }
}

// Detect acoustic shadows based on SPL differences
static void detect_acoustic_shadows(void) {
    if (active_nodes < 2) return;
    
    int64_t now = esp_timer_get_time();
    float avg_spl = current_spl;
    int valid_nodes = 1;
    
    // Calculate average SPL from all active nodes
    for (int i = 0; i < active_nodes; i++) {
        // Only use recent data (within 1 second)
        if (now - node_registry[i].timestamp < 1000000) {
            avg_spl += node_registry[i].spl_value;
            valid_nodes++;
        }
    }
    
    if (valid_nodes > 0) {
        avg_spl /= valid_nodes;
    }
    
    // Check for shadow conditions
    bool in_shadow = false;
    
    // Local node shadow detection
    if (fabs(current_spl - avg_spl) > SHADOW_THRESHOLD) {
        in_shadow = true;
        ESP_LOGW(TAG, "Acoustic shadow detected! Local SPL: %.1f dB, Network avg: %.1f dB, Diff: %.1f dB",
                 current_spl, avg_spl, fabs(current_spl - avg_spl));
    }
    
    // Check other nodes for shadows
    for (int i = 0; i < active_nodes; i++) {
        if (now - node_registry[i].timestamp < 1000000) {
            float diff = fabs(node_registry[i].spl_value - avg_spl);
            if (diff > SHADOW_THRESHOLD) {
                ESP_LOGW(TAG, "Node %02X:%02X shadow detected! SPL: %.1f dB, Diff: %.1f dB",
                         node_registry[i].mac_addr[4], node_registry[i].mac_addr[5],
                         node_registry[i].spl_value, diff);
            }
        }
    }
}

// Initialize ESP-NOW
static esp_err_t espnow_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    
    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    
    // Add broadcast peer
    esp_now_peer_info_t peer = {
        .channel = ESP_NOW_CHANNEL,
        .encrypt = false,
    };
    memset(peer.peer_addr, 0xFF, 6);  // Broadcast address
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    
    // Generate node ID from MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    node_id = mac[5];  // Use last byte as simple node ID
    
    ESP_LOGI(TAG, "ESP-NOW initialized. Node ID: %02X", node_id);
    return ESP_OK;
}

void app_main(void) {
    ESP_LOGI(TAG, "=== Acoustic Shadow Detector Starting ===");
    ESP_LOGI(TAG, "Node ID will be assigned from MAC address");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create audio queue
    audio_queue = xQueueCreate(10, sizeof(audio_packet_t));
    if (!audio_queue) {
        ESP_LOGE(TAG, "Failed to create audio queue");
        return;
    }
    
    // Initialize components
    ESP_ERROR_CHECK(i2s_init());
    ESP_ERROR_CHECK(espnow_init());
    
    // Start tasks on specific cores
    xTaskCreatePinnedToCore(audio_processing_task, "audio_task", 8192, NULL, 
                            configMAX_PRIORITIES - 1, NULL, 1);
    xTaskCreatePinnedToCore(network_task, "network_task", 4096, NULL, 
                            configMAX_PRIORITIES - 2, NULL, 0);
    
    ESP_LOGI(TAG, "System initialized successfully!");
    ESP_LOGI(TAG, "Sampling rate: %d Hz", I2S_SAMPLE_RATE);
    ESP_LOGI(TAG, "Shadow threshold: %.1f dB", SHADOW_THRESHOLD);
    
    // Main loop - could add display updates here
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Active nodes: %d, Current SPL: %.1f dB", active_nodes, current_spl);
    }
}