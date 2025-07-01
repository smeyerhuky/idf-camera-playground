idf_component_register(
    SRCS "waveshare_display.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common esp_driver_spi esp_driver_gpio esp_lcd
)