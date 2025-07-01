#include "camera_control_ui.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "CAMERA_UI";

static camera_settings_t camera_settings = {
    .brightness = 0,
    .contrast = 0,
    .saturation = 0,
    .exposure = 0,
    .jpeg_quality = 50,
    .mode = CAMERA_MODE_PHOTO,
    .microphone_enabled = true
};

static menu_screen_t current_menu = MENU_MAIN;
static int selected_item = 0;
static bool in_setting_mode = false;

static lv_obj_t *main_screen;
static lv_obj_t *header_label;
static lv_obj_t *menu_list;
static lv_obj_t *status_bar;

static const char* camera_mode_names[] = {
    "Photo",
    "Video", 
    "Timelapse"
};

static const char* main_menu_items[] = {
    "Take Photo/Video",
    "Camera Settings",
    "Video Settings", 
    "Audio Settings",
    "System Settings"
};

static const char* camera_menu_items[] = {
    "< Back",
    "Brightness",
    "Contrast", 
    "Saturation",
    "Exposure",
    "JPEG Quality"
};

static const char* video_menu_items[] = {
    "< Back",
    "Resolution",
    "Frame Rate",
    "Bitrate",
    "Duration"
};

static const char* audio_menu_items[] = {
    "< Back",
    "Microphone",
    "Volume",
    "Audio Format"
};

static void update_header(void)
{
    char header_text[64];
    
    switch (current_menu) {
        case MENU_MAIN:
            snprintf(header_text, sizeof(header_text), "Camera UI - %s", camera_mode_names[camera_settings.mode]);
            break;
        case MENU_CAMERA_SETTINGS:
            strcpy(header_text, "Camera Settings");
            break;
        case MENU_VIDEO_SETTINGS:
            strcpy(header_text, "Video Settings");
            break;
        case MENU_AUDIO_SETTINGS:
            strcpy(header_text, "Audio Settings");
            break;
        case MENU_SYSTEM_SETTINGS:
            strcpy(header_text, "System Settings");
            break;
        default:
            strcpy(header_text, "Camera UI");
            break;
    }
    
    lv_label_set_text(header_label, header_text);
}

static void update_menu_list(void)
{
    lv_obj_clean(menu_list);
    
    const char **items = NULL;
    int item_count = 0;
    
    switch (current_menu) {
        case MENU_MAIN:
            items = main_menu_items;
            item_count = sizeof(main_menu_items) / sizeof(main_menu_items[0]);
            break;
        case MENU_CAMERA_SETTINGS:
            items = camera_menu_items;
            item_count = sizeof(camera_menu_items) / sizeof(camera_menu_items[0]);
            break;
        case MENU_VIDEO_SETTINGS:
            items = video_menu_items;
            item_count = sizeof(video_menu_items) / sizeof(video_menu_items[0]);
            break;
        case MENU_AUDIO_SETTINGS:
            items = audio_menu_items;
            item_count = sizeof(audio_menu_items) / sizeof(audio_menu_items[0]);
            break;
        default:
            return;
    }
    
    for (int i = 0; i < item_count; i++) {
        lv_obj_t *label = lv_label_create(menu_list);
        char item_text[64];
        
        if (current_menu == MENU_CAMERA_SETTINGS && i > 0) {
            int value = 0;
            switch (i) {
                case 1: value = camera_settings.brightness; break;
                case 2: value = camera_settings.contrast; break;
                case 3: value = camera_settings.saturation; break;
                case 4: value = camera_settings.exposure; break;
                case 5: value = camera_settings.jpeg_quality; break;
            }
            snprintf(item_text, sizeof(item_text), "%s %s: %d", 
                    (i == selected_item) ? ">" : " ", items[i], value);
        } else if (current_menu == MENU_AUDIO_SETTINGS && i == 1) {
            snprintf(item_text, sizeof(item_text), "%s %s: %s", 
                    (i == selected_item) ? ">" : " ", items[i], 
                    camera_settings.microphone_enabled ? "ON" : "OFF");
        } else {
            snprintf(item_text, sizeof(item_text), "%s %s", 
                    (i == selected_item) ? ">" : " ", items[i]);
        }
        
        lv_label_set_text(label, item_text);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(label, 0, i * 10);
    }
}

static void update_status_bar(void)
{
    char status_text[32];
    snprintf(status_text, sizeof(status_text), "Mic:%s Qual:%d", 
            camera_settings.microphone_enabled ? "ON" : "OFF",
            camera_settings.jpeg_quality);
    lv_label_set_text(status_bar, status_text);
}

