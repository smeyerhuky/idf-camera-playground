#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include "esp_err.h"
#include "driver/gpio.h"

typedef struct {
    gpio_num_t button_gpio;
    gpio_num_t encoder_a_gpio;
    gpio_num_t encoder_b_gpio;
    uint32_t debounce_ms;
} input_handler_config_t;

esp_err_t input_handler_init(const input_handler_config_t *config);
void input_handler_task(void *pvParameters);

#endif