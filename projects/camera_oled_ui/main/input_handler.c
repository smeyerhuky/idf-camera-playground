#include "input_handler.h"
#include "oled_ui.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "INPUT_HANDLER";

static input_handler_config_t input_config;
static TimerHandle_t long_press_timer = NULL;
static bool button_pressed = false;
static int encoder_last_state = 0;

static void long_press_timer_callback(TimerHandle_t xTimer)
{
    if (button_pressed) {
        ui_event_t event = {
            .type = UI_EVENT_LONG_PRESS,
            .value = 1
        };
        oled_ui_send_event(&event);
        ESP_LOGI(TAG, "Long press detected");
    }
}

static void handle_button_press(void)
{
    static uint32_t last_press_time = 0;
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if (current_time - last_press_time < input_config.debounce_ms) {
        return;
    }
    last_press_time = current_time;
    
    int button_state = gpio_get_level(input_config.button_gpio);
    
    if (button_state == 0 && !button_pressed) {
        button_pressed = true;
        if (long_press_timer) {
            xTimerStart(long_press_timer, 0);
        }
        ESP_LOGI(TAG, "Button pressed");
    } else if (button_state == 1 && button_pressed) {
        button_pressed = false;
        if (long_press_timer) {
            xTimerStop(long_press_timer, 0);
        }
        
        ui_event_t event = {
            .type = UI_EVENT_BUTTON_PRESSED,
            .value = 1
        };
        oled_ui_send_event(&event);
        ESP_LOGI(TAG, "Button released - sending press event");
    }
}

static void handle_encoder_change(void)
{
    if (input_config.encoder_a_gpio == -1 || input_config.encoder_b_gpio == -1) {
        return;
    }
    
    int a_state = gpio_get_level(input_config.encoder_a_gpio);
    int b_state = gpio_get_level(input_config.encoder_b_gpio);
    int current_state = (a_state << 1) | b_state;
    
    static const int encoder_table[4][4] = {
        {0,  1, -1,  0},
        {-1, 0,  0,  1},
        {1,  0,  0, -1},
        {0, -1,  1,  0}
    };
    
    int direction = encoder_table[encoder_last_state][current_state];
    encoder_last_state = current_state;
    
    if (direction != 0) {
        ui_event_t event = {
            .type = UI_EVENT_ENCODER_TURNED,
            .value = direction
        };
        oled_ui_send_event(&event);
        ESP_LOGI(TAG, "Encoder turned: %s", direction > 0 ? "CW" : "CCW");
    }
}

esp_err_t input_handler_init(const input_handler_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    input_config = *config;
    
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << config->button_gpio),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure button GPIO");
        return ret;
    }
    
    if (config->encoder_a_gpio != -1 && config->encoder_b_gpio != -1) {
        io_conf.pin_bit_mask = (1ULL << config->encoder_a_gpio) | (1ULL << config->encoder_b_gpio);
        ret = gpio_config(&io_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure encoder GPIOs");
            return ret;
        }
        
        encoder_last_state = (gpio_get_level(config->encoder_a_gpio) << 1) | gpio_get_level(config->encoder_b_gpio);
    }
    
    long_press_timer = xTimerCreate("long_press_timer", pdMS_TO_TICKS(1000), pdFALSE, NULL, long_press_timer_callback);
    if (long_press_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create long press timer");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Input handler initialized");
    return ESP_OK;
}

void input_handler_task(void *pvParameters)
{
    while (1) {
        handle_button_press();
        handle_encoder_change();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}