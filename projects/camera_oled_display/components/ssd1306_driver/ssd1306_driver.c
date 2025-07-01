#include "ssd1306_driver.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SSD1306";

static esp_err_t ssd1306_write_command(ssd1306_handle_t *handle, uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};  // Co = 0, D/C# = 0
    return i2c_master_transmit(handle->i2c_dev, data, 2, pdMS_TO_TICKS(100));
}

static esp_err_t ssd1306_write_data(ssd1306_handle_t *handle, const uint8_t *data, size_t len)
{
    uint8_t *buffer = malloc(len + 1);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    buffer[0] = 0x40;  // Co = 0, D/C# = 1
    memcpy(buffer + 1, data, len);
    
    esp_err_t ret = i2c_master_transmit(handle->i2c_dev, buffer, len + 1, pdMS_TO_TICKS(100));
    free(buffer);
    
    return ret;
}

static esp_err_t ssd1306_hardware_init(ssd1306_handle_t *handle)
{
    esp_err_t ret;
    
    // Display off
    ret = ssd1306_write_command(handle, SSD1306_CMD_DISPLAY_OFF);
    if (ret != ESP_OK) return ret;
    
    // Set display clock divide ratio/oscillator frequency
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_CLOCK_DIV);
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, 0x80);
    if (ret != ESP_OK) return ret;
    
    // Set multiplex ratio
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_MUX_RATIO);
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, SSD1306_HEIGHT - 1);
    if (ret != ESP_OK) return ret;
    
    // Set display offset
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_DISPLAY_OFFSET);
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, 0x00);
    if (ret != ESP_OK) return ret;
    
    // Set display start line
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_DISPLAY_START | 0x00);
    if (ret != ESP_OK) return ret;
    
    // Enable charge pump
    ret = ssd1306_write_command(handle, SSD1306_CMD_CHARGE_PUMP);
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, 0x14);  // Enable
    if (ret != ESP_OK) return ret;
    
    // Set memory addressing mode
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_MEMORY_MODE);
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, 0x00);  // Horizontal addressing mode
    if (ret != ESP_OK) return ret;
    
    // Set segment re-map
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_SEGMENT_REMAP | 0x01);
    if (ret != ESP_OK) return ret;
    
    // Set COM scan direction
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_COM_SCAN_DIR | 0x08);
    if (ret != ESP_OK) return ret;
    
    // Set COM pins hardware configuration
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_COM_PINS);
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, 0x12);
    if (ret != ESP_OK) return ret;
    
    // Set contrast
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_CONTRAST);
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, 0xCF);
    if (ret != ESP_OK) return ret;
    
    // Set precharge period
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_PRECHARGE);
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, 0xF1);
    if (ret != ESP_OK) return ret;
    
    // Set VCOMH deselect level
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_VCOMH_DESELECT);
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, 0x40);
    if (ret != ESP_OK) return ret;
    
    // Display RAM content
    ret = ssd1306_write_command(handle, SSD1306_CMD_DISPLAY_RAM);
    if (ret != ESP_OK) return ret;
    
    // Normal display (not inverted)
    ret = ssd1306_write_command(handle, SSD1306_CMD_DISPLAY_NORMAL);
    if (ret != ESP_OK) return ret;
    
    // Clear screen
    ret = ssd1306_clear_screen(handle);
    if (ret != ESP_OK) return ret;
    
    // Display on
    ret = ssd1306_write_command(handle, SSD1306_CMD_DISPLAY_ON);
    
    return ret;
}

