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
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/i2s_std.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_netif.h"

static const char *TAG = "atmospheric-bass-barometer";

// I2C Configuration
#define I2C_MASTER_SCL_IO          22
#define I2C_MASTER_SDA_IO          21
#define I2C_MASTER_NUM             I2C_NUM_0
#define I2C_MASTER_FREQ_HZ         400000
#define I2C_MASTER_TX_BUF_DISABLE  0
#define I2C_MASTER_RX_BUF_DISABLE  0

// Sensor I2C Addresses
#define BMP390_I2C_ADDR            0x77
#define MS5611_I2C_ADDR            0x77  // Alternative address for redundancy
#define DHT22_GPIO                 4     // DHT22 uses single-wire protocol

// UART Configuration for GPS
#define GPS_UART_NUM               UART_NUM_1
#define GPS_TXD                    17
#define GPS_RXD                    16
#define GPS_BAUD_RATE              9600
#define GPS_BUF_SIZE               1024

// I2S Configuration for Bass Analysis
#define I2S_SAMPLE_RATE            44100
#define I2S_NUM                    I2S_NUM_0
#define I2S_BCK_PIN                26
#define I2S_WS_PIN                 25
#define I2S_DATA_PIN               27
#define BASS_FREQ_MIN              20    // Hz
#define BASS_FREQ_MAX              200   // Hz
#define FFT_SIZE                   1024

// Environmental Constants
#define SEA_LEVEL_PRESSURE         1013.25f  // hPa
#define TEMPERATURE_LAPSE_RATE     0.0065f   // K/m
#define GAS_CONSTANT               287.05f   // J/(kg·K)
#define GRAVITY                    9.80665f  // m/s²

// Calibration and Filtering
#define KALMAN_Q                   0.1f      // Process noise
#define KALMAN_R                   0.5f      // Measurement noise
#define BASS_COMPENSATION_RATE     0.5f      // dB per 300m altitude
#define PRESSURE_EFFECT_THRESHOLD  10.0f     // hPa

// Environmental Data Structure
typedef struct {
    float temperature;      // °C
    float humidity;         // %RH
    float pressure;         // hPa
    float altitude;         // meters
    float sound_speed;      // m/s
    int64_t timestamp;
} environmental_data_t;

// GPS Data Structure
typedef struct {
    float latitude;
    float longitude; 
    float altitude;         // meters above sea level
    float speed;            // km/h
    int fix_quality;        // 0=invalid, 1=GPS, 2=DGPS
    int satellites;
    int64_t timestamp;
} gps_data_t;

// Bass Analysis Structure
typedef struct {
    float bass_energy[16];  // Bass frequency bands
    float total_bass_power;
    float peak_bass_freq;
    float bass_attenuation;
    int64_t timestamp;
} bass_analysis_t;

// Sensor Fusion State (Extended Kalman Filter)
typedef struct {
    float altitude;         // Estimated altitude
    float velocity;         // Vertical velocity
    float bias_baro;        // Barometer bias
    float bias_gps;         // GPS bias
    float P[4][4];          // Covariance matrix
} kalman_state_t;

// Compensation Settings
typedef struct {
    float bass_boost;           // dB adjustment
    float frequency_shift;      // Hz shift due to temperature
    float loudness_compensation; // Psychoacoustic adjustment
    float pressure_factor;      // Pressure-based correction
    int64_t timestamp;
} compensation_settings_t;

// Global Variables
static i2c_config_t i2c_conf;
static i2s_chan_handle_t rx_handle = NULL;
static QueueHandle_t env_queue;
static QueueHandle_t bass_queue;
static environmental_data_t current_env = {0};
static gps_data_t current_gps = {0};
static bass_analysis_t current_bass = {0};
static kalman_state_t kalman_filter = {0};
static compensation_settings_t compensation = {0};

