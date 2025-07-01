idf_component_register(
    SRCS "camera_preview.c"
    INCLUDE_DIRS "include"
    REQUIRES esp32-camera lvgl_port
    PRIV_REQUIRES lvgl
)