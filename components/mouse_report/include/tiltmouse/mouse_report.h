#ifndef TILTMOUSE_MOUSE_REPORT_H
#define TILTMOUSE_MOUSE_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    TM_MOUSE_BUTTON_LEFT = 1u << 0,
    TM_MOUSE_BUTTON_RIGHT = 1u << 1,
    TM_MOUSE_BUTTON_ALL = TM_MOUSE_BUTTON_LEFT | TM_MOUSE_BUTTON_RIGHT,
};

/* Invalid zero-initialized reports must never cause output. */
typedef enum {
    TM_MOUSE_REPORT_INVALID = 0,
    TM_MOUSE_REPORT_CURRENT = 1,
    TM_MOUSE_REPORT_RELEASE_ALL = 2,
} tm_mouse_report_kind_t;

/* Mouse semantics only; do not serialize this struct as a wire packet. */
typedef struct {
    int8_t dx;
    int8_t dy;
    uint8_t buttons;
    uint8_t kind;
} tm_mouse_report_t;

/* Out is made INVALID on any failure, including out-of-range input. */
bool tm_mouse_report_make_current(
    int32_t dx, int32_t dy, uint32_t buttons, tm_mouse_report_t *out);

/* Canonical zero-motion, all-buttons-up safety command. */
bool tm_mouse_report_make_release(tm_mouse_report_t *out);

bool tm_mouse_report_is_valid(const tm_mouse_report_t *report);

#ifdef __cplusplus
}
#endif

#endif
