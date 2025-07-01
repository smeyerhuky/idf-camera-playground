#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "nvs_flash.h"

// Red Rocks components
#include "camera_module.h"
#include "sdcard_module.h"
#include "time_sync.h"
#include "qr_generator.h"
#include "waveshare_display.h"

static const char *TAG = "red_rocks_photobooth";

// Photo booth configuration
#define PHOTO_WIDTH         1600
#define PHOTO_HEIGHT        1200
#define PHOTO_QUALITY       8          // High quality JPEG
#define MAX_PHOTOS          200        // Internal storage limit
#define WIFI_SSID          "RedRocks_PhotoBooth"
#define WIFI_PASSWORD      "Concert2024"
#define HTTP_SERVER_PORT   80

// UI state machine
typedef enum {
    STATE_IDLE,
    STATE_CAPTURING,
    STATE_PROCESSING,
    STATE_DISPLAYING_QR,
    STATE_DOWNLOADED
} photobooth_state_t;

typedef struct {
    photobooth_state_t state;
    char current_photo_filename[64];
    qr_code_t current_qr;
    int photo_count;
    bool touch_pressed;
} photobooth_context_t;

static photobooth_context_t g_photobooth = {
    .state = STATE_IDLE,
    .photo_count = 0,
    .touch_pressed = false
};

static httpd_handle_t g_server = NULL;
static QueueHandle_t ui_queue = NULL;

// HTTP server for photo downloads
static esp_err_t photo_download_handler(httpd_req_t *req) {
    // Extract filename from URL path
    const char* uri = req->uri;
    const char* filename_start = strrchr(uri, '/');
    if (!filename_start) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    filename_start++; // Skip the '/'
    
    char filepath[128];
    snprintf(filepath, sizeof(filepath), "/photos/%s", filename_start);
    
    ESP_LOGI(TAG, "Serving photo: %s", filepath);
    
    // Read photo from storage and send to client
    size_t file_size = 0;
    uint8_t* file_data = sdcard_module_read_file(filepath, &file_size);
    
    if (file_data && file_size > 0) {
        // Set headers for photo download
        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_set_hdr(req, "Content-Disposition", 
                          "attachment; filename=\"red_rocks_memory.jpg\"");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
        
        // Send photo data
        esp_err_t ret = httpd_resp_send(req, (char*)file_data, file_size);
        
        free(file_data);
        
        ESP_LOGI(TAG, "Photo downloaded successfully: %s (%zu bytes)", filename_start, file_size);
        return ret;
    } else {
        ESP_LOGE(TAG, "Failed to read photo: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
}

static esp_err_t setup_http_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_open_sockets = 10;
    
    if (httpd_start(&g_server, &config) == ESP_OK) {
        // Register photo download handler
        httpd_uri_t photo_uri = {
            .uri = "/photo/*",
            .method = HTTP_GET,
            .handler = photo_download_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &photo_uri);
        
        ESP_LOGI(TAG, "HTTP server started on port %d", HTTP_SERVER_PORT);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return ESP_FAIL;
}

static esp_err_t setup_wifi_hotspot(void) {
    ESP_LOGI(TAG, "Setting up WiFi hotspot: %s", WIFI_SSID);
    
    // Initialize WiFi
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    
    // Configure AP mode
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(WIFI_SSID),
            .channel = 6,
            .max_connection = 10,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .beacon_interval = 100,
        },
    };
    
    strcpy((char*)wifi_config.ap.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.ap.password, WIFI_PASSWORD);
    
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
    
    ESP_LOGI(TAG, "WiFi hotspot '%s' started", WIFI_SSID);
    return ESP_OK;
}

static esp_err_t take_photo(void) {
    ESP_LOGI(TAG, "Taking photo...");
    
    // Generate unique filename with timestamp
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    snprintf(g_photobooth.current_photo_filename, 
             sizeof(g_photobooth.current_photo_filename),
             "redrock_%04d%02d%02d_%02d%02d%02d_%03d.jpg",
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
             g_photobooth.photo_count++);
    
    // Capture high-quality photo
    camera_fb_t* fb = camera_module_capture();
    if (!fb) {
        ESP_LOGE(TAG, "Failed to capture photo");
        return ESP_FAIL;
    }
    
    // Save to storage
    char filepath[128];
    snprintf(filepath, sizeof(filepath), "/photos/%s", g_photobooth.current_photo_filename);
    
    esp_err_t ret = sdcard_module_write_file(filepath, fb->buf, fb->len);
    
    // Return frame buffer
    esp_camera_fb_return(fb);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Photo saved: %s (%zu bytes)", filepath, fb->len);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to save photo: %s", filepath);
        return ESP_FAIL;
    }
}