// Function Prototypes
static esp_err_t i2c_master_init(void);
static esp_err_t gps_uart_init(void);
static esp_err_t i2s_init(void);
static esp_err_t bmp390_init(void);
static esp_err_t bmp390_read(float *temperature, float *pressure);
static esp_err_t dht22_read(float *temperature, float *humidity);
static void environmental_task(void *pvParameters);
static void gps_task(void *pvParameters);
static void bass_analysis_task(void *pvParameters);
static void correlation_task(void *pvParameters);
static float calculate_altitude_from_pressure(float pressure, float temperature);
static float calculate_sound_speed(float temperature, float humidity);
static void kalman_predict(kalman_state_t *state, float dt);
static void kalman_update_barometer(kalman_state_t *state, float altitude_baro);
static void kalman_update_gps(kalman_state_t *state, float altitude_gps);
static compensation_settings_t calculate_compensation(environmental_data_t *env, gps_data_t *gps, bass_analysis_t *bass);
static void parse_nmea_sentence(const char *sentence);

// Initialize I2C Master
static esp_err_t i2c_master_init(void) {
    i2c_conf.mode = I2C_MODE_MASTER;
    i2c_conf.sda_io_num = I2C_MASTER_SDA_IO;
    i2c_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.scl_io_num = I2C_MASTER_SCL_IO;
    i2c_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, i2c_conf.mode, 
                                       I2C_MASTER_RX_BUF_DISABLE, 
                                       I2C_MASTER_TX_BUF_DISABLE, 0));
    
    ESP_LOGI(TAG, "I2C master initialized");
    return ESP_OK;
}

// Initialize GPS UART
static esp_err_t gps_uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = GPS_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_NUM, GPS_TXD, GPS_RXD, 
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_NUM, GPS_BUF_SIZE * 2, 0, 0, NULL, 0));
    
    ESP_LOGI(TAG, "GPS UART initialized");
    return ESP_OK;
}

// Initialize I2S for bass analysis
static esp_err_t i2s_init(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));
    
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_BITS_PER_SAMPLE_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DATA_PIN,
            .invert_flags = {0},
        },
    };
    
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    
    ESP_LOGI(TAG, "I2S initialized for bass analysis");
    return ESP_OK;
}

// BMP390 Barometric Sensor Initialization
static esp_err_t bmp390_init(void) {
    // BMP390 initialization sequence (simplified)
    uint8_t cmd_data[2];
    
    // Reset sensor
    cmd_data[0] = 0x7E;  // CMD register
    cmd_data[1] = 0xB6;  // Soft reset command
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMP390_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, cmd_data, 2, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BMP390 initialization failed");
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait for reset
    
    // Configure sensor for high precision
    cmd_data[0] = 0x1B;  // PWR_CTRL register
    cmd_data[1] = 0x33;  // Enable pressure and temperature, normal mode
    
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMP390_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, cmd_data, 2, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    ESP_LOGI(TAG, "BMP390 initialized successfully");
    return ret;
}

// Read BMP390 sensor data
static esp_err_t bmp390_read(float *temperature, float *pressure) {
    uint8_t data[6];
    uint8_t reg_addr = 0x04;  // PRESS_MSB register
    
    // Read 6 bytes (pressure + temperature)
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMP390_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMP390_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
        // Convert raw data (simplified - actual BMP390 requires calibration coefficients)
        uint32_t press_raw = (data[0] << 16) | (data[1] << 8) | data[2];
        uint32_t temp_raw = (data[3] << 16) | (data[4] << 8) | data[5];
        
        // Apply calibration (placeholder - real implementation needs stored coefficients)
        *temperature = (float)temp_raw / 100.0f - 40.0f;  // Simplified conversion
        *pressure = (float)press_raw / 100.0f;            // Simplified conversion
    }
    
    return ret;
}

// DHT22 Temperature/Humidity Sensor (simplified implementation)
static esp_err_t dht22_read(float *temperature, float *humidity) {
    // DHT22 implementation would require precise timing using GPIO
    // For this example, we'll use placeholder values that vary slightly
    static float base_temp = 22.0f;
    static float base_hum = 45.0f;
    
    // Add some realistic variation
    *temperature = base_temp + (esp_random() % 100) / 100.0f - 0.5f;
    *humidity = base_hum + (esp_random() % 200) / 100.0f - 1.0f;
    
    return ESP_OK;
}

