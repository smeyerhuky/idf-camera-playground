#include "waveshare_display.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <string.h>

static const char *TAG = "waveshare_display";

// Waveshare ESP32-S3 1.69" LCD pin definitions
#define LCD_DC_GPIO     GPIO_NUM_7
#define LCD_RST_GPIO    GPIO_NUM_6
#define LCD_CS_GPIO     GPIO_NUM_5
#define LCD_CLK_GPIO    GPIO_NUM_4
#define LCD_MOSI_GPIO   GPIO_NUM_3
#define LCD_BL_GPIO     GPIO_NUM_2

// Touch controller pins (if available)
#define TOUCH_SDA_GPIO  GPIO_NUM_8
#define TOUCH_SCL_GPIO  GPIO_NUM_9
#define TOUCH_INT_GPIO  GPIO_NUM_1

static esp_lcd_panel_handle_t lcd_panel = NULL;
static bool is_initialized = false;

esp_err_t waveshare_display_init(void) {
    if (is_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing Waveshare 1.69\" display");
    
    // Configure SPI bus
    spi_bus_config_t bus_config = {
        .mosi_io_num = LCD_MOSI_GPIO,
        .miso_io_num = -1,  // Not used for display
        .sclk_io_num = LCD_CLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t)
    };
    
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));
    
    // Configure LCD panel IO
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC_GPIO,
        .cs_gpio_num = LCD_CS_GPIO,
        .pclk_hz = 40000000,  // 40MHz
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, 
                                            &io_config, &io_handle));
    
    // Configure LCD panel (ST7789 driver)
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_GPIO,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &lcd_panel));
    
    // Reset and initialize panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
    
    // Configure display orientation for 240x280
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(lcd_panel, 0, 20));  // Adjust for 1.69" display
    
    // Configure backlight
    gpio_config_t bl_config = {
        .pin_bit_mask = (1ULL << LCD_BL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_config));
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL_GPIO, 1));  // Turn on backlight
    
    // TODO: Initialize touch controller (I2C)
    // For now, touch is stubbed
    
    is_initialized = true;
    ESP_LOGI(TAG, "Display initialized successfully");
    
    // Clear screen
    waveshare_display_fill(COLOR_BLACK);
    
    return ESP_OK;
}

esp_err_t waveshare_display_deinit(void) {
    if (!is_initialized) {
        return ESP_OK;
    }
    
    // Turn off backlight
    gpio_set_level(LCD_BL_GPIO, 0);
    
    // Deinitialize LCD panel
    if (lcd_panel) {
        esp_lcd_panel_del(lcd_panel);
        lcd_panel = NULL;
    }
    
    // Deinitialize SPI bus
    spi_bus_free(SPI2_HOST);
    
    is_initialized = false;
    ESP_LOGI(TAG, "Display deinitialized");
    
    return ESP_OK;
}

esp_err_t waveshare_display_fill(uint16_t color) {
    if (!is_initialized || !lcd_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create buffer filled with color
    size_t buffer_size = DISPLAY_WIDTH * DISPLAY_HEIGHT;
    uint16_t *buffer = malloc(buffer_size * sizeof(uint16_t));
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    for (size_t i = 0; i < buffer_size; i++) {
        buffer[i] = color;
    }
    
    esp_err_t ret = esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, 
                                             DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                             buffer);
    
    free(buffer);
    return ret;
}

esp_err_t waveshare_display_fill_rect(const display_rect_t* rect, uint16_t color) {
    if (!is_initialized || !lcd_panel || !rect) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create buffer for rectangle
    size_t buffer_size = rect->width * rect->height;
    uint16_t *buffer = malloc(buffer_size * sizeof(uint16_t));
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    for (size_t i = 0; i < buffer_size; i++) {
        buffer[i] = color;
    }
    
    esp_err_t ret = esp_lcd_panel_draw_bitmap(lcd_panel, 
                                             rect->x, rect->y,
                                             rect->x + rect->width, 
                                             rect->y + rect->height,
                                             buffer);
    
    free(buffer);
    return ret;
}

esp_err_t waveshare_display_draw_text(uint16_t x, uint16_t y, 
                                     const char* text, 
                                     uint16_t color, 
                                     uint16_t bg_color, 
                                     uint8_t scale) {
    if (!text) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Simple text rendering stub
    // TODO: Implement bitmap font rendering
    ESP_LOGI(TAG, "Drawing text at (%d,%d): %s", x, y, text);
    
    // For now, just draw a colored rectangle as placeholder
    display_rect_t text_rect = {
        .x = x,
        .y = y,
        .width = strlen(text) * 8 * scale,
        .height = 16 * scale
    };
    
    return waveshare_display_fill_rect(&text_rect, color);
}

