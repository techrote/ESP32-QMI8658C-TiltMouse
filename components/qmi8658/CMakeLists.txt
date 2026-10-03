idf_component_register(
    SRCS
        "qmi8658.c"
        "qmi8658_esp_idf.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES
        board
        esp_driver_i2c
        esp_timer
        freertos
)
