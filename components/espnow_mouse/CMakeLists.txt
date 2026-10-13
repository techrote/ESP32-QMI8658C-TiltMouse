idf_component_register(
    SRCS "espnow_codec.c" "espnow_mouse.c"
    INCLUDE_DIRS "include"
    REQUIRES mouse_report esp_wifi esp_event esp_hw_support mbedtls nvs_flash
)