void camera_control_ui_init(void)
{
    main_screen = lv_obj_create(NULL);
    lv_scr_load(main_screen);
    
    header_label = lv_label_create(main_screen);
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(header_label, 0, 0);
    lv_obj_set_size(header_label, 128, 12);
    lv_obj_set_style_bg_color(header_label, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(header_label, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(header_label, lv_color_black(), 0);
    
    menu_list = lv_obj_create(main_screen);
    lv_obj_set_pos(menu_list, 0, 14);
    lv_obj_set_size(menu_list, 128, 40);
    lv_obj_set_style_bg_opa(menu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(menu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(menu_list, 0, 0);
    
    status_bar = lv_label_create(main_screen);
    lv_obj_set_style_text_font(status_bar, &lv_font_montserrat_8, 0);
    lv_obj_set_pos(status_bar, 0, 56);
    lv_obj_set_size(status_bar, 128, 8);
    
    update_header();
    update_menu_list();
    update_status_bar();
    
    ESP_LOGI(TAG, "Camera control UI initialized");
}

static void navigate_up(void)
{
    if (selected_item > 0) {
        selected_item--;
        update_menu_list();
    }
}

static void navigate_down(void)
{
    int max_items = 0;
    switch (current_menu) {
        case MENU_MAIN:
            max_items = sizeof(main_menu_items) / sizeof(main_menu_items[0]);
            break;
        case MENU_CAMERA_SETTINGS:
            max_items = sizeof(camera_menu_items) / sizeof(camera_menu_items[0]);
            break;
        case MENU_VIDEO_SETTINGS:
            max_items = sizeof(video_menu_items) / sizeof(video_menu_items[0]);
            break;
        case MENU_AUDIO_SETTINGS:
            max_items = sizeof(audio_menu_items) / sizeof(audio_menu_items[0]);
            break;
    }
    
    if (selected_item < max_items - 1) {
        selected_item++;
        update_menu_list();
    }
}

static void handle_select(void)
{
    switch (current_menu) {
        case MENU_MAIN:
            switch (selected_item) {
                case 0:
                    ESP_LOGI(TAG, "Taking photo/video in %s mode", camera_mode_names[camera_settings.mode]);
                    break;
                case 1:
                    current_menu = MENU_CAMERA_SETTINGS;
                    selected_item = 0;
                    break;
                case 2:
                    current_menu = MENU_VIDEO_SETTINGS;
                    selected_item = 0;
                    break;
                case 3:
                    current_menu = MENU_AUDIO_SETTINGS;
                    selected_item = 0;
                    break;
                case 4:
                    current_menu = MENU_SYSTEM_SETTINGS;
                    selected_item = 0;
                    break;
            }
            break;
            
        case MENU_CAMERA_SETTINGS:
        case MENU_VIDEO_SETTINGS:
        case MENU_AUDIO_SETTINGS:
            if (selected_item == 0) {
                current_menu = MENU_MAIN;
                selected_item = 0;
            } else {
                in_setting_mode = !in_setting_mode;
                ESP_LOGI(TAG, "%s setting mode", in_setting_mode ? "Entering" : "Exiting");
            }
            break;
    }
    
    update_header();
    update_menu_list();
    update_status_bar();
}

static void adjust_setting(int direction)
{
    if (!in_setting_mode) return;
    
    if (current_menu == MENU_CAMERA_SETTINGS) {
        switch (selected_item) {
            case 1:
                camera_settings.brightness = LV_CLAMP(-100, camera_settings.brightness + direction * 10, 100);
                break;
            case 2:
                camera_settings.contrast = LV_CLAMP(-100, camera_settings.contrast + direction * 10, 100);
                break;
            case 3:
                camera_settings.saturation = LV_CLAMP(-100, camera_settings.saturation + direction * 10, 100);
                break;
            case 4:
                camera_settings.exposure = LV_CLAMP(-100, camera_settings.exposure + direction * 10, 100);
                break;
            case 5:
                camera_settings.jpeg_quality = LV_CLAMP(10, camera_settings.jpeg_quality + direction * 5, 100);
                break;
        }
    } else if (current_menu == MENU_AUDIO_SETTINGS && selected_item == 1) {
        camera_settings.microphone_enabled = !camera_settings.microphone_enabled;
    }
    
    update_menu_list();
    update_status_bar();
}

void camera_control_ui_handle_event(const ui_event_t *event)
{
    if (event == NULL) return;
    
    switch (event->type) {
        case UI_EVENT_BUTTON_PRESSED:
            handle_select();
            break;
            
        case UI_EVENT_ENCODER_TURNED:
            if (in_setting_mode) {
                adjust_setting(event->value);
            } else {
                if (event->value > 0) {
                    navigate_down();
                } else {
                    navigate_up();
                }
            }
            break;
            
        case UI_EVENT_LONG_PRESS:
            camera_settings.mode = (camera_settings.mode + 1) % CAMERA_MODE_MAX;
            update_header();
            ESP_LOGI(TAG, "Switched to %s mode", camera_mode_names[camera_settings.mode]);
            break;
            
        default:
            break;
    }
}

void camera_control_ui_update_status(void)
{
    update_status_bar();
}

camera_settings_t* camera_control_ui_get_settings(void)
{
    return &camera_settings;
}