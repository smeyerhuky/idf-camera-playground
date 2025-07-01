#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QR_MAX_URL_LENGTH 256
#define QR_MAX_VERSION 10
#define QR_DISPLAY_SIZE 120  // Display area for QR code

typedef struct {
    uint8_t version;
    uint8_t size;
    uint8_t *data;
    char url[QR_MAX_URL_LENGTH];
} qr_code_t;

/**
 * @brief Initialize QR code generator
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t qr_generator_init(void);

/**
 * @brief Generate QR code for photo download URL
 * 
 * @param photo_filename Filename of the photo
 * @param qr_code Output QR code structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t qr_generator_create_photo_qr(const char* photo_filename, qr_code_t* qr_code);

/**
 * @brief Generate QR code for custom URL
 * 
 * @param url URL to encode
 * @param qr_code Output QR code structure  
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t qr_generator_create_url_qr(const char* url, qr_code_t* qr_code);

/**
 * @brief Free QR code memory
 * 
 * @param qr_code QR code to free
 */
void qr_generator_free(qr_code_t* qr_code);

/**
 * @brief Get display buffer for QR code (for LCD display)
 * 
 * @param qr_code QR code structure
 * @param display_buffer Output buffer (120x120 pixels, 1-bit per pixel)
 * @param buffer_size Size of display buffer
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t qr_generator_get_display_buffer(const qr_code_t* qr_code, 
                                         uint8_t* display_buffer, 
                                         size_t buffer_size);

#ifdef __cplusplus
}
#endif