#include "ssd1306_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SSD1306";

esp_err_t ssd1306_init(const ssd1306_config_t *config, ssd1306_handle_t **handle)
{
    esp_err_t ret = ESP_OK;
    
    *handle = calloc(1, sizeof(ssd1306_handle_t));
    if (*handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    (*handle)->width = SSD1306_WIDTH;
    (*handle)->height = SSD1306_HEIGHT;
    
    ESP_LOGI(TAG, "Initializing SSD1306 display on I2C%d (SDA: %d, SCL: %d, ADDR: 0x%02X)",
             I2C_MASTER_NUM, config->sda_gpio, config->scl_gpio, config->address);
    
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sda_gpio,
        .scl_io_num = config->scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    ret = i2c_param_config(I2C_MASTER_NUM, &i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2C parameters");
        goto err;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM, i2c_conf.mode,
                           I2C_MASTER_RX_BUF_DISABLE,
                           I2C_MASTER_TX_BUF_DISABLE, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C driver");
        goto err;
    }
    
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = config->address,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .dc_low_on_data = true,
            .disable_control_phase = false,
        },
    };
    
    ret = esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_MASTER_NUM, &io_config, &(*handle)->io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO");
        goto err_driver;
    }
    
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
        .color_space = ESP_LCD_COLOR_SPACE_MONOCHROME,
    };
    
    ret = esp_lcd_new_panel_ssd1306((*handle)->io_handle, &panel_config, &(*handle)->panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create SSD1306 panel");
        goto err_io;
    }
    
    ret = esp_lcd_panel_reset((*handle)->panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset panel");
        goto err_panel;
    }
    
    ret = esp_lcd_panel_init((*handle)->panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize panel");
        goto err_panel;
    }
    
    ret = esp_lcd_panel_disp_on_off((*handle)->panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn on display");
        goto err_panel;
    }
    
    ESP_LOGI(TAG, "SSD1306 display initialized successfully");
    return ESP_OK;

err_panel:
    esp_lcd_panel_del((*handle)->panel_handle);
err_io:
    esp_lcd_panel_io_del((*handle)->io_handle);
err_driver:
    i2c_driver_delete(I2C_MASTER_NUM);
err:
    free(*handle);
    *handle = NULL;
    return ret;
}

esp_err_t ssd1306_deinit(ssd1306_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_lcd_panel_del(handle->panel_handle);
    esp_lcd_panel_io_del(handle->io_handle);
    i2c_driver_delete(I2C_MASTER_NUM);
    free(handle);
    
    return ESP_OK;
}

esp_lcd_panel_handle_t ssd1306_get_panel_handle(ssd1306_handle_t *handle)
{
    return handle ? handle->panel_handle : NULL;
}