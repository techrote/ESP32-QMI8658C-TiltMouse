#include "tiltmouse/mouse_report.h"

#include <stddef.h>

bool tm_mouse_report_make(int dx, int dy, unsigned buttons, tm_mouse_report_t *out)
{
    if (out == NULL) {
        return false;
    }

    *out = (tm_mouse_report_t){0, 0, 0, TM_REPORT_INVALID};
    if (dx < -127 || dx > 127 || dy < -127 || dy > 127 ||
        (buttons & ~(unsigned)TM_BUTTON_MASK) != 0u) {
        return false;
    }

    *out = (tm_mouse_report_t){
        .dx = (int8_t)dx,
        .dy = (int8_t)dy,
        .buttons = (uint8_t)buttons,
        .kind = TM_REPORT_CURRENT,
    };
    return true;
}

tm_mouse_report_t tm_mouse_report_release(void)
{
    return (tm_mouse_report_t){0, 0, 0, TM_REPORT_RELEASE_ALL};
}

bool tm_mouse_report_valid(const tm_mouse_report_t *report)
{
    if (report == NULL || (report->buttons & (uint8_t)~TM_BUTTON_MASK) != 0u) {
        return false;
    }

    if (report->kind == TM_REPORT_RELEASE_ALL) {
        return report->dx == 0 && report->dy == 0 && report->buttons == 0;
    }
    if (report->kind != TM_REPORT_CURRENT) {
        return false;
    }

    return report->dx >= -127 && report->dy >= -127;
}
