#ifndef CAMERA_PREVIEW_H
#define CAMERA_PREVIEW_H

#include "esp_err.h"
#include "esp_camera.h"
#include "lvgl.h"

#define CAMERA_PREVIEW_WIDTH  128
#define CAMERA_PREVIEW_HEIGHT 64

typedef struct {
    lv_obj_t *container;
    lv_obj_t *canvas;
    lv_obj_t *info_label;
    lv_canvas_buf_t cbuf;
    bool running;
    camera_fb_t *last_frame;
} camera_preview_t;

typedef struct {
    uint16_t preview_width;
    uint16_t preview_height;
    bool show_info;
    uint32_t refresh_rate_ms;
} camera_preview_config_t;

esp_err_t camera_preview_init(camera_preview_t *preview, const camera_preview_config_t *config);
esp_err_t camera_preview_deinit(camera_preview_t *preview);
esp_err_t camera_preview_start(camera_preview_t *preview);
esp_err_t camera_preview_stop(camera_preview_t *preview);
esp_err_t camera_preview_update_frame(camera_preview_t *preview);
lv_obj_t *camera_preview_get_container(camera_preview_t *preview);

#endif