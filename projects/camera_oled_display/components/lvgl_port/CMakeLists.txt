idf_component_register(
    SRCS "lvgl_port.c"
    INCLUDE_DIRS "include"
    REQUIRES ssd1306_driver
    PRIV_REQUIRES lvgl
)