static esp_err_t generate_qr_for_photo(void) {
    ESP_LOGI(TAG, "Generating QR code for photo: %s", g_photobooth.current_photo_filename);
    
    esp_err_t ret = qr_generator_create_photo_qr(g_photobooth.current_photo_filename, 
                                                &g_photobooth.current_qr);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "QR code generated: %s", g_photobooth.current_qr.url);
    }
    
    return ret;
}

static void update_display(void) {
    switch (g_photobooth.state) {
        case STATE_IDLE:
            waveshare_display_fill(COLOR_BLACK);
            waveshare_display_draw_header();
            waveshare_display_draw_capture_button(g_photobooth.touch_pressed);
            waveshare_display_draw_qr_area(NULL);
            waveshare_display_draw_footer("Touch to take your Red Rocks photo!");
            break;
            
        case STATE_CAPTURING:
            waveshare_display_fill(COLOR_BLACK);
            waveshare_display_draw_header();
            waveshare_display_draw_capture_button(true);
            waveshare_display_draw_qr_area(NULL);
            waveshare_display_draw_footer("Smile! Taking your photo...");
            break;
            
        case STATE_PROCESSING:
            waveshare_display_fill(COLOR_BLACK);
            waveshare_display_draw_header();
            waveshare_display_draw_capture_button(false);
            waveshare_display_draw_qr_area(NULL);
            waveshare_display_draw_footer("Creating your QR code...");
            break;
            
        case STATE_DISPLAYING_QR:
            waveshare_display_fill(COLOR_BLACK);
            waveshare_display_draw_header();
            waveshare_display_draw_capture_button(false);
            
            // Get QR display buffer
            uint8_t qr_buffer[1800]; // 120x120 bits = 1800 bytes
            if (qr_generator_get_display_buffer(&g_photobooth.current_qr, 
                                               qr_buffer, sizeof(qr_buffer)) == ESP_OK) {
                waveshare_display_draw_qr_area(qr_buffer);
            } else {
                waveshare_display_draw_qr_area(NULL);
            }
            
            waveshare_display_draw_footer("Scan QR code to download!");
            break;
            
        case STATE_DOWNLOADED:
            waveshare_display_fill(COLOR_BLACK);
            waveshare_display_draw_header();
            waveshare_display_draw_capture_button(g_photobooth.touch_pressed);
            waveshare_display_draw_qr_area(NULL);
            waveshare_display_draw_footer("Photo downloaded! Touch for next person");
            break;
    }
}

