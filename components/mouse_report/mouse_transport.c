#include "tiltmouse/mouse_transport.h"

#include <stddef.h>
#include <string.h>

static tm_mouse_publish_result_t result_default(void)
{
    return (tm_mouse_publish_result_t){
        .motion = TM_MOUSE_MOTION_NONE,
        .state = TM_MOUSE_STATE_UNCHANGED,
        .reason = TM_MOUSE_REASON_OK,
    };
}

bool tm_mouse_transport_activate(
    tm_mouse_transport_t *transport, tm_mouse_transport_id_t id,
    tm_mouse_transport_sink_t sink)
{
    if (transport == NULL || id == TM_MOUSE_TRANSPORT_NONE ||
        (id != TM_MOUSE_TRANSPORT_USB && id != TM_MOUSE_TRANSPORT_ESPNOW) ||
        sink.ready == NULL || sink.submit == NULL) {
        return false;
    }

    /* Never reuse a ticket across transport activations. */
    const uint64_t last_ticket = transport->last_ticket;
    memset(transport, 0, sizeof(*transport));
    transport->last_ticket = last_ticket;
    transport->selected = id;
    transport->sink = sink;
    transport->release_debt = TM_MOUSE_BUTTON_ALL;
    transport->release_revision = 1u;
    transport->state_revision = 1u;
    transport->dirty = true;
    return true;
}

void tm_mouse_transport_deactivate(tm_mouse_transport_t *transport)
{
    if (transport != NULL) {
        const uint64_t last_ticket = transport->last_ticket;
        memset(transport, 0, sizeof(*transport));
        transport->last_ticket = last_ticket;
    }
}

static tm_mouse_publish_result_t try_submit(
    tm_mouse_transport_t *transport, int8_t dx, int8_t dy,
    tm_mouse_publish_result_t result, bool force_report)
{
    if (transport->in_flight) {
        result.reason = TM_MOUSE_REASON_BUSY;
        return result;
    }
    if (!transport->sink.ready(transport->sink.context)) {
        result.reason = TM_MOUSE_REASON_NOT_READY;
        return result;
    }

    const bool barrier =
        (transport->desired_buttons & transport->release_debt) != 0u;
    const uint8_t buttons = barrier
        ? (uint8_t)(transport->desired_buttons & ~transport->release_debt)
        : transport->desired_buttons;
    if (barrier) {
        dx = 0;
        dy = 0;
        result.reason = TM_MOUSE_REASON_RELEASE_FIRST;
    }

    if (dx == 0 && dy == 0 && !force_report && !transport->dirty &&
        transport->release_debt == 0u) {
        return result;
    }

    const tm_mouse_report_t outgoing = {
        .dx = dx,
        .dy = dy,
        .buttons = buttons,
        .kind = TM_MOUSE_REPORT_CURRENT,
    };

    /* Reserve the ticket before submit; backends must not re-enter this owner.
     * No caller-owned report pointer is retained beyond submit(). */
    ++transport->last_ticket;
    if (transport->last_ticket == 0u) {
        ++transport->last_ticket;
    }
    transport->pending_ticket = transport->last_ticket;
    transport->pending_buttons = buttons;
    transport->pending_clear_mask =
        (uint8_t)(transport->release_debt & ~buttons);
    transport->pending_state_revision = transport->state_revision;
    transport->pending_release_revision = transport->release_revision;
    transport->in_flight = true;

    if (!transport->sink.submit(transport->sink.context, &outgoing)) {
        transport->in_flight = false;
        transport->dirty = true;
        result.reason = TM_MOUSE_REASON_SUBMIT_FAILED;
        return result;
    }

    if (dx != 0 || dy != 0) {
        result.motion = TM_MOUSE_MOTION_ACCEPTED;
    }
    return result;
}

