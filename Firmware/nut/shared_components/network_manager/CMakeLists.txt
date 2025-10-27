idf_component_register(
    SRCS "network_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES
        wifi_manager
        ethernet_init
        esp_netif
        esp_eth
        esp_event
)
