#ifndef SSD1306_DRIVER_H
#define SSD1306_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64
#define SSD1306_PAGES  (SSD1306_HEIGHT / 8)

#define SSD1306_I2C_ADDR_PRIMARY   0x3D
#define SSD1306_I2C_ADDR_SECONDARY 0x3C

#define SSD1306_CMD_DISPLAY_OFF         0xAE
#define SSD1306_CMD_DISPLAY_ON          0xAF
#define SSD1306_CMD_SET_CONTRAST        0x81
#define SSD1306_CMD_DISPLAY_ALL_ON      0xA5
#define SSD1306_CMD_DISPLAY_RAM         0xA4
#define SSD1306_CMD_DISPLAY_INVERTED    0xA7
#define SSD1306_CMD_DISPLAY_NORMAL      0xA6
#define SSD1306_CMD_SET_MUX_RATIO       0xA8
#define SSD1306_CMD_SET_DISPLAY_OFFSET  0xD3
#define SSD1306_CMD_SET_DISPLAY_START   0x40
#define SSD1306_CMD_SET_SEGMENT_REMAP   0xA0
#define SSD1306_CMD_SET_COM_SCAN_DIR    0xC0
#define SSD1306_CMD_SET_COM_PINS        0xDA
#define SSD1306_CMD_SET_CLOCK_DIV       0xD5
#define SSD1306_CMD_SET_PRECHARGE       0xD9
#define SSD1306_CMD_SET_VCOMH_DESELECT  0xDB
#define SSD1306_CMD_CHARGE_PUMP         0x8D
#define SSD1306_CMD_SET_MEMORY_MODE     0x20
#define SSD1306_CMD_SET_COLUMN_ADDR     0x21
#define SSD1306_CMD_SET_PAGE_ADDR       0x22

typedef struct {
    i2c_master_dev_handle_t i2c_dev;
    uint8_t i2c_addr;
    uint8_t *framebuffer;
    bool initialized;
} ssd1306_handle_t;

typedef struct {
    uint8_t i2c_port;
    uint8_t i2c_addr;
    int sda_pin;
    int scl_pin;
    uint32_t clk_speed;
} ssd1306_config_t;

esp_err_t ssd1306_init(ssd1306_handle_t *handle, const ssd1306_config_t *config);
esp_err_t ssd1306_deinit(ssd1306_handle_t *handle);
esp_err_t ssd1306_display_on(ssd1306_handle_t *handle, bool on);
esp_err_t ssd1306_set_contrast(ssd1306_handle_t *handle, uint8_t contrast);
esp_err_t ssd1306_set_invert(ssd1306_handle_t *handle, bool invert);
esp_err_t ssd1306_clear_screen(ssd1306_handle_t *handle);
esp_err_t ssd1306_update_screen(ssd1306_handle_t *handle);
esp_err_t ssd1306_draw_pixel(ssd1306_handle_t *handle, uint8_t x, uint8_t y, bool color);
uint8_t *ssd1306_get_framebuffer(ssd1306_handle_t *handle);

#endif