#pragma once

/*
 * Waveshare ESP32-S3-Matrix board resource contract.
 *
 * Keep this header ESP-IDF-independent so host tests can validate the pin map
 * without a target SDK. Do not change these values without updating RAG.md and
 * checking the Waveshare schematic.
 */
enum {
    TILTMOUSE_GPIO_BOOT = 0,

    TILTMOUSE_GPIO_TOUCH_LEFT_1 = 1,
    TILTMOUSE_GPIO_TOUCH_LEFT_2 = 2,
    TILTMOUSE_GPIO_TOUCH_LEFT_3 = 3,
    TILTMOUSE_GPIO_TOUCH_REFERENCE = 4,
    TILTMOUSE_GPIO_TOUCH_RIGHT_1 = 5,
    TILTMOUSE_GPIO_TOUCH_RIGHT_2 = 6,
    TILTMOUSE_GPIO_TOUCH_RIGHT_3 = 7,

    TILTMOUSE_GPIO_IMU_INT1 = 10,
    TILTMOUSE_GPIO_IMU_SDA = 11,
    TILTMOUSE_GPIO_IMU_SCL = 12,
    TILTMOUSE_GPIO_IMU_INT2 = 13,

    TILTMOUSE_GPIO_RGB_DATA = 14,

    TILTMOUSE_GPIO_USB_D_MINUS = 19,
    TILTMOUSE_GPIO_USB_D_PLUS = 20,
};
