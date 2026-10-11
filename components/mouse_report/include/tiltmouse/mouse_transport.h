#ifndef TILTMOUSE_MOUSE_TRANSPORT_H
#define TILTMOUSE_MOUSE_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "tiltmouse/mouse_report.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ACCEPTED = copied by one backend, NOT guaranteed host delivery.
 * Every publication consumes its integer movement, even on failure.
 */
typedef enum {
    TM_MOTION_NONE = 0,
    TM_MOTION_ACCEPTED,
    TM_MOTION_DROPPED,
} tm_motion_disposition_t;

typedef enum {
    TM_STATE_UNCHANGED = 0,
    TM_BUTTONS_RETAINED,
    TM_SAFE_RELEASE_RETAINED,
} tm_state_disposition_t;

typedef enum {
    TM_REASON_OK = 0,
    TM_REASON_NOT_READY,
    TM_REASON_SUBMIT_FAILED,
    TM_REASON_INVALID,
    TM_REASON_RELEASE_FIRST,
    TM_REASON_EXPIRED,
    TM_REASON_INACTIVE,
} tm_publish_reason_t;

typedef struct {
    tm_motion_disposition_t motion;
    tm_state_disposition_t state;
    tm_publish_reason_t reason;
} tm_publish_result_t;

typedef enum {
    TM_LINK_DOWN = 0,
    TM_LINK_READY,
    TM_LINK_BUSY,
    TM_LINK_QUIESCING,
} tm_link_state_t;

typedef struct {
    tm_link_state_t link;
    bool release_pending; /* Local debt, not proof of remote delivery. */
    bool quiesced;        /* Backend-local fence; never an RF ACK. */
} tm_transport_status_t;

typedef struct {
    void *ctx;
    tm_publish_result_t (*publish_consume)(
        void *ctx, const tm_mouse_report_t *report, uint64_t motion_deadline_us);
    void (*service)(void *ctx); /* Zero-motion state repair only. */
    void (*begin_quiesce)(void *ctx);
    tm_transport_status_t (*status)(void *ctx);
} tm_mouse_transport_t;

typedef enum {
    TM_ROUTE_USB = 1,
    TM_ROUTE_ESPNOW = 2,
} tm_route_id_t;

/* One output owner is chosen at initialization. No runtime fallback/switch
 * before TM-006/TM-007 establish transport fencing and rearm.
 * Never offer a consumed delta to a second router or transport.
 */
typedef struct {
    tm_mouse_transport_t transport;
    tm_route_id_t selected;
    bool initialized;
    bool accepting;
} tm_mouse_router_t;

bool tm_mouse_router_init(
    tm_mouse_router_t *router, tm_route_id_t selected,
    tm_mouse_transport_t transport);
tm_publish_result_t tm_mouse_router_publish(
    tm_mouse_router_t *router, tm_route_id_t requester,
    const tm_mouse_report_t *report, uint64_t motion_deadline_us);
void tm_mouse_router_service(tm_mouse_router_t *router);
void tm_mouse_router_begin_quiesce(tm_mouse_router_t *router);
tm_transport_status_t tm_mouse_router_status(const tm_mouse_router_t *router);

#ifdef __cplusplus
}
#endif

#endif
