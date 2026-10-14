#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    TM_INDICATOR_PIXELS = 64,
    TM_INDICATOR_BYTES = TM_INDICATOR_PIXELS * 3,
};

/* RGB row-major chain; display grip is LED face up, USB toward the operator.
 * Inputs are recentered angles with the same source/sign as the mouse axes.
 * At most one pixel/channel has code 1; pending/invalid input is centre blue. */
void tm_indicator_frame(float x_deg, float y_deg, bool ready,
                        uint8_t rgb[TM_INDICATOR_BYTES]);
