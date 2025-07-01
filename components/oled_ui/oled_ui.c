#include "oled_ui.h"
#include "camera_control_ui.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "OLED_UI";
static QueueHandle_t ui_event_queue = NULL;

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

esp_err_t oled_ui_init(const ssd1306_config_t *config, oled_ui_handle_t **handle)
{
    esp_err_t ret = ESP_OK;
    
    *handle = calloc(1, sizeof(oled_ui_handle_t));
    if (*handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    ui_event_queue = xQueueCreate(10, sizeof(ui_event_t));
    if (ui_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create UI event queue");
        free(*handle);
        return ESP_ERR_NO_MEM;
    }
    
    ret = ssd1306_init(config, &(*handle)->display_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SSD1306 display");
        vQueueDelete(ui_event_queue);
        free(*handle);
        return ret;
    }
    
    lv_init();
    
    (*handle)->buf1 = heap_caps_malloc(OLED_UI_BUFFER_SIZE, MALLOC_CAP_DMA);
    (*handle)->buf2 = heap_caps_malloc(OLED_UI_BUFFER_SIZE, MALLOC_CAP_DMA);
    if ((*handle)->buf1 == NULL || (*handle)->buf2 == NULL) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers");
        ret = ESP_ERR_NO_MEM;
        goto err;
    }
    
    lv_disp_draw_buf_init(&(*handle)->draw_buf, (*handle)->buf1, (*handle)->buf2, OLED_UI_BUFFER_SIZE);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SSD1306_WIDTH;
    disp_drv.ver_res = SSD1306_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &(*handle)->draw_buf;
    disp_drv.user_data = ssd1306_get_panel_handle((*handle)->display_handle);
    
    (*handle)->lvgl_display = lv_disp_drv_register(&disp_drv);
    if ((*handle)->lvgl_display == NULL) {
        ESP_LOGE(TAG, "Failed to register LVGL display driver");
        ret = ESP_FAIL;
        goto err;
    }
    
    camera_control_ui_init();
    
    ESP_LOGI(TAG, "OLED UI initialized successfully");
    return ESP_OK;

err:
    if ((*handle)->buf1) free((*handle)->buf1);
    if ((*handle)->buf2) free((*handle)->buf2);
    if ((*handle)->display_handle) ssd1306_deinit((*handle)->display_handle);
    vQueueDelete(ui_event_queue);
    free(*handle);
    *handle = NULL;
    return ret;
}

esp_err_t oled_ui_deinit(oled_ui_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (handle->buf1) free(handle->buf1);
    if (handle->buf2) free(handle->buf2);
    if (handle->display_handle) ssd1306_deinit(handle->display_handle);
    if (ui_event_queue) vQueueDelete(ui_event_queue);
    
    free(handle);
    return ESP_OK;
}

void oled_ui_task(void *pvParameters)
{
    ui_event_t event;
    
    while (1) {
        lv_timer_handler();
        
        if (xQueueReceive(ui_event_queue, &event, pdMS_TO_TICKS(5)) == pdTRUE) {
            camera_control_ui_handle_event(&event);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t oled_ui_send_event(const ui_event_t *event)
{
    if (ui_event_queue == NULL || event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xQueueSend(ui_event_queue, event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "UI event queue full, dropping event");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}