// Calculate altitude from barometric pressure
static float calculate_altitude_from_pressure(float pressure, float temperature) {
    // International Standard Atmosphere formula
    float temp_kelvin = temperature + 273.15f;
    float pressure_ratio = pressure / SEA_LEVEL_PRESSURE;
    float altitude = (temp_kelvin / TEMPERATURE_LAPSE_RATE) * 
                    (1.0f - powf(pressure_ratio, (GAS_CONSTANT * TEMPERATURE_LAPSE_RATE) / GRAVITY));
    
    return altitude;
}

// Calculate sound speed based on temperature and humidity
static float calculate_sound_speed(float temperature, float humidity) {
    float temp_kelvin = temperature + 273.15f;
    float base_speed = 331.3f * sqrtf(temp_kelvin / 273.15f);
    
    // Humidity correction (simplified)
    float humidity_factor = 1.0f + (humidity / 100.0f) * 0.001f;
    
    return base_speed * humidity_factor;
}

// Kalman Filter Prediction Step
static void kalman_predict(kalman_state_t *state, float dt) {
    // State transition: [altitude, velocity, bias_baro, bias_gps]
    // altitude += velocity * dt
    state->altitude += state->velocity * dt;
    
    // Add process noise to covariance matrix
    state->P[0][0] += KALMAN_Q * dt * dt;  // Position uncertainty
    state->P[1][1] += KALMAN_Q * dt;       // Velocity uncertainty
    state->P[2][2] += KALMAN_Q * 0.1f;     // Barometer bias uncertainty
    state->P[3][3] += KALMAN_Q * 0.1f;     // GPS bias uncertainty
}

// Kalman Filter Update with Barometer
static void kalman_update_barometer(kalman_state_t *state, float altitude_baro) {
    // Innovation (measurement residual)
    float innovation = altitude_baro - (state->altitude + state->bias_baro);
    
    // Innovation covariance
    float S = state->P[0][0] + state->P[2][2] + KALMAN_R;
    
    // Kalman gain
    float K[4];
    K[0] = state->P[0][0] / S;  // Altitude gain
    K[1] = state->P[1][0] / S;  // Velocity gain  
    K[2] = state->P[2][0] / S;  // Baro bias gain
    K[3] = 0.0f;                // GPS bias not affected
    
    // State update
    state->altitude += K[0] * innovation;
    state->velocity += K[1] * innovation;
    state->bias_baro += K[2] * innovation;
    
    // Covariance update (simplified)
    for (int i = 0; i < 4; i++) {
        state->P[i][i] *= (1.0f - K[i]);
    }
}

// Kalman Filter Update with GPS
static void kalman_update_gps(kalman_state_t *state, float altitude_gps) {
    // Innovation (measurement residual)
    float innovation = altitude_gps - (state->altitude + state->bias_gps);
    
    // Innovation covariance (GPS is less precise)
    float S = state->P[0][0] + state->P[3][3] + KALMAN_R * 4.0f;
    
    // Kalman gain
    float K[4];
    K[0] = state->P[0][0] / S;  // Altitude gain
    K[1] = state->P[1][0] / S;  // Velocity gain
    K[2] = 0.0f;                // Baro bias not affected
    K[3] = state->P[3][0] / S;  // GPS bias gain
    
    // State update
    state->altitude += K[0] * innovation;
    state->velocity += K[1] * innovation;
    state->bias_gps += K[3] * innovation;
    
    // Covariance update (simplified)
    for (int i = 0; i < 4; i++) {
        if (i != 2) state->P[i][i] *= (1.0f - K[i]);
    }
}

