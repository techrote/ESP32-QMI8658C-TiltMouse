#include "tiltmouse/indicator_frame.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static int offset(float angle)
{
    if (angle < -15.0f) {
        angle = -15.0f;
    } else if (angle > 15.0f) {
        angle = 15.0f;
    }
    return (int)lroundf(angle / 5.0f);
}

void tm_indicator_frame(float x_deg, float y_deg, bool ready,
                        uint8_t rgb[TM_INDICATOR_BYTES])
{
    if (rgb == NULL) {
        return;
    }
    memset(rgb, 0, TM_INDICATOR_BYTES);
    ready = ready && isfinite(x_deg) && isfinite(y_deg);
    const int x = 3 + (ready ? offset(x_deg) : 0);
    const int y = 3 + (ready ? offset(y_deg) : 0);
    /* ESPsand qualified index=y*8+x with USB up. Our USB-down grip rotates
     * that physical reference 180 degrees. Seven cells give symmetric offsets. */
    const size_t pixel = (size_t)(7 - y) * 8u + (size_t)(7 - x);
    rgb[pixel * 3u + (ready ? 1u : 2u)] = 1u;
}
