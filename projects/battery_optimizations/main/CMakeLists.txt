idf_component_register(
    SRCS 
        "main.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        nvs_flash
        esp_wifi
        esp_netif
        esp_http_server
        fatfs
        driver
        esp_timer
        esp_psram
        spi_flash
        esp_adc
        sdcard_module
        camera_module
)