#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include "esp_err.h"
#include "ssd1306_driver.h"
#include "lvgl.h"

#define LVGL_TICK_PERIOD_MS    5
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE   (4 * 1024)
#define LVGL_TASK_PRIORITY     2

esp_err_t lvgl_port_init(ssd1306_handle_t *oled_handle);
esp_err_t lvgl_port_deinit(void);
void lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock(void);

#endif