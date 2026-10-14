#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tiltmouse/indicator_frame.h"

static void check(float x, float y, bool ready, size_t pixel, size_t colour)
{
    struct {
        uint8_t before;
        uint8_t rgb[TM_INDICATOR_BYTES];
        uint8_t after;
    } frame;
    memset(&frame, 0xa5, sizeof(frame));
    tm_indicator_frame(x, y, ready, frame.rgb);
    assert(frame.before == 0xa5 && frame.after == 0xa5);
    unsigned total = 0;
    for (size_t i = 0; i < sizeof(frame.rgb); ++i) {
        assert(frame.rgb[i] <= 1u);
        total += frame.rgb[i];
    }
    assert(total == 1u);
    assert(frame.rgb[pixel * 3u + colour] == 1u);
}

int main(void)
{
    /* Pending and invalid input must not display a misleading tilt. */
    check(100.0f, -100.0f, false, 36, 2);
    check(NAN, 0.0f, true, 36, 2);
    check(0.0f, INFINITY, true, 36, 2);
    check(0.0f, 0.0f, true, 36, 1);
    check(0.25f, -0.25f, true, 36, 1);
    /* USB-toward-operator physical view rotates ESPsand's USB-up chain. */
    check(5.0f, 0.0f, true, 35, 1);
    check(-5.0f, 0.0f, true, 37, 1);
    check(0.0f, 5.0f, true, 28, 1);
    check(0.0f, -5.0f, true, 44, 1);
    check(1e30f, 1e30f, true, 9, 1);
    check(-1e30f, -1e30f, true, 63, 1);
    /* Sweep representative angles to enforce one-code aggregate brightness. */
    for (int x = -30; x <= 30; ++x) {
        for (int y = -30; y <= 30; ++y) {
            uint8_t rgb[TM_INDICATOR_BYTES];
            tm_indicator_frame((float)x, (float)y, true, rgb);
            unsigned total = 0;
            for (size_t i = 0; i < sizeof(rgb); ++i) {
                assert(rgb[i] <= 1u);
                total += rgb[i];
            }
            assert(total == 1u);
        }
    }
    return 0;
}
