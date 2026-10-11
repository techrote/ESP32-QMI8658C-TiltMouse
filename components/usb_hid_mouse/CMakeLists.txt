idf_component_register(
    SRCS
        "usb_hid_mouse.c"
        "usb_hid_mouse_report.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_tinyusb mouse_report
)
