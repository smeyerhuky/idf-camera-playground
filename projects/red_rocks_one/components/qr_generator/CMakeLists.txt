idf_component_register(
    SRCS "qr_generator.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common esp_http_server
)