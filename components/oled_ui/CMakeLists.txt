idf_component_register(
    SRCS "oled_ui.c" "ssd1306_driver.c" "camera_control_ui.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd lvgl
    PRIV_REQUIRES esp_timer
)