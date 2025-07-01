idf_component_register(
    SRCS "main.c" "input_handler.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_lcd lvgl oled_ui
)