idf_component_register(
    SRCS "avi_recorder.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common driver
    PRIV_REQUIRES sdcard_module camera_module
)