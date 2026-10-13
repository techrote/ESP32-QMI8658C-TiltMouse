idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        board app_pipeline qmi8658 orientation mouse_motion
        touch_input touch_buttons usb_hid_mouse espnow_mouse
        esp_timer esp_driver_gpio
)
