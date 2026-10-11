#include "tiltmouse/mouse_report.h"

#include <stddef.h>

bool tm_mouse_report_make_current(
    int32_t dx, int32_t dy, uint32_t buttons, tm_mouse_report_t *out)
{
    if (out == NULL) {
        return false;
    }

    *out = (tm_mouse_report_t){0};
    if (dx < -127 || dx > 127 || dy < -127 || dy > 127 ||
        (buttons & ~((uint32_t)TM_MOUSE_BUTTON_ALL)) != 0u) {
        return false;
    }

    *out = (tm_mouse_report_t){
        .dx = (int8_t)dx,
        .dy = (int8_t)dy,
        .buttons = (uint8_t)buttons,
        .kind = TM_MOUSE_REPORT_CURRENT,
    };
    return true;
}

bool tm_mouse_report_make_release(tm_mouse_report_t *out)
{
    if (out == NULL) {
        return false;
    }

    *out = (tm_mouse_report_t){.kind = TM_MOUSE_REPORT_RELEASE_ALL};
    return true;
}

bool tm_mouse_report_is_valid(const tm_mouse_report_t *report)
{
    if (report == NULL) {
        return false;
    }

    if (report->kind == TM_MOUSE_REPORT_RELEASE_ALL) {
        return report->dx == 0 && report->dy == 0 && report->buttons == 0u;
    }

    return report->kind == TM_MOUSE_REPORT_CURRENT &&
           report->dx >= -127 && report->dy >= -127 &&
           (report->buttons & ~TM_MOUSE_BUTTON_ALL) == 0u;
}
