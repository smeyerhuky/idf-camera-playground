#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 280
#define DISPLAY_BPP    16  // 16-bit color (RGB565)

// Display regions for Red Rocks photo booth UI
#define UI_HEADER_HEIGHT    40
#define UI_BUTTON_HEIGHT    80  
#define UI_QR_HEIGHT        120
#define UI_FOOTER_HEIGHT    40

// Colors (RGB565 format)
#define COLOR_BLACK    0x0000
#define COLOR_WHITE    0xFFFF
#define COLOR_RED      0xF800
#define COLOR_GREEN    0x07E0
#define COLOR_BLUE     0x001F
#define COLOR_YELLOW   0xFFE0
#define COLOR_ORANGE   0xFD20
#define COLOR_GRAY     0x8410

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} display_rect_t;

typedef struct {
    uint16_t x;
    uint16_t y;
} display_point_t;

typedef enum {
    TOUCH_EVENT_NONE,
    TOUCH_EVENT_PRESS,
    TOUCH_EVENT_RELEASE,
    TOUCH_EVENT_DRAG
} touch_event_t;

typedef struct {
    touch_event_t event;
    display_point_t point;
    bool is_valid;
} touch_data_t;

/**
 * @brief Initialize Waveshare display and touch controller
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t waveshare_display_init(void);

/**
 * @brief Deinitialize display
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t waveshare_display_deinit(void);

/**
 * @brief Fill entire screen with color
 * 
 * @param color RGB565 color value
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t waveshare_display_fill(uint16_t color);

/**
 * @brief Fill rectangle with color
 * 
 * @param rect Rectangle to fill
 * @param color RGB565 color value
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t waveshare_display_fill_rect(const display_rect_t* rect, uint16_t color);

/**
 * @brief Draw text at position
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @param text Text to draw
 * @param color Text color
 * @param bg_color Background color
 * @param scale Text scale factor (1-4)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t waveshare_display_draw_text(uint16_t x, uint16_t y, 
                                     const char* text, 
                                     uint16_t color, 
                                     uint16_t bg_color, 
                                     uint8_t scale);

/**
 * @brief Draw QR code bitmap
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @param qr_data QR code bitmap data (1 bit per pixel)
 * @param size QR code size (square)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t waveshare_display_draw_qr(uint16_t x, uint16_t y, 
                                   const uint8_t* qr_data, 
                                   uint8_t size);

/**
 * @brief Draw button with text
 * 
 * @param rect Button rectangle
 * @param text Button text
 * @param pressed Whether button appears pressed
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t waveshare_display_draw_button(const display_rect_t* rect, 
                                       const char* text, 
                                       bool pressed);

/**
 * @brief Read touch input
 * 
 * @param touch_data Output touch data
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t waveshare_display_read_touch(touch_data_t* touch_data);

/**
 * @brief Check if point is within rectangle
 * 
 * @param point Point to check
 * @param rect Rectangle bounds
 * @return true if point is inside rectangle
 */
bool waveshare_display_point_in_rect(const display_point_t* point, 
                                     const display_rect_t* rect);

/**
 * @brief Set display brightness
 * 
 * @param brightness Brightness level (0-100)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t waveshare_display_set_brightness(uint8_t brightness);

/**
 * @brief Display Red Rocks header
 */
esp_err_t waveshare_display_draw_header(void);

/**
 * @brief Display photo capture button
 * 
 * @param pressed Button state
 */
esp_err_t waveshare_display_draw_capture_button(bool pressed);

/**
 * @brief Display QR code area
 * 
 * @param qr_data QR code data, NULL for empty area
 */
esp_err_t waveshare_display_draw_qr_area(const uint8_t* qr_data);

/**
 * @brief Display footer instructions
 * 
 * @param text Footer text
 */
esp_err_t waveshare_display_draw_footer(const char* text);

#ifdef __cplusplus
}
#endif