static void ui_task(void* pvParameters) {
    TickType_t last_update = xTaskGetTickCount();
    touch_data_t touch_data;
    
    // UI button bounds
    display_rect_t capture_button = {
        .x = 20,
        .y = UI_HEADER_HEIGHT + 10,
        .width = DISPLAY_WIDTH - 40,
        .height = UI_BUTTON_HEIGHT
    };
    
    while (1) {
        // Read touch input
        if (waveshare_display_read_touch(&touch_data) == ESP_OK && touch_data.is_valid) {
            bool in_button = waveshare_display_point_in_rect(&touch_data.point, &capture_button);
            
            if (touch_data.event == TOUCH_EVENT_PRESS && in_button) {
                g_photobooth.touch_pressed = true;
                
                // Handle state transitions
                if (g_photobooth.state == STATE_IDLE || g_photobooth.state == STATE_DOWNLOADED) {
                    // Start photo capture process
                    g_photobooth.state = STATE_CAPTURING;
                    
                    // Queue photo capture task
                    int photo_task_signal = 1;
                    xQueueSend(ui_queue, &photo_task_signal, 0);
                }
            } else if (touch_data.event == TOUCH_EVENT_RELEASE) {
                g_photobooth.touch_pressed = false;
            }
        }
        
        // Update display at 10 FPS
        TickType_t current_time = xTaskGetTickCount();
        if (current_time - last_update >= pdMS_TO_TICKS(100)) {
            update_display();
            last_update = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // 20 FPS touch polling
    }
}

static void photo_capture_task(void* pvParameters) {
    int signal;
    
    while (1) {
        // Wait for photo capture signal
        if (xQueueReceive(ui_queue, &signal, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Photo capture requested");
            
            // Take photo
            if (take_photo() == ESP_OK) {
                g_photobooth.state = STATE_PROCESSING;
                vTaskDelay(pdMS_TO_TICKS(500)); // Brief processing delay
                
                // Generate QR code
                if (generate_qr_for_photo() == ESP_OK) {
                    g_photobooth.state = STATE_DISPLAYING_QR;
                    
                    // Auto-return to idle after 30 seconds
                    vTaskDelay(pdMS_TO_TICKS(30000));
                    
                    // Clean up QR code
                    qr_generator_free(&g_photobooth.current_qr);
                    g_photobooth.state = STATE_IDLE;
                } else {
                    ESP_LOGE(TAG, "Failed to generate QR code");
                    g_photobooth.state = STATE_IDLE;
                }
            } else {
                ESP_LOGE(TAG, "Failed to take photo");
                g_photobooth.state = STATE_IDLE;
            }
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Red Rocks Photo Booth starting...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create UI queue
    ui_queue = xQueueCreate(5, sizeof(int));
    if (!ui_queue) {
        ESP_LOGE(TAG, "Failed to create UI queue");
        return;
    }
    
    // Initialize display first
    ESP_LOGI(TAG, "Initializing display...");
    if (waveshare_display_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }
    
    // Show startup screen
    waveshare_display_fill(COLOR_BLACK);
    waveshare_display_draw_header();
    waveshare_display_draw_footer("Initializing Red Rocks Photo Booth...");
    
    // Initialize camera with wide-angle settings
    ESP_LOGI(TAG, "Initializing camera...");
    if (camera_module_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize camera");
        waveshare_display_draw_footer("ERROR: Camera failed to initialize");
        return;
    }
    
    // Configure camera for Red Rocks concerts
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, FRAMESIZE_UXGA);      // 1600x1200
        s->set_quality(s, PHOTO_QUALITY);        // High quality
        s->set_brightness(s, 1);                 // Slightly bright for outdoor
        s->set_contrast(s, 1);                   // Enhanced contrast
        s->set_saturation(s, 0);                 // Natural colors
        s->set_whitebal(s, 1);                   // Auto white balance
        s->set_awb_gain(s, 1);                   // Auto white balance gain
        ESP_LOGI(TAG, "Camera configured for Red Rocks concerts");
    }
    
    // Initialize storage (internal flash for photos)
    ESP_LOGI(TAG, "Initializing storage...");
    if (sdcard_module_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize storage");
        waveshare_display_draw_footer("ERROR: Storage failed to initialize");
        return;
    }
    
    // Create photos directory
    sdcard_module_create_directory("/photos");
    
    // Initialize QR generator
    ESP_LOGI(TAG, "Initializing QR generator...");
    if (qr_generator_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize QR generator");
        waveshare_display_draw_footer("ERROR: QR generator failed");
        return;
    }
    
    // Set up WiFi hotspot
    ESP_LOGI(TAG, "Setting up WiFi hotspot...");
    if (setup_wifi_hotspot() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup WiFi hotspot");
        waveshare_display_draw_footer("ERROR: WiFi setup failed");
        return;
    }
    
    // Start HTTP server for photo downloads
    ESP_LOGI(TAG, "Starting HTTP server...");
    if (setup_http_server() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        waveshare_display_draw_footer("ERROR: Web server failed");
        return;
    }
    
    // Initialize complete - show ready screen
    g_photobooth.state = STATE_IDLE;
    ESP_LOGI(TAG, "Red Rocks Photo Booth ready!");
    
    // Start UI and photo capture tasks
    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);
    xTaskCreate(photo_capture_task, "photo_capture_task", 8192, NULL, 4, NULL);
    
    // Main loop - monitor system health
    while (1) {
        ESP_LOGI(TAG, "Photo booth running - State: %d, Photos taken: %d", 
                g_photobooth.state, g_photobooth.photo_count);
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Status update every 10 seconds
    }
}