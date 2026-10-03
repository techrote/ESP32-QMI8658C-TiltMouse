#include <assert.h>
#include <stddef.h>

#include "tiltmouse/board_pins.h"

static void assert_unique(const int *values, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            assert(values[i] != values[j]);
        }
    }
}

int main(void)
{
    static const int touch_pins[] = {
        TILTMOUSE_GPIO_TOUCH_LEFT_1,
        TILTMOUSE_GPIO_TOUCH_LEFT_2,
        TILTMOUSE_GPIO_TOUCH_LEFT_3,
        TILTMOUSE_GPIO_TOUCH_REFERENCE,
        TILTMOUSE_GPIO_TOUCH_RIGHT_1,
        TILTMOUSE_GPIO_TOUCH_RIGHT_2,
        TILTMOUSE_GPIO_TOUCH_RIGHT_3,
    };

    assert(TILTMOUSE_GPIO_BOOT == 0);

    for (size_t i = 0; i < sizeof(touch_pins) / sizeof(touch_pins[0]); ++i) {
        assert(touch_pins[i] == (int)i + 1);
    }
    assert_unique(touch_pins, sizeof(touch_pins) / sizeof(touch_pins[0]));

    assert(TILTMOUSE_GPIO_IMU_INT1 == 10);
    assert(TILTMOUSE_GPIO_IMU_SDA == 11);
    assert(TILTMOUSE_GPIO_IMU_SCL == 12);
    assert(TILTMOUSE_GPIO_IMU_INT2 == 13);
    assert(TILTMOUSE_GPIO_RGB_DATA == 14);
    assert(TILTMOUSE_GPIO_USB_D_MINUS == 19);
    assert(TILTMOUSE_GPIO_USB_D_PLUS == 20);

    return 0;
}
