#ifndef TILTMOUSE_MOUSE_REPORT_H
#define TILTMOUSE_MOUSE_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    TM_BUTTON_LEFT = 1u << 0,
    TM_BUTTON_RIGHT = 1u << 1,
    TM_BUTTON_MASK = TM_BUTTON_LEFT | TM_BUTTON_RIGHT,
};

typedef enum {
    TM_REPORT_INVALID = 0,
    TM_REPORT_CURRENT = 1,
    TM_REPORT_RELEASE_ALL = 2,
} tm_report_kind_t;

/* Logical input only: NOT a packed USB/ESP-NOW wire structure. */
typedef struct {
    int8_t dx;
    int8_t dy;
    uint8_t buttons;
    uint8_t kind;
} tm_mouse_report_t;

/* Checks the wide input BEFORE narrowing; failures leave out INVALID. */
bool tm_mouse_report_make(int dx, int dy, unsigned buttons, tm_mouse_report_t *out);
tm_mouse_report_t tm_mouse_report_release(void);
bool tm_mouse_report_valid(const tm_mouse_report_t *report);

#ifdef __cplusplus
}
#endif

#endif
