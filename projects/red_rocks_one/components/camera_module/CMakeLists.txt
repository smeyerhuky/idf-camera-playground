idf_component_register(
    SRCS "camera_module.c"
    INCLUDE_DIRS "include"
    REQUIRES esp32-camera
    PRIV_REQUIRES log freertos
)