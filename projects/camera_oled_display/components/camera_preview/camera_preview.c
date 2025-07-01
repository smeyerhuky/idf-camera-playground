#include "camera_preview.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "CAMERA_PREVIEW";

static camera_preview_t *g_preview = NULL;
static TimerHandle_t preview_timer = NULL;

// Downsample RGB565 image to monochrome for OLED display
static void rgb565_to_mono(const uint8_t *rgb565_buf, uint8_t *mono_buf, 
                          uint16_t src_width, uint16_t src_height,
                          uint16_t dst_width, uint16_t dst_height)
{
    if (!rgb565_buf || !mono_buf) return;
    
    // Calculate scaling factors
    float x_scale = (float)src_width / dst_width;
    float y_scale = (float)src_height / dst_height;
    
    for (uint16_t y = 0; y < dst_height; y++) {
        for (uint16_t x = 0; x < dst_width; x++) {
            // Calculate source coordinates
            uint16_t src_x = (uint16_t)(x * x_scale);
            uint16_t src_y = (uint16_t)(y * y_scale);
            
            // Ensure within bounds
            if (src_x >= src_width) src_x = src_width - 1;
            if (src_y >= src_height) src_y = src_height - 1;
            
            // Get RGB565 pixel
            uint16_t src_offset = (src_y * src_width + src_x) * 2;
            uint16_t rgb565 = (rgb565_buf[src_offset + 1] << 8) | rgb565_buf[src_offset];
            
            // Extract RGB components
            uint8_t r = (rgb565 >> 11) & 0x1F;
            uint8_t g = (rgb565 >> 5) & 0x3F;
            uint8_t b = rgb565 & 0x1F;
            
            // Convert to 8-bit and calculate grayscale
            r = (r * 255) / 31;
            g = (g * 255) / 63;
            b = (b * 255) / 31;
            
            uint8_t gray = (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b);
            
            // Convert to monochrome (1 = white, 0 = black)
            uint8_t mono_pixel = (gray > 127) ? 1 : 0;
            
            // Set pixel in monochrome buffer
            uint16_t dst_offset = y * dst_width + x;
            mono_buf[dst_offset] = mono_pixel ? 0xFF : 0x00;
        }
    }
}

static void preview_timer_callback(TimerHandle_t xTimer)
{
    if (g_preview && g_preview->running) {
        camera_preview_update_frame(g_preview);
    }
}

esp_err_t camera_preview_init(camera_preview_t *preview, const camera_preview_config_t *config)
{
    if (!preview || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(preview, 0, sizeof(camera_preview_t));
    g_preview = preview;
    
    // Create main container
    preview->container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(preview->container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(preview->container);
    lv_obj_set_style_bg_opa(preview->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(preview->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(preview->container, 0, 0);
    
    // Create canvas for image display
    size_t canvas_buf_size = config->preview_width * config->preview_height;
    lv_canvas_buf_init(&preview->cbuf, config->preview_width, config->preview_height, LV_COLOR_FORMAT_L8, 
                       heap_caps_malloc(canvas_buf_size, MALLOC_CAP_DMA));
    
    if (!preview->cbuf.buf) {
        ESP_LOGE(TAG, "Failed to allocate canvas buffer");
        lv_obj_delete(preview->container);
        return ESP_ERR_NO_MEM;
    }
    
    preview->canvas = lv_canvas_create(preview->container);
    lv_canvas_set_buffer(preview->canvas, &preview->cbuf);
    lv_obj_set_pos(preview->canvas, 0, 0);
    lv_canvas_fill_bg(preview->canvas, lv_color_black(), LV_OPA_COVER);
    
    // Create info label if requested
    if (config->show_info) {
        preview->info_label = lv_label_create(preview->container);
        lv_label_set_text(preview->info_label, "Camera");
        lv_obj_set_style_text_font(preview->info_label, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(preview->info_label, lv_color_white(), 0);
        lv_obj_align_to(preview->info_label, preview->canvas, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    }
    
    // Create update timer
    preview_timer = xTimerCreate("preview_timer",
                               pdMS_TO_TICKS(config->refresh_rate_ms),
                               pdTRUE,
                               NULL,
                               preview_timer_callback);
    
    if (!preview_timer) {
        ESP_LOGE(TAG, "Failed to create preview timer");
        heap_caps_free(preview->cbuf.buf);
        lv_obj_delete(preview->container);
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Camera preview initialized (%dx%d)", config->preview_width, config->preview_height);
    return ESP_OK;
}

esp_err_t camera_preview_deinit(camera_preview_t *preview)
{
    if (!preview) {
        return ESP_ERR_INVALID_ARG;
    }
    
    camera_preview_stop(preview);
    
    if (preview_timer) {
        xTimerDelete(preview_timer, pdMS_TO_TICKS(100));
        preview_timer = NULL;
    }
    
    if (preview->last_frame) {
        esp_camera_fb_return(preview->last_frame);
        preview->last_frame = NULL;
    }
    
    if (preview->cbuf.buf) {
        heap_caps_free(preview->cbuf.buf);
        preview->cbuf.buf = NULL;
    }
    
    if (preview->container) {
        lv_obj_delete(preview->container);
        preview->container = NULL;
    }
    
    g_preview = NULL;
    
    ESP_LOGI(TAG, "Camera preview deinitialized");
    return ESP_OK;
}

esp_err_t camera_preview_start(camera_preview_t *preview)
{
    if (!preview) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (preview->running) {
        return ESP_OK;
    }
    
    preview->running = true;
    
    if (preview_timer) {
        xTimerStart(preview_timer, pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Camera preview started");
    return ESP_OK;
}

esp_err_t camera_preview_stop(camera_preview_t *preview)
{
    if (!preview) {
        return ESP_ERR_INVALID_ARG;
    }
    
    preview->running = false;
    
    if (preview_timer) {
        xTimerStop(preview_timer, pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Camera preview stopped");
    return ESP_OK;
}

esp_err_t camera_preview_update_frame(camera_preview_t *preview)
{
    if (!preview || !preview->running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Get frame from camera
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGW(TAG, "Failed to get camera frame");
        return ESP_FAIL;
    }
    
    // Return previous frame if any
    if (preview->last_frame) {
        esp_camera_fb_return(preview->last_frame);
    }
    preview->last_frame = fb;
    
    // Convert and display frame
    if (fb->format == PIXFORMAT_RGB565) {
        // Get canvas buffer
        lv_color_t *canvas_buf = (lv_color_t *)preview->cbuf.buf;
        uint8_t *mono_buf = (uint8_t *)canvas_buf;
        
        // Convert RGB565 to monochrome
        rgb565_to_mono(fb->buf, mono_buf, fb->width, fb->height, 
                      preview->cbuf.header.w, preview->cbuf.header.h);
        
        // Update canvas
        lv_canvas_invalidate(preview->canvas);
        
        // Update info label
        if (preview->info_label) {
            static char info_text[64];
            snprintf(info_text, sizeof(info_text), "%dx%d", fb->width, fb->height);
            lv_label_set_text(preview->info_label, info_text);
        }
    } else {
        ESP_LOGW(TAG, "Unsupported pixel format: %d", fb->format);
    }
    
    return ESP_OK;
}

lv_obj_t *camera_preview_get_container(camera_preview_t *preview)
{
    if (!preview) {
        return NULL;
    }
    
    return preview->container;
}