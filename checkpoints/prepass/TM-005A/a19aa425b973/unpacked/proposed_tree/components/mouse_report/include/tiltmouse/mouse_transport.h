#ifndef TILTMOUSE_MOUSE_TRANSPORT_H
#define TILTMOUSE_MOUSE_TRANSPORT_H
#include "tiltmouse/mouse_report.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Every publish is TERMINAL for its delta, including rejected/dropped input.
 * ACCEPTED means copied/owned by this transport, NOT delivered to the host.
 * NONE means canonical input had no movement; DROPPED never implies retry.
 */
typedef enum { TM_MOTION_NONE, TM_MOTION_ACCEPTED, TM_MOTION_DROPPED }
    tm_motion_disposition_t;
typedef enum { TM_STATE_UNCHANGED, TM_BUTTONS_RETAINED, TM_SAFE_RELEASE_RETAINED }
    tm_state_disposition_t;
typedef enum {
    TM_REASON_OK, TM_REASON_NOT_READY, TM_REASON_SUBMIT_FAILED,
    TM_REASON_INVALID, TM_REASON_RELEASE_FIRST, TM_REASON_EXPIRED,
    TM_REASON_INACTIVE
} tm_publish_reason_t;
typedef struct {
    tm_motion_disposition_t motion;
    tm_state_disposition_t state;
    tm_publish_reason_t reason;
} tm_publish_result_t;
typedef enum { TM_LINK_DOWN, TM_LINK_READY, TM_LINK_BUSY, TM_LINK_QUIESCING }
    tm_link_state_t;
typedef struct {
    tm_link_state_t link;
    bool release_pending;     /* Local obligation, not end-to-end proof. */
    bool quiesced;            /* No old transfer can subsequently emit. */
} tm_transport_status_t;
typedef struct {
    void *ctx;
    /* Uses backend's injected monotonic clock. Deadline is local scheduling
     * information, not a wire timestamp. Caller separately checks source ages.
     * Copies synchronously; never retains the caller's pointer.
     * NULL/invalid on an active transport latches safe release, never old motion.
     */
    tm_publish_result_t (*publish_consume)(void *ctx,
        const tm_mouse_report_t *report, uint64_t motion_deadline_us);
    /* Periodic, bounded: only ZERO motion using retained state/release debt. */
    void (*service)(void *ctx);
    /* Stops normal input; drains or cancels accepted work, prioritizes neutral.
     * May remain non-quiesced indefinitely if the backend cannot fence safely.
     */
    void (*begin_quiesce)(void *ctx);
    tm_transport_status_t (*status)(void *ctx);
} tm_mouse_transport_t;
#ifdef __cplusplus
}
#endif
#endif