tm_mouse_publish_result_t tm_mouse_transport_publish(
    tm_mouse_transport_t *transport, tm_mouse_transport_id_t owner,
    const tm_mouse_report_t *report, uint64_t now_us, uint64_t deadline_us)
{
    tm_mouse_publish_result_t result = result_default();

    /* Never mutate state, submit or fail over on a wrong-owner publication. */
    if (transport == NULL || owner == TM_MOUSE_TRANSPORT_NONE ||
        transport->selected != owner) {
        result.reason = TM_MOUSE_REASON_INACTIVE;
        result.motion = TM_MOUSE_MOTION_DROPPED;
        return result;
    }

    if (!tm_mouse_report_is_valid(report)) {
        result.motion = TM_MOUSE_MOTION_DROPPED;
        result.state = TM_MOUSE_STATE_SAFE_RELEASE_RETAINED;
        result.reason = TM_MOUSE_REASON_INVALID;

        if (!transport->fault_latched || transport->desired_buttons != 0u) {
            ++transport->release_revision;
            ++transport->state_revision;
        }
        transport->fault_latched = true;
        transport->desired_buttons = 0u;
        transport->release_debt = TM_MOUSE_BUTTON_ALL;
        transport->dirty = true;
        return try_submit(transport, 0, 0, result, true);
    }

    if (report->kind == TM_MOUSE_REPORT_RELEASE_ALL) {
        result.state = TM_MOUSE_STATE_SAFE_RELEASE_RETAINED;
        if (!transport->fault_latched || transport->desired_buttons != 0u) {
            ++transport->release_revision;
            ++transport->state_revision;
        }
        transport->fault_latched = true;
        transport->desired_buttons = 0u;
        transport->release_debt = TM_MOUSE_BUTTON_ALL;
        transport->dirty = true;
        return try_submit(transport, 0, 0, result, true);
    }

    const uint8_t previously_held = transport->desired_buttons;
    const uint8_t released =
        (uint8_t)(previously_held & ~report->buttons);
    if (released != 0u) {
        transport->release_debt |= released;
        ++transport->release_revision;
    }
    if (previously_held != report->buttons) {
        transport->dirty = true;
        ++transport->state_revision;
    }
    transport->fault_latched = false;
    transport->desired_buttons = report->buttons;
    result.state = TM_MOUSE_STATE_BUTTONS_RETAINED;

    if (report->dx != 0 || report->dy != 0) {
        result.motion = TM_MOUSE_MOTION_DROPPED;
    }

    if (deadline_us != 0u && now_us > deadline_us) {
        result.reason = TM_MOUSE_REASON_EXPIRED;
        return try_submit(transport, 0, 0, result, false);
    }

    return try_submit(transport, report->dx, report->dy, result, true);
}

tm_mouse_publish_result_t tm_mouse_transport_service(
    tm_mouse_transport_t *transport, tm_mouse_transport_id_t owner)
{
    tm_mouse_publish_result_t result = result_default();
    if (transport == NULL || owner == TM_MOUSE_TRANSPORT_NONE ||
        transport->selected != owner) {
        result.reason = TM_MOUSE_REASON_INACTIVE;
        return result;
    }

    result.state = transport->fault_latched
        ? TM_MOUSE_STATE_SAFE_RELEASE_RETAINED
        : TM_MOUSE_STATE_BUTTONS_RETAINED;
    return try_submit(transport, 0, 0, result, false);
}

uint64_t tm_mouse_transport_pending_ticket(const tm_mouse_transport_t *transport)
{
    return transport != NULL && transport->in_flight
        ? transport->pending_ticket : 0u;
}

bool tm_mouse_transport_complete(
    tm_mouse_transport_t *transport, uint64_t ticket, bool success)
{
    if (transport == NULL || !transport->in_flight || ticket == 0u ||
        ticket != transport->pending_ticket) {
        return false;
    }

    transport->in_flight = false;
    if (success &&
        transport->pending_release_revision == transport->release_revision) {
        transport->release_debt &=
            (uint8_t)~transport->pending_clear_mask;
    }

    if (success &&
        transport->pending_state_revision == transport->state_revision &&
        transport->pending_buttons == transport->desired_buttons &&
        transport->release_debt == 0u) {
        transport->dirty = false;
    } else {
        transport->dirty = true;
    }

    return true;
}
