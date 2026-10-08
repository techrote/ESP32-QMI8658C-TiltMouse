#ifndef TILTMOUSE_MOUSE_REPORT_H
#define TILTMOUSE_MOUSE_REPORT_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Proposed API only. Not a wire struct. No sequence, session or USB types. */
enum { TM_REPORT_INVALID = 0, TM_REPORT_CURRENT = 1, TM_REPORT_RELEASE_ALL = 2 };
enum { TM_BUTTON_LEFT = 1, TM_BUTTON_RIGHT = 2, TM_BUTTON_MASK = 3 };
typedef struct {
    int8_t dx;
    int8_t dy;
    uint8_t buttons;
    uint8_t kind;
} tm_mouse_report_t;
/* Wide inputs are checked BEFORE conversion. Zero initialization is invalid. */
static inline bool tm_mouse_report_make(
    int dx, int dy, unsigned buttons, tm_mouse_report_t *out)
{
    if (out == 0) { return false; }
    const tm_mouse_report_t invalid = {0, 0, 0, TM_REPORT_INVALID};
    *out = invalid;
    if (dx < -127 || dx > 127 || dy < -127 || dy > 127 ||
        (buttons & ~((unsigned)TM_BUTTON_MASK)) != 0) { return false; }
    const tm_mouse_report_t current = {(int8_t)dx, (int8_t)dy,
                                      (uint8_t)buttons, TM_REPORT_CURRENT};
    *out = current;
    return true;
}
static inline tm_mouse_report_t tm_mouse_report_release(void)
{
    const tm_mouse_report_t release = {0, 0, 0, TM_REPORT_RELEASE_ALL};
    return release;
}
#ifdef __cplusplus
}
#endif
#endif