esp_err_t waveshare_display_draw_qr(uint16_t x, uint16_t y, 
                                   const uint8_t* qr_data, 
                                   uint8_t size) {
    if (!qr_data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: Implement QR code bitmap rendering
    ESP_LOGI(TAG, "Drawing QR code at (%d,%d) size %d", x, y, size);
    
    // For now, draw a placeholder rectangle
    display_rect_t qr_rect = {
        .x = x,
        .y = y,
        .width = 120,
        .height = 120
    };
    
    return waveshare_display_fill_rect(&qr_rect, COLOR_WHITE);
}

esp_err_t waveshare_display_draw_button(const display_rect_t* rect, 
                                       const char* text, 
                                       bool pressed) {
    if (!rect || !text) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Draw button background
    uint16_t bg_color = pressed ? COLOR_ORANGE : COLOR_BLUE;
    esp_err_t ret = waveshare_display_fill_rect(rect, bg_color);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Draw button text (centered)
    uint16_t text_x = rect->x + (rect->width / 2) - (strlen(text) * 4);
    uint16_t text_y = rect->y + (rect->height / 2) - 8;
    
    return waveshare_display_draw_text(text_x, text_y, text, COLOR_WHITE, bg_color, 2);
}

esp_err_t waveshare_display_read_touch(touch_data_t* touch_data) {
    if (!touch_data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: Implement actual touch reading via I2C
    // For now, return no touch event
    touch_data->event = TOUCH_EVENT_NONE;
    touch_data->point.x = 0;
    touch_data->point.y = 0;
    touch_data->is_valid = false;
    
    return ESP_OK;
}

bool waveshare_display_point_in_rect(const display_point_t* point, 
                                     const display_rect_t* rect) {
    if (!point || !rect) {
        return false;
    }
    
    return (point->x >= rect->x && 
            point->x < rect->x + rect->width &&
            point->y >= rect->y && 
            point->y < rect->y + rect->height);
}

esp_err_t waveshare_display_set_brightness(uint8_t brightness) {
    // TODO: Implement PWM brightness control
    ESP_LOGI(TAG, "Setting brightness to %d%%", brightness);
    
    // For now, just on/off
    if (brightness > 0) {
        gpio_set_level(LCD_BL_GPIO, 1);
    } else {
        gpio_set_level(LCD_BL_GPIO, 0);
    }
    
    return ESP_OK;
}

// Red Rocks UI specific functions

esp_err_t waveshare_display_draw_header(void) {
    display_rect_t header_rect = {
        .x = 0,
        .y = 0,
        .width = DISPLAY_WIDTH,
        .height = UI_HEADER_HEIGHT
    };
    
    esp_err_t ret = waveshare_display_fill_rect(&header_rect, COLOR_RED);
    if (ret != ESP_OK) {
        return ret;
    }
    
    return waveshare_display_draw_text(20, 12, "RED ROCKS PHOTOS", 
                                      COLOR_WHITE, COLOR_RED, 1);
}

esp_err_t waveshare_display_draw_capture_button(bool pressed) {
    display_rect_t button_rect = {
        .x = 20,
        .y = UI_HEADER_HEIGHT + 10,
        .width = DISPLAY_WIDTH - 40,
        .height = UI_BUTTON_HEIGHT
    };
    
    return waveshare_display_draw_button(&button_rect, "TAKE PHOTO", pressed);
}

esp_err_t waveshare_display_draw_qr_area(const uint8_t* qr_data) {
    display_rect_t qr_rect = {
        .x = (DISPLAY_WIDTH - 120) / 2,
        .y = UI_HEADER_HEIGHT + UI_BUTTON_HEIGHT + 20,
        .width = 120,
        .height = 120
    };
    
    if (qr_data) {
        return waveshare_display_draw_qr(qr_rect.x, qr_rect.y, qr_data, 120);
    } else {
        return waveshare_display_fill_rect(&qr_rect, COLOR_GRAY);
    }
}

esp_err_t waveshare_display_draw_footer(const char* text) {
    display_rect_t footer_rect = {
        .x = 0,
        .y = DISPLAY_HEIGHT - UI_FOOTER_HEIGHT,
        .width = DISPLAY_WIDTH,
        .height = UI_FOOTER_HEIGHT
    };
    
    esp_err_t ret = waveshare_display_fill_rect(&footer_rect, COLOR_BLACK);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (text) {
        uint16_t text_x = (DISPLAY_WIDTH - strlen(text) * 6) / 2;
        return waveshare_display_draw_text(text_x, footer_rect.y + 12, 
                                          text, COLOR_WHITE, COLOR_BLACK, 1);
    }
    
    return ESP_OK;
}