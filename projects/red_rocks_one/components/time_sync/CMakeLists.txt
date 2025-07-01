idf_component_register(
    SRCS "time_sync.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi nvs_flash lwip
    PRIV_REQUIRES esp_netif esp_event
)