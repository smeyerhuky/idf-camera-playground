#include "lvgl_port.h"
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "LVGL_PORT";

static lv_display_t *disp = NULL;
static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL;
static ssd1306_handle_t *g_oled_handle = NULL;
static SemaphoreHandle_t lvgl_mutex = NULL;
static TaskHandle_t lvgl_task_handle = NULL;
static esp_timer_handle_t lvgl_tick_timer = NULL;

// Monochrome display buffer (1 bit per pixel)
static uint8_t mono_buf[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (!g_oled_handle || !g_oled_handle->initialized) {
        lv_display_flush_ready(disp);
        return;
    }
    
    // Get framebuffer
    uint8_t *fb = ssd1306_get_framebuffer(g_oled_handle);
    if (!fb) {
        lv_display_flush_ready(disp);
        return;
    }
    
    // Convert LVGL's color buffer to monochrome
    lv_color_t *color_p = (lv_color_t *)px_map;
    
    for (int32_t y = area->y1; y <= area->y2; y++) {
        for (int32_t x = area->x1; x <= area->x2; x++) {
            // Get pixel from LVGL buffer
            lv_color_t color = *color_p++;
            
            // Convert to monochrome (threshold at 50% brightness)
            bool pixel_on = lv_color_brightness(color) > 127;
            
            // Calculate position in framebuffer
            uint16_t index = x + (y / 8) * SSD1306_WIDTH;
            uint8_t bit_mask = 1 << (y % 8);
            
            if (pixel_on) {
                fb[index] |= bit_mask;
            } else {
                fb[index] &= ~bit_mask;
            }
        }
    }
    
    // Update the display
    esp_err_t ret = ssd1306_update_screen(g_oled_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update screen: %s", esp_err_to_name(ret));
    }
    
    // Notify LVGL that flushing is done
    lv_display_flush_ready(disp);
}

static void lvgl_rounder_cb(lv_event_t *e)
{
    lv_area_t *area = lv_event_get_param(e);
    
    // Round to byte boundaries for Y axis (8 pixels per byte)
    area->y1 = (area->y1 / 8) * 8;
    area->y2 = ((area->y2 / 8) + 1) * 8 - 1;
    
    // Ensure within display bounds
    if (area->x1 < 0) area->x1 = 0;
    if (area->y1 < 0) area->y1 = 0;
    if (area->x2 >= SSD1306_WIDTH) area->x2 = SSD1306_WIDTH - 1;
    if (area->y2 >= SSD1306_HEIGHT) area->y2 = SSD1306_HEIGHT - 1;
}

static void lvgl_set_px_cb(lv_display_t *disp, uint8_t *buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y, lv_color_t color, lv_opa_t opa)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
    
    // Calculate position in monochrome buffer
    uint16_t index = x + (y / 8) * SSD1306_WIDTH;
    uint8_t bit_mask = 1 << (y % 8);
    
    // Convert color to monochrome
    bool pixel_on = lv_color_brightness(color) > 127;
    
    if (pixel_on && opa > LV_OPA_50) {
        mono_buf[index] |= bit_mask;
    } else {
        mono_buf[index] &= ~bit_mask;
    }
}

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task started");
    
    while (1) {
        lvgl_port_lock(0);
        
        uint32_t task_delay_ms = lv_timer_handler();
        
        lvgl_port_unlock();
        
        // Cap the delay
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

esp_err_t lvgl_port_init(ssd1306_handle_t *oled_handle)
{
    if (!oled_handle || !oled_handle->initialized) {
        ESP_LOGE(TAG, "Invalid OLED handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    g_oled_handle = oled_handle;
    
    // Initialize LVGL
    lv_init();
    
    // Create mutex
    lvgl_mutex = xSemaphoreCreateMutex();
    if (!lvgl_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Allocate display buffers (1 bit per pixel)
    size_t buf_size = SSD1306_WIDTH * SSD1306_HEIGHT / 8;
    buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (!buf1) {
        ESP_LOGE(TAG, "Failed to allocate display buffer");
        vSemaphoreDelete(lvgl_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Create display
    disp = lv_display_create(SSD1306_WIDTH, SSD1306_HEIGHT);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to create display");
        heap_caps_free(buf1);
        vSemaphoreDelete(lvgl_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Set display callbacks
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_add_event_cb(disp, lvgl_rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Set monochrome theme
    lv_theme_t *theme = lv_theme_mono_init(disp, false, LV_FONT_DEFAULT);
    lv_display_set_theme(disp, theme);
    
    // Create tick timer
    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick"
    };
    
    esp_err_t ret = esp_timer_create(&tick_timer_args, &lvgl_tick_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create tick timer: %s", esp_err_to_name(ret));
        lv_deinit();
        heap_caps_free(buf1);
        vSemaphoreDelete(lvgl_mutex);
        return ret;
    }
    
    ret = esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start tick timer: %s", esp_err_to_name(ret));
        esp_timer_delete(lvgl_tick_timer);
        lv_deinit();
        heap_caps_free(buf1);
        vSemaphoreDelete(lvgl_mutex);
        return ret;
    }
    
    // Create LVGL task
    BaseType_t xret = xTaskCreate(lvgl_task, "lvgl_task", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, &lvgl_task_handle);
    if (xret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        esp_timer_stop(lvgl_tick_timer);
        esp_timer_delete(lvgl_tick_timer);
        lv_deinit();
        heap_caps_free(buf1);
        vSemaphoreDelete(lvgl_mutex);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "LVGL port initialized successfully");
    return ESP_OK;
}

esp_err_t lvgl_port_deinit(void)
{
    if (lvgl_task_handle) {
        vTaskDelete(lvgl_task_handle);
        lvgl_task_handle = NULL;
    }
    
    if (lvgl_tick_timer) {
        esp_timer_stop(lvgl_tick_timer);
        esp_timer_delete(lvgl_tick_timer);
        lvgl_tick_timer = NULL;
    }
    
    lvgl_port_lock(portMAX_DELAY);
    
    lv_deinit();
    
    if (buf1) {
        heap_caps_free(buf1);
        buf1 = NULL;
    }
    
    if (buf2) {
        heap_caps_free(buf2);
        buf2 = NULL;
    }
    
    lvgl_port_unlock();
    
    if (lvgl_mutex) {
        vSemaphoreDelete(lvgl_mutex);
        lvgl_mutex = NULL;
    }
    
    g_oled_handle = NULL;
    disp = NULL;
    
    ESP_LOGI(TAG, "LVGL port deinitialized");
    return ESP_OK;
}

void lvgl_port_lock(int timeout_ms)
{
    if (!lvgl_mutex) return;
    
    const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    xSemaphoreTake(lvgl_mutex, timeout_ticks);
}

void lvgl_port_unlock(void)
{
    if (!lvgl_mutex) return;
    
    xSemaphoreGive(lvgl_mutex);
}