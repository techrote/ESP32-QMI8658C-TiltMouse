idf_component_register(
    SRCS "touch_input_esp.c"
    INCLUDE_DIRS "include"
    REQUIRES board esp_driver_touch_sens esp_timer
)
