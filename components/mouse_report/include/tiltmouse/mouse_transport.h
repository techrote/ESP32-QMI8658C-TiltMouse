#ifndef TILTMOUSE_MOUSE_TRANSPORT_H
#define TILTMOUSE_MOUSE_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "tiltmouse/mouse_report.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TM_MOUSE_TRANSPORT_NONE = 0,
    TM_MOUSE_TRANSPORT_USB = 1,
    TM_MOUSE_TRANSPORT_ESPNOW = 2,
} tm_mouse_transport_id_t;

/* The backend copies/consumes the report synchronously on successful submit.
 * Submit acceptance is NOT a claim of delivery to the host or RF receiver. */
typedef struct {
    bool (*ready)(void *context);
    bool (*submit)(void *context, const tm_mouse_report_t *report);
    void *context;
} tm_mouse_transport_sink_t;

typedef enum {
    TM_MOUSE_MOTION_NONE = 0,
    TM_MOUSE_MOTION_ACCEPTED = 1,
    TM_MOUSE_MOTION_DROPPED = 2,
} tm_mouse_motion_result_t;

typedef enum {
    TM_MOUSE_STATE_UNCHANGED = 0,
    TM_MOUSE_STATE_BUTTONS_RETAINED = 1,
    TM_MOUSE_STATE_SAFE_RELEASE_RETAINED = 2,
} tm_mouse_state_result_t;

typedef enum {
    TM_MOUSE_REASON_OK = 0,
    TM_MOUSE_REASON_INACTIVE,
    TM_MOUSE_REASON_INVALID,
    TM_MOUSE_REASON_NOT_READY,
    TM_MOUSE_REASON_BUSY,
    TM_MOUSE_REASON_SUBMIT_FAILED,
    TM_MOUSE_REASON_EXPIRED,
    TM_MOUSE_REASON_RELEASE_FIRST,
} tm_mouse_publish_reason_t;

typedef struct {
    tm_mouse_motion_result_t motion;
    tm_mouse_state_result_t state;
    tm_mouse_publish_reason_t reason;
} tm_mouse_publish_result_t;

/* One owner, one in-flight report, bounded button state, NO motion queue.
 * Initialize the object to zero before the first activate. All functions
 * (including complete) must be called by one serialized owner. Asynchronous
 * transport callbacks must enqueue completion status for that owner. */
typedef struct {
    tm_mouse_transport_id_t selected;
    tm_mouse_transport_sink_t sink;
    uint8_t desired_buttons;
    uint8_t release_debt;
    uint8_t pending_buttons;
    uint8_t pending_clear_mask;
    uint32_t state_revision;
    uint32_t release_revision;
    uint32_t pending_state_revision;
    uint32_t pending_release_revision;
    uint64_t last_ticket;
    uint64_t pending_ticket;
    bool in_flight;
    bool dirty;
    bool fault_latched;
} tm_mouse_transport_t;

/* Activating/switching transport fences prior pending reports and starts with
 * a zero-motion release obligation on the newly selected sink. */
bool tm_mouse_transport_activate(
    tm_mouse_transport_t *transport, tm_mouse_transport_id_t id,
    tm_mouse_transport_sink_t sink);
void tm_mouse_transport_deactivate(tm_mouse_transport_t *transport);

/* A publication is terminal; never re-offer a candidate to recover movement.
 * deadline_us=0 disables motion expiry (for producers with bounded immediate
 * dispatch). Buttons are retained only if upstream verified their freshness. */
tm_mouse_publish_result_t tm_mouse_transport_publish(
    tm_mouse_transport_t *transport, tm_mouse_transport_id_t owner,
    const tm_mouse_report_t *report, uint64_t now_us, uint64_t deadline_us);

/* Service retained BUTTON STATE using only zero-motion reports. */
tm_mouse_publish_result_t tm_mouse_transport_service(
    tm_mouse_transport_t *transport, tm_mouse_transport_id_t owner);

/* Token is local transport metadata, never part of the logical report.
 * Completion may clear release debt, but cannot restore consumed movement. */
uint64_t tm_mouse_transport_pending_ticket(const tm_mouse_transport_t *transport);
bool tm_mouse_transport_complete(
    tm_mouse_transport_t *transport, uint64_t ticket, bool success);

#ifdef __cplusplus
}
#endif

#endif
