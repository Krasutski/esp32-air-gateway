idf_component_register (
    SRCS
        bsp_battery.c
        bsp_modem.c
    INCLUDE_DIRS
        include
    REQUIRES
        esp_netif
        esp_modem
        esp_adc

)
