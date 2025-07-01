#ifndef CAMERA_CONTROL_UI_H
#define CAMERA_CONTROL_UI_H

#include "oled_ui.h"
#include "lvgl.h"

typedef enum {
    CAMERA_MODE_PHOTO = 0,
    CAMERA_MODE_VIDEO,
    CAMERA_MODE_TIMELAPSE,
    CAMERA_MODE_MAX
} camera_mode_t;

typedef enum {
    MENU_MAIN = 0,
    MENU_CAMERA_SETTINGS,
    MENU_VIDEO_SETTINGS,
    MENU_AUDIO_SETTINGS,
    MENU_SYSTEM_SETTINGS,
    MENU_MAX
} menu_screen_t;

typedef struct {
    int brightness;
    int contrast;
    int saturation;
    int exposure;
    int jpeg_quality;
    camera_mode_t mode;
    bool microphone_enabled;
} camera_settings_t;

void camera_control_ui_init(void);
void camera_control_ui_handle_event(const ui_event_t *event);
void camera_control_ui_update_status(void);
camera_settings_t* camera_control_ui_get_settings(void);

#endif