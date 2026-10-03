#pragma once

/*
 * Logical ordering for the seven ESP32-S3 touch channels used by TiltMouse.
 * Channel IDs/GPIOs are 1..7 in the same order; GPIO4 is diagnostic only.
 */
enum {
    TILTMOUSE_TOUCH_LEFT_1 = 0,
    TILTMOUSE_TOUCH_LEFT_2 = 1,
    TILTMOUSE_TOUCH_LEFT_3 = 2,
    TILTMOUSE_TOUCH_REFERENCE = 3,
    TILTMOUSE_TOUCH_RIGHT_1 = 4,
    TILTMOUSE_TOUCH_RIGHT_2 = 5,
    TILTMOUSE_TOUCH_RIGHT_3 = 6,
    TILTMOUSE_TOUCH_CHANNEL_COUNT = 7,
};
