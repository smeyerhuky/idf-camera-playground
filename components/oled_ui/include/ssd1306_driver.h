#ifndef SSD1306_DRIVER_H
#define SSD1306_DRIVER_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#define SSD1306_I2C_ADDRESS     0x3D
#define SSD1306_WIDTH           128
#define SSD1306_HEIGHT          64

#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      400000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

typedef struct {
    int sda_gpio;
    int scl_gpio;
    uint8_t address;
} ssd1306_config_t;

typedef struct {
    esp_lcd_panel_handle_t panel_handle;
    esp_lcd_panel_io_handle_t io_handle;
    int width;
    int height;
} ssd1306_handle_t;

esp_err_t ssd1306_init(const ssd1306_config_t *config, ssd1306_handle_t **handle);
esp_err_t ssd1306_deinit(ssd1306_handle_t *handle);
esp_lcd_panel_handle_t ssd1306_get_panel_handle(ssd1306_handle_t *handle);

#endif