esp_err_t ssd1306_init(ssd1306_handle_t *handle, const ssd1306_config_t *config)
{
    if (!handle || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(handle, 0, sizeof(ssd1306_handle_t));
    
    // Allocate framebuffer
    size_t fb_size = SSD1306_WIDTH * SSD1306_PAGES;
    handle->framebuffer = calloc(1, fb_size);
    if (!handle->framebuffer) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer");
        return ESP_ERR_NO_MEM;
    }
    
    // Configure I2C bus
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = config->i2c_port,
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    i2c_master_bus_handle_t bus_handle;
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        free(handle->framebuffer);
        return ret;
    }
    
    // Add device to bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_addr,
        .scl_speed_hz = config->clk_speed,
    };
    
    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &handle->i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device to I2C bus: %s", esp_err_to_name(ret));
        i2c_del_master_bus(bus_handle);
        free(handle->framebuffer);
        return ret;
    }
    
    handle->i2c_addr = config->i2c_addr;
    
    // Initialize hardware
    ret = ssd1306_hardware_init(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize hardware: %s", esp_err_to_name(ret));
        i2c_master_bus_rm_device(handle->i2c_dev);
        free(handle->framebuffer);
        return ret;
    }
    
    handle->initialized = true;
    ESP_LOGI(TAG, "SSD1306 initialized successfully at address 0x%02X", config->i2c_addr);
    
    return ESP_OK;
}

esp_err_t ssd1306_deinit(ssd1306_handle_t *handle)
{
    if (!handle || !handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ssd1306_display_on(handle, false);
    
    if (handle->i2c_dev) {
        i2c_master_bus_rm_device(handle->i2c_dev);
    }
    
    if (handle->framebuffer) {
        free(handle->framebuffer);
        handle->framebuffer = NULL;
    }
    
    handle->initialized = false;
    
    return ESP_OK;
}

esp_err_t ssd1306_display_on(ssd1306_handle_t *handle, bool on)
{
    if (!handle || !handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return ssd1306_write_command(handle, on ? SSD1306_CMD_DISPLAY_ON : SSD1306_CMD_DISPLAY_OFF);
}

esp_err_t ssd1306_set_contrast(ssd1306_handle_t *handle, uint8_t contrast)
{
    if (!handle || !handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = ssd1306_write_command(handle, SSD1306_CMD_SET_CONTRAST);
    if (ret != ESP_OK) return ret;
    
    return ssd1306_write_command(handle, contrast);
}

esp_err_t ssd1306_set_invert(ssd1306_handle_t *handle, bool invert)
{
    if (!handle || !handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return ssd1306_write_command(handle, invert ? SSD1306_CMD_DISPLAY_INVERTED : SSD1306_CMD_DISPLAY_NORMAL);
}

esp_err_t ssd1306_clear_screen(ssd1306_handle_t *handle)
{
    if (!handle || !handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    memset(handle->framebuffer, 0, SSD1306_WIDTH * SSD1306_PAGES);
    return ssd1306_update_screen(handle);
}

esp_err_t ssd1306_update_screen(ssd1306_handle_t *handle)
{
    if (!handle || !handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    
    // Set column address range
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_COLUMN_ADDR);
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, 0);  // Start
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, SSD1306_WIDTH - 1);  // End
    if (ret != ESP_OK) return ret;
    
    // Set page address range
    ret = ssd1306_write_command(handle, SSD1306_CMD_SET_PAGE_ADDR);
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, 0);  // Start
    if (ret != ESP_OK) return ret;
    ret = ssd1306_write_command(handle, SSD1306_PAGES - 1);  // End
    if (ret != ESP_OK) return ret;
    
    // Write framebuffer
    return ssd1306_write_data(handle, handle->framebuffer, SSD1306_WIDTH * SSD1306_PAGES);
}

esp_err_t ssd1306_draw_pixel(ssd1306_handle_t *handle, uint8_t x, uint8_t y, bool color)
{
    if (!handle || !handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint16_t index = x + (y / 8) * SSD1306_WIDTH;
    uint8_t bit_mask = 1 << (y % 8);
    
    if (color) {
        handle->framebuffer[index] |= bit_mask;
    } else {
        handle->framebuffer[index] &= ~bit_mask;
    }
    
    return ESP_OK;
}

uint8_t *ssd1306_get_framebuffer(ssd1306_handle_t *handle)
{
    if (!handle || !handle->initialized) {
        return NULL;
    }
    
    return handle->framebuffer;
}