// Calculate bass compensation settings
static compensation_settings_t calculate_compensation(environmental_data_t *env, 
                                                     gps_data_t *gps, 
                                                     bass_analysis_t *bass) {
    compensation_settings_t comp = {0};
    
    // Altitude-based bass boost (empirical formula)
    comp.bass_boost = (kalman_filter.altitude - 0.0f) / 300.0f * BASS_COMPENSATION_RATE;
    
    // Temperature effect on sound speed and frequency perception
    float temp_ratio = sqrtf((273.15f + env->temperature) / 293.15f);
    comp.frequency_shift = (temp_ratio - 1.0f) * 100.0f;  // Hz shift
    
    // Psychoacoustic pressure compensation
    float pressure_deviation = fabsf(env->pressure - SEA_LEVEL_PRESSURE);
    if (pressure_deviation > PRESSURE_EFFECT_THRESHOLD) {
        comp.pressure_factor = pressure_deviation / 100.0f;  // Normalized effect
    }
    
    // Atmospheric humidity effect
    comp.loudness_compensation = (env->humidity - 50.0f) / 100.0f * 0.5f;  // ±0.5dB
    
    comp.timestamp = esp_timer_get_time();
    
    return comp;
}

// Parse NMEA GPS sentences (simplified)
static void parse_nmea_sentence(const char *sentence) {
    if (strncmp(sentence, "$GPGGA", 6) == 0) {
        // Parse GGA sentence for position and altitude
        char *token = strtok((char*)sentence, ",");
        int field = 0;
        
        while (token != NULL && field < 15) {
            switch (field) {
                case 2:  // Latitude
                    if (strlen(token) > 0) {
                        current_gps.latitude = atof(token) / 100.0f;
                    }
                    break;
                case 4:  // Longitude  
                    if (strlen(token) > 0) {
                        current_gps.longitude = atof(token) / 100.0f;
                    }
                    break;
                case 6:  // Fix quality
                    current_gps.fix_quality = atoi(token);
                    break;
                case 7:  // Number of satellites
                    current_gps.satellites = atoi(token);
                    break;
                case 9:  // Altitude above sea level
                    if (strlen(token) > 0) {
                        current_gps.altitude = atof(token);
                    }
                    break;
            }
            token = strtok(NULL, ",");
            field++;
        }
        current_gps.timestamp = esp_timer_get_time();
    }
}

