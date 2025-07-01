#ifndef OLED_UI_H
#define OLED_UI_H

#include "ssd1306_driver.h"
#include "lvgl.h"

#define OLED_UI_BUFFER_SIZE     (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

typedef enum {
    UI_EVENT_NONE = 0,
    UI_EVENT_BUTTON_PRESSED,
    UI_EVENT_ENCODER_TURNED,
    UI_EVENT_LONG_PRESS,
} ui_event_type_t;

typedef struct {
    ui_event_type_t type;
    int value;
} ui_event_t;

typedef struct {
    ssd1306_handle_t *display_handle;
    lv_disp_t *lvgl_display;
    lv_disp_draw_buf_t draw_buf;
    uint8_t *buf1;
    uint8_t *buf2;
} oled_ui_handle_t;

esp_err_t oled_ui_init(const ssd1306_config_t *config, oled_ui_handle_t **handle);
esp_err_t oled_ui_deinit(oled_ui_handle_t *handle);
void oled_ui_task(void *pvParameters);
esp_err_t oled_ui_send_event(const ui_event_t *event);

#endif