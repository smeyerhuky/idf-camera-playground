idf_component_register(
    SRCS 
        "esp_mesh_audio.c"
        "mesh_protocol.c"
        "node_discovery.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        esp_wifi
        esp_netif
        esp_now
        esp_timer
    PRIV_REQUIRES
        nvs_flash
)