#include "qr_generator.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "qr_generator";

// Simple QR code implementation - for now we'll create a stub
// In production, would use a QR code library like esp32-qrcode

esp_err_t qr_generator_init(void) {
    ESP_LOGI(TAG, "QR Generator initialized");
    return ESP_OK;
}

esp_err_t qr_generator_create_photo_qr(const char* photo_filename, qr_code_t* qr_code) {
    if (!photo_filename || !qr_code) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Generate photo download URL
    snprintf(qr_code->url, QR_MAX_URL_LENGTH, 
             "http://192.168.4.1/photo/%s", photo_filename);
    
    ESP_LOGI(TAG, "Generated QR URL: %s", qr_code->url);
    
    // For now, create a simple stub QR code
    // TODO: Integrate actual QR code library
    qr_code->version = 3;
    qr_code->size = 29; // Size for version 3
    qr_code->data = malloc(qr_code->size * qr_code->size);
    
    if (!qr_code->data) {
        return ESP_ERR_NO_MEM;
    }
    
    // Create a simple pattern (placeholder)
    memset(qr_code->data, 0, qr_code->size * qr_code->size);
    
    // Add some basic pattern to make it recognizable
    for (int i = 0; i < qr_code->size; i++) {
        for (int j = 0; j < qr_code->size; j++) {
            // Create a checkerboard pattern for now
            if ((i + j) % 2 == 0) {
                qr_code->data[i * qr_code->size + j] = 1;
            }
        }
    }
    
    return ESP_OK;
}

esp_err_t qr_generator_create_url_qr(const char* url, qr_code_t* qr_code) {
    if (!url || !qr_code) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(qr_code->url, url, QR_MAX_URL_LENGTH - 1);
    qr_code->url[QR_MAX_URL_LENGTH - 1] = '\0';
    
    ESP_LOGI(TAG, "Generated QR URL: %s", qr_code->url);
    
    // Stub implementation - same as photo QR for now
    return qr_generator_create_photo_qr("test.jpg", qr_code);
}

void qr_generator_free(qr_code_t* qr_code) {
    if (qr_code && qr_code->data) {
        free(qr_code->data);
        qr_code->data = NULL;
    }
}

esp_err_t qr_generator_get_display_buffer(const qr_code_t* qr_code, 
                                         uint8_t* display_buffer, 
                                         size_t buffer_size) {
    if (!qr_code || !display_buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Expected buffer size for 120x120 pixels (1 bit per pixel)
    size_t expected_size = (QR_DISPLAY_SIZE * QR_DISPLAY_SIZE) / 8;
    if (buffer_size < expected_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Clear buffer
    memset(display_buffer, 0, buffer_size);
    
    // Scale QR code to display size
    if (qr_code->data) {
        int scale = QR_DISPLAY_SIZE / qr_code->size;
        
        for (int y = 0; y < qr_code->size && y * scale < QR_DISPLAY_SIZE; y++) {
            for (int x = 0; x < qr_code->size && x * scale < QR_DISPLAY_SIZE; x++) {
                if (qr_code->data[y * qr_code->size + x]) {
                    // Fill scaled area
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            int display_y = y * scale + sy;
                            int display_x = x * scale + sx;
                            
                            if (display_y < QR_DISPLAY_SIZE && display_x < QR_DISPLAY_SIZE) {
                                int bit_index = display_y * QR_DISPLAY_SIZE + display_x;
                                int byte_index = bit_index / 8;
                                int bit_offset = bit_index % 8;
                                
                                display_buffer[byte_index] |= (1 << bit_offset);
                            }
                        }
                    }
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "QR code converted to display buffer");
    return ESP_OK;
}