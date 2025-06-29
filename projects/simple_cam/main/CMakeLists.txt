idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES camera_module sdcard_module nvs_flash
)