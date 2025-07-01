idf_component_register(
    SRCS "manifest_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES json
    PRIV_REQUIRES sdcard_module
)