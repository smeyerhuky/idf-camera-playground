idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES camera_module sdcard_module time_sync qr_generator waveshare_display 
             esp_wifi esp_http_server esp_event nvs_flash
)