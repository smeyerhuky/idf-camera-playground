#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_common.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "driver/temperature_sensor.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

static const char *TAG = "hello_wifi";

#define WIFI_SSID "pianomaster"
#define WIFI_PASS "happyness"
#define WIFI_MAXIMUM_RETRY 5

#define MOUNT_POINT "/sdcard"

// LED GPIO definitions for XIAO ESP32S3 Sense
#define LED_TX_RED_GPIO    GPIO_NUM_21     // Red LED for TX activity
#define LED_RX_ORANGE_GPIO GPIO_NUM_47     // Orange LED for RX activity
#define LED_STATUS_GPIO    GPIO_NUM_48     // Status LED (built-in)

// Power monitoring definitions
// Note: XIAO ESP32S3 Sense has NO built-in battery voltage monitoring GPIO
// Only USB power detection and built-in charge LED are available

// LED PWM configuration
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_TX_CHANNEL         LEDC_CHANNEL_0
#define LEDC_RX_CHANNEL         LEDC_CHANNEL_1
#define LEDC_STATUS_CHANNEL     LEDC_CHANNEL_2
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY          (5000)

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static esp_ip4_addr_t s_ip_addr;
static httpd_handle_t server = NULL;
static bool led_tx_active = false;
static bool led_rx_active = false;
static TaskHandle_t led_task_handle = NULL;

// Power monitoring variables
static bool usb_powered = true;  // Assume USB powered initially

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT     BIT1

// LED control functions
static void init_leds(void) {
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t tx_channel = {
        .channel    = LEDC_TX_CHANNEL,
        .duty       = 0,
        .gpio_num   = LED_TX_RED_GPIO,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER,
        .flags.output_invert = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&tx_channel));

    ledc_channel_config_t rx_channel = {
        .channel    = LEDC_RX_CHANNEL,
        .duty       = 0,
        .gpio_num   = LED_RX_ORANGE_GPIO,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER,
        .flags.output_invert = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&rx_channel));

    ledc_channel_config_t status_channel = {
        .channel    = LEDC_STATUS_CHANNEL,
        .duty       = 0,
        .gpio_num   = LED_STATUS_GPIO,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER,
        .flags.output_invert = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&status_channel));

    ESP_LOGI(TAG, "LEDs initialized");
}

static void set_led_brightness(ledc_channel_t channel, uint32_t brightness) {
    uint32_t duty = (brightness * ((1 << LEDC_DUTY_RES) - 1)) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, channel));
}

static void led_tx_pulse(void) {
    led_tx_active = true;
}

static void led_rx_pulse(void) {
    led_rx_active = true;
}

// Power monitoring functions
static void init_power_monitoring(void) {
    // XIAO ESP32S3 Sense has built-in charging circuit but no software battery monitoring
    // The red charge LED indicates charging status visually
    ESP_LOGI(TAG, "Power monitoring: Visual charge LED only (no software battery reading)");
}

static void update_power_status(void) {
    // On XIAO ESP32S3 Sense, we can only detect if powered by USB vs battery
    // No actual battery voltage reading without external hardware modification
    usb_powered = true;  // Simplified - could detect USB VBUS if needed
}

static void led_task(void *pvParameters) {
    uint32_t tx_fade = 0;
    uint32_t rx_fade = 0;
    bool status_on = false;
    uint32_t status_counter = 0;
    
    while (1) {
        if (led_tx_active) {
            tx_fade = 100;
            led_tx_active = false;
        }
        if (tx_fade > 0) {
            set_led_brightness(LEDC_TX_CHANNEL, tx_fade);
            tx_fade = (tx_fade > 5) ? tx_fade - 5 : 0;
        }
        
        if (led_rx_active) {
            rx_fade = 100;
            led_rx_active = false;
        }
        if (rx_fade > 0) {
            set_led_brightness(LEDC_RX_CHANNEL, rx_fade);
            rx_fade = (rx_fade > 5) ? rx_fade - 5 : 0;
        }
        
        status_counter++;
        if (status_counter >= 100) {
            update_power_status();
            
            // Simple heartbeat status LED
            status_on = !status_on;
            set_led_brightness(LEDC_STATUS_CHANNEL, status_on ? 20 : 0);
            status_counter = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        led_tx_pulse(); // TX activity for connection attempt
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
            led_tx_pulse(); // TX activity for retry
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        s_ip_addr = event->ip_info.ip;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        led_rx_pulse(); // RX activity for IP received
    }
}

static void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

static void init_sdcard(void) {
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;

    ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 15,
        .miso_io_num = 2,
        .sclk_io_num = 14,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = 13;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s).", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "SD card initialized");

    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&s_ip_addr));
    
    FILE* f = fopen(MOUNT_POINT"/ip_address.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "IP Address: %s\n", ip_str);
    fprintf(f, "SSID: %s\n", WIFI_SSID);
    fprintf(f, "Power: %s\n", usb_powered ? "USB" : "Battery");
    fclose(f);
    ESP_LOGI(TAG, "IP address and power status written to SD card");
}

static esp_err_t stats_get_handler(httpd_req_t *req) {
    led_rx_pulse(); // RX activity for incoming request
    
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);
    
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
    
    float tsens_value;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &tsens_value));
    
    temperature_sensor_disable(temp_sensor);
    temperature_sensor_uninstall(temp_sensor);
    
    char resp_str[512];
    snprintf(resp_str, sizeof(resp_str),
             "{\"chip\":\"%s\",\"cores\":%d,\"revision\":%d,"
             "\"flash_size_mb\":%lu,\"heap_free\":%d,\"heap_total\":%d,"
             "\"psram_free\":%d,\"psram_total\":%d,"
             "\"temperature_c\":%.1f,\"uptime_ms\":%llu,"
             "\"usb_powered\":%s}",
             chip_info.model == CHIP_ESP32S3 ? "ESP32-S3" : "Unknown",
             chip_info.cores,
             chip_info.revision,
             flash_size / (1024 * 1024),
             (int)heap_info.total_free_bytes,
             (int)(heap_info.total_allocated_bytes + heap_info.total_free_bytes),
             (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (int)heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
             tsens_value,
             (unsigned long long)(esp_timer_get_time() / 1000),
             usb_powered ? "true" : "false");
    
    // Save stats to SD card
    FILE* f = fopen(MOUNT_POINT"/stats.json", "w");
    if (f != NULL) {
        fprintf(f, "%s\n", resp_str);
        fclose(f);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    
    led_tx_pulse(); // TX activity for response sent
    return ESP_OK;
}

static void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t stats_uri = {
            .uri       = "/stats",
            .method    = HTTP_GET,
            .handler   = stats_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &stats_uri);
        ESP_LOGI(TAG, "HTTP server started");
    } else {
        ESP_LOGE(TAG, "Error starting server!");
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== Hello WiFi Starting ===");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "System initialized successfully!");
    
    // Initialize LEDs and power monitoring
    init_leds();
    init_power_monitoring();
    
    // Start LED task
    xTaskCreate(led_task, "led_task", 2048, NULL, 5, &led_task_handle);
    
    wifi_init_sta();
    init_sdcard();
    start_webserver();
    
    while (1) {
        ESP_LOGI(TAG, "Hello WiFi! Server: http://" IPSTR "/stats | Power: %s", 
                 IP2STR(&s_ip_addr), usb_powered ? "USB" : "Battery");
        led_tx_pulse(); // Periodic TX indicator
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}