#ifndef TILTMOUSE_MOUSE_OUTPUT_H
#define TILTMOUSE_MOUSE_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "tiltmouse/mouse_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The adapter copies the report during submit and calls complete only from
 * the same serialized worker as publish/service, never directly from an
 * asynchronous USB/Wi-Fi callback.
 */
typedef struct {
    void *ctx;
    bool (*ready)(void *ctx);
    bool (*submit)(void *ctx, const tm_mouse_report_t *report);
    uint64_t (*now_us)(void *ctx);
} tm_mouse_output_backend_t;

/* Constant-space state: no queue or refund of integer mouse movement. */
typedef struct {
    tm_mouse_output_backend_t backend;
    uint8_t desired_buttons;
    uint8_t release_debt;
    uint8_t confirmed_buttons;
    uint8_t flight_debt;
    uint32_t revision;
    uint32_t flight_revision;
    uint64_t next_ticket;
    uint64_t flight_ticket;
    tm_mouse_report_t flight_report;
    bool confirmed_valid;
    bool in_flight;
    bool repair_needed;
    bool quiescing;
    bool initialized;
} tm_mouse_output_t;

bool tm_mouse_output_init(
    tm_mouse_output_t *output, tm_mouse_output_backend_t backend);
tm_publish_result_t tm_mouse_output_publish(
    tm_mouse_output_t *output, const tm_mouse_report_t *report,
    uint64_t motion_deadline_us);
void tm_mouse_output_service(tm_mouse_output_t *output);

/* A completion/failed event identifies the one accepted in-flight report.
 * Unknown, old or duplicate tickets are ignored. Failure never replays dx/dy.
 */
bool tm_mouse_output_complete(
    tm_mouse_output_t *output, uint64_t ticket, bool success);
bool tm_mouse_output_inflight(
    const tm_mouse_output_t *output, uint64_t *ticket, tm_mouse_report_t *report);

/* Record disconnect/suspend on the serialized worker and fence old tickets.
 * This is local cleanup, NOT a proof the old host has received a release.
 */
void tm_mouse_output_link_lost(tm_mouse_output_t *output);
void tm_mouse_output_begin_quiesce(tm_mouse_output_t *output);
tm_transport_status_t tm_mouse_output_status(const tm_mouse_output_t *output);
tm_mouse_transport_t tm_mouse_output_transport(tm_mouse_output_t *output);

#ifdef __cplusplus
}
#endif

#endif
