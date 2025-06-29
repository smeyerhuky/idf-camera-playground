idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES camera_module sdcard_module time_sync manifest_manager audio_recorder nvs_flash
)