// Environmental Monitoring Task
static void environmental_task(void *pvParameters) {
    ESP_LOGI(TAG, "Environmental monitoring task started on core %d", xPortGetCoreID());
    
    while (1) {
        environmental_data_t env_data = {0};
        
        // Read barometric sensor
        esp_err_t baro_result = bmp390_read(&env_data.temperature, &env_data.pressure);
        
        // Read humidity sensor
        float dht_temp, dht_humidity;
        esp_err_t dht_result = dht22_read(&dht_temp, &dht_humidity);
        
        if (baro_result == ESP_OK && dht_result == ESP_OK) {
            // Use average temperature from both sensors
            env_data.temperature = (env_data.temperature + dht_temp) / 2.0f;
            env_data.humidity = dht_humidity;
            
            // Calculate derived values
            env_data.altitude = calculate_altitude_from_pressure(env_data.pressure, env_data.temperature);
            env_data.sound_speed = calculate_sound_speed(env_data.temperature, env_data.humidity);
            env_data.timestamp = esp_timer_get_time();
            
            // Update Kalman filter with barometer data
            static int64_t last_update = 0;
            float dt = (env_data.timestamp - last_update) / 1000000.0f;  // Convert to seconds
            if (last_update > 0) {
                kalman_predict(&kalman_filter, dt);
                kalman_update_barometer(&kalman_filter, env_data.altitude);
            }
            last_update = env_data.timestamp;
            
            // Update global state
            current_env = env_data;
            
            // Send to correlation queue
            xQueueSend(env_queue, &env_data, 0);
            
            ESP_LOGI(TAG, "Env: %.1f°C, %.1f%%RH, %.1fhPa, Alt: %.1fm (Kalman: %.1fm)", 
                     env_data.temperature, env_data.humidity, env_data.pressure,
                     env_data.altitude, kalman_filter.altitude);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1Hz update rate
    }
}

// GPS Monitoring Task
static void gps_task(void *pvParameters) {
    ESP_LOGI(TAG, "GPS monitoring task started on core %d", xPortGetCoreID());
    
    char gps_buffer[GPS_BUF_SIZE];
    
    while (1) {
        int len = uart_read_bytes(GPS_UART_NUM, gps_buffer, GPS_BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        
        if (len > 0) {
            gps_buffer[len] = '\0';
            
            // Parse NMEA sentences
            char *line = strtok(gps_buffer, "\r\n");
            while (line != NULL) {
                parse_nmea_sentence(line);
                line = strtok(NULL, "\r\n");
            }
            
            // Update Kalman filter with GPS altitude if fix is good
            if (current_gps.fix_quality > 0 && current_gps.satellites >= 4) {
                static int64_t last_gps_update = 0;
                int64_t now = esp_timer_get_time();
                float dt = (now - last_gps_update) / 1000000.0f;
                
                if (last_gps_update > 0) {
                    kalman_predict(&kalman_filter, dt);
                    kalman_update_gps(&kalman_filter, current_gps.altitude);
                }
                last_gps_update = now;
                
                ESP_LOGI(TAG, "GPS: Fix=%d, Sats=%d, Alt=%.1fm", 
                         current_gps.fix_quality, current_gps.satellites, current_gps.altitude);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Bass Analysis Task
static void bass_analysis_task(void *pvParameters) {
    ESP_LOGI(TAG, "Bass analysis task started on core %d", xPortGetCoreID());
    
    int32_t *i2s_samples = heap_caps_malloc(FFT_SIZE * sizeof(int32_t), MALLOC_CAP_DMA);
    if (!i2s_samples) {
        ESP_LOGE(TAG, "Failed to allocate I2S buffer");
        vTaskDelete(NULL);
        return;
    }
    
    size_t bytes_read;
    
    while (1) {
        // Read audio data for bass analysis
        esp_err_t ret = i2s_channel_read(rx_handle, i2s_samples, 
                                         FFT_SIZE * sizeof(int32_t), 
                                         &bytes_read, portMAX_DELAY);
        
        if (ret == ESP_OK && bytes_read > 0) {
            bass_analysis_t bass_data = {0};
            
            // Analyze bass frequencies (20-200 Hz)
            float total_energy = 0.0f;
            float peak_freq = 0.0f;
            float peak_magnitude = 0.0f;
            
            for (int i = 0; i < 16; i++) {
                float freq_start = BASS_FREQ_MIN + (i * (BASS_FREQ_MAX - BASS_FREQ_MIN)) / 16.0f;
                float freq_end = BASS_FREQ_MIN + ((i + 1) * (BASS_FREQ_MAX - BASS_FREQ_MIN)) / 16.0f;
                
                // Calculate energy in this frequency band (simplified)
                int start_bin = (int)(freq_start * FFT_SIZE / I2S_SAMPLE_RATE);
                int end_bin = (int)(freq_end * FFT_SIZE / I2S_SAMPLE_RATE);
                
                float band_energy = 0.0f;
                for (int j = start_bin; j < end_bin && j < FFT_SIZE/2; j++) {
                    // Convert I2S sample to magnitude (simplified)
                    float magnitude = fabsf((float)(i2s_samples[j] >> 14)) / 131072.0f;
                    band_energy += magnitude * magnitude;
                    
                    if (magnitude > peak_magnitude) {
                        peak_magnitude = magnitude;
                        peak_freq = (float)j * I2S_SAMPLE_RATE / FFT_SIZE;
                    }
                }
                
                bass_data.bass_energy[i] = sqrtf(band_energy);
                total_energy += band_energy;
            }
            
            bass_data.total_bass_power = sqrtf(total_energy);
            bass_data.peak_bass_freq = peak_freq;
            
            // Calculate bass attenuation based on current altitude
            bass_data.bass_attenuation = kalman_filter.altitude / 300.0f * BASS_COMPENSATION_RATE;
            bass_data.timestamp = esp_timer_get_time();
            
            // Update global state
            current_bass = bass_data;
            
            // Send to correlation queue
            xQueueSend(bass_queue, &bass_data, 0);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));  // 10Hz bass analysis
    }
    
    heap_caps_free(i2s_samples);
    vTaskDelete(NULL);
}

// Environmental Correlation and Compensation Task
static void correlation_task(void *pvParameters) {
    ESP_LOGI(TAG, "Correlation analysis task started on core %d", xPortGetCoreID());
    
    environmental_data_t env_data;
    bass_analysis_t bass_data;
    
    while (1) {
        // Process environmental data
        if (xQueueReceive(env_queue, &env_data, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Environmental data received
        }
        
        // Process bass analysis data
        if (xQueueReceive(bass_queue, &bass_data, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Bass analysis data received
        }
        
        // Calculate compensation settings
        compensation = calculate_compensation(&current_env, &current_gps, &current_bass);
        
        // Log correlation results periodically
        static int64_t last_log = 0;
        int64_t now = esp_timer_get_time();
        if (now - last_log > 5000000) {  // Every 5 seconds
            ESP_LOGI(TAG, "Compensation: Bass +%.1fdB, Freq %+.1fHz, Pressure %.2f, Alt %.1fm", 
                     compensation.bass_boost, compensation.frequency_shift, 
                     compensation.pressure_factor, kalman_filter.altitude);
            
            ESP_LOGI(TAG, "Bass Analysis: Power=%.3f, Peak=%.1fHz, Attenuation=%.1fdB", 
                     current_bass.total_bass_power, current_bass.peak_bass_freq, 
                     current_bass.bass_attenuation);
            
            last_log = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));  // 20Hz correlation analysis
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== Atmospheric Bass Barometer Starting ===");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create queues
    env_queue = xQueueCreate(10, sizeof(environmental_data_t));
    bass_queue = xQueueCreate(10, sizeof(bass_analysis_t));
    
    if (!env_queue || !bass_queue) {
        ESP_LOGE(TAG, "Failed to create data queues");
        return;
    }
    
    // Initialize hardware
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_ERROR_CHECK(gps_uart_init());
    ESP_ERROR_CHECK(i2s_init());
    ESP_ERROR_CHECK(bmp390_init());
    
    // Initialize Kalman filter
    kalman_filter.altitude = 0.0f;
    kalman_filter.velocity = 0.0f;
    kalman_filter.bias_baro = 0.0f;
    kalman_filter.bias_gps = 0.0f;
    
    // Initialize covariance matrix
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            kalman_filter.P[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    
    // Start tasks on appropriate cores
    xTaskCreatePinnedToCore(environmental_task, "env_monitor", 4096, NULL, 
                            configMAX_PRIORITIES - 2, NULL, 0);
    xTaskCreatePinnedToCore(gps_task, "gps_monitor", 4096, NULL, 
                            configMAX_PRIORITIES - 3, NULL, 0);
    xTaskCreatePinnedToCore(bass_analysis_task, "bass_analysis", 8192, NULL, 
                            configMAX_PRIORITIES - 1, NULL, 1);
    xTaskCreatePinnedToCore(correlation_task, "correlation", 4096, NULL, 
                            configMAX_PRIORITIES - 2, NULL, 0);
    
    ESP_LOGI(TAG, "System initialized successfully!");
    ESP_LOGI(TAG, "Sensors: BMP390 barometer, DHT22 temp/humidity, L86 GPS, INMP441 microphone");
    ESP_LOGI(TAG, "Bass frequency range: %d-%d Hz", BASS_FREQ_MIN, BASS_FREQ_MAX);
    ESP_LOGI(TAG, "Compensation rate: %.1f dB per 300m altitude", BASS_COMPENSATION_RATE);
    
    // Main monitoring loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "System running... Kalman altitude: %.1fm, Bass compensation: %+.1fdB", 
                 kalman_filter.altitude, compensation.bass_boost);
    }
}