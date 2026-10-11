#include "tiltmouse/mouse_output.h"

#include <stddef.h>

static void latch_safe_release(tm_mouse_output_t *output)
{
    if (output->desired_buttons != 0u || output->release_debt != TM_BUTTON_MASK) {
        ++output->revision;
    }
    output->desired_buttons = 0;
    output->release_debt = TM_BUTTON_MASK;
}

static void update_desired(tm_mouse_output_t *output, uint8_t buttons)
{
    if (output->desired_buttons != buttons) {
        const uint8_t falling =
            (uint8_t)(output->desired_buttons & (uint8_t)~buttons);
        output->release_debt |= falling;
        if (output->release_debt != 0u) {
            ++output->revision;
        }
        output->desired_buttons = buttons;
    }
}

bool tm_mouse_output_init(
    tm_mouse_output_t *output, tm_mouse_output_backend_t backend)
{
    if (output == NULL || backend.ctx == NULL || backend.ready == NULL ||
        backend.submit == NULL || backend.now_us == NULL) {
        return false;
    }

    *output = (tm_mouse_output_t){0};
    output->backend = backend;
    output->initialized = true;
    return true;
}

static tm_publish_reason_t submit_report(
    tm_mouse_output_t *output, uint8_t buttons, int8_t dx, int8_t dy)
{
    if (output->in_flight || !output->backend.ready(output->backend.ctx)) {
        return TM_REASON_NOT_READY;
    }

    const tm_mouse_report_t tx = {dx, dy, buttons, TM_REPORT_CURRENT};
    if (!output->backend.submit(output->backend.ctx, &tx)) {
        output->repair_needed = true;
        return TM_REASON_SUBMIT_FAILED;
    }

    output->in_flight = true;
    output->flight_report = tx;
    output->flight_debt = output->release_debt;
    output->flight_revision = output->revision;
    output->flight_ticket = ++output->next_ticket;
    return TM_REASON_OK;
}

tm_publish_result_t tm_mouse_output_publish(
    tm_mouse_output_t *output, const tm_mouse_report_t *report,
    uint64_t motion_deadline_us)
{
    tm_publish_result_t result = {
        TM_MOTION_DROPPED, TM_STATE_UNCHANGED, TM_REASON_INACTIVE
    };
    if (output == NULL || !output->initialized || output->quiescing) {
        return result;
    }

    if (!tm_mouse_report_valid(report) || report->kind == TM_REPORT_RELEASE_ALL) {
        latch_safe_release(output);
        result.state = TM_SAFE_RELEASE_RETAINED;
        result.motion = report != NULL && tm_mouse_report_valid(report) &&
                        report->kind == TM_REPORT_RELEASE_ALL
                            ? TM_MOTION_NONE : TM_MOTION_DROPPED;
        result.reason = !tm_mouse_report_valid(report) ? TM_REASON_INVALID : TM_REASON_OK;
        const tm_publish_reason_t submit =
            submit_report(output, 0, 0, 0);
        if (submit != TM_REASON_OK) {
            result.reason = submit;
        }
        return result;
    }

    update_desired(output, report->buttons);
    result.state = TM_BUTTONS_RETAINED;
    const bool has_motion = report->dx != 0 || report->dy != 0;
    result.motion = has_motion ? TM_MOTION_DROPPED : TM_MOTION_NONE;

    /* A pending release of a re-pressed bit must reach the host before
     * reassertion; the barrier never carries movement.
     */
    const bool release_first =
        (output->desired_buttons & output->release_debt) != 0u;
    const bool expired = has_motion && motion_deadline_us != 0u &&
                         output->backend.now_us(output->backend.ctx) >
                             motion_deadline_us;
    const int8_t dx = release_first || expired ? 0 : report->dx;
    const int8_t dy = release_first || expired ? 0 : report->dy;
    const uint8_t buttons = (uint8_t)(
        output->desired_buttons & (uint8_t)~output->release_debt);

    const tm_publish_reason_t submit = submit_report(output, buttons, dx, dy);
    result.reason = submit;
    if (submit == TM_REASON_OK) {
        if (dx != 0 || dy != 0) {
            result.motion = TM_MOTION_ACCEPTED;
        }
        if (release_first) {
            result.reason = TM_REASON_RELEASE_FIRST;
        } else if (expired) {
            result.reason = TM_REASON_EXPIRED;
        }
    }
    return result;
}

void tm_mouse_output_service(tm_mouse_output_t *output)
{
    if (output == NULL || !output->initialized || output->in_flight) {
        return;
    }

    if (output->release_debt != 0u || !output->confirmed_valid ||
        output->confirmed_buttons != output->desired_buttons ||
        output->repair_needed) {
        (void)submit_report(
            output,
            (uint8_t)(output->desired_buttons & (uint8_t)~output->release_debt),
            0, 0);
    }
}

bool tm_mouse_output_complete(
    tm_mouse_output_t *output, uint64_t ticket, bool success)
{
    if (output == NULL || !output->initialized || !output->in_flight ||
        ticket != output->flight_ticket) {
        return false;
    }

    output->in_flight = false;
    if (!success) {
        output->repair_needed = true;
        return true;
    }

    output->confirmed_valid = true;
    output->confirmed_buttons = output->flight_report.buttons;
    output->repair_needed = false;

    if (output->revision == output->flight_revision) {
        const uint8_t released = (uint8_t)(
            output->flight_debt & (uint8_t)~output->flight_report.buttons);
        output->release_debt &= (uint8_t)~released;
    }
    return true;
}

bool tm_mouse_output_inflight(
    const tm_mouse_output_t *output, uint64_t *ticket, tm_mouse_report_t *report)
{
    if (output == NULL || !output->initialized || !output->in_flight) {
        return false;
    }
    if (ticket != NULL) {
        *ticket = output->flight_ticket;
    }
    if (report != NULL) {
        *report = output->flight_report;
    }
    return true;
}

void tm_mouse_output_link_lost(tm_mouse_output_t *output)
{
    if (output == NULL || !output->initialized) {
        return;
    }

    output->in_flight = false;
    output->confirmed_valid = false;
    output->repair_needed = true;
    latch_safe_release(output);
}

void tm_mouse_output_begin_quiesce(tm_mouse_output_t *output)
{
    if (output != NULL && output->initialized && !output->quiescing) {
        output->quiescing = true;
        latch_safe_release(output);
    }
}

tm_transport_status_t tm_mouse_output_status(const tm_mouse_output_t *output)
{
    tm_transport_status_t status = {TM_LINK_DOWN, false, false};
    if (output == NULL || !output->initialized) {
        return status;
    }

    if (output->quiescing) {
        status.link = TM_LINK_QUIESCING;
    } else if (output->in_flight) {
        status.link = TM_LINK_BUSY;
    } else if (output->backend.ready(output->backend.ctx)) {
        status.link = TM_LINK_READY;
    }
    status.release_pending = output->release_debt != 0u;
    status.quiesced = output->quiescing && !output->in_flight &&
                      !status.release_pending && output->confirmed_valid &&
                      output->confirmed_buttons == 0u;
    return status;
}

static tm_publish_result_t transport_publish(
    void *ctx, const tm_mouse_report_t *report, uint64_t deadline)
{
    return tm_mouse_output_publish((tm_mouse_output_t *)ctx, report, deadline);
}

static void transport_service(void *ctx)
{
    tm_mouse_output_service((tm_mouse_output_t *)ctx);
}

static void transport_quiesce(void *ctx)
{
    tm_mouse_output_begin_quiesce((tm_mouse_output_t *)ctx);
}

static tm_transport_status_t transport_status(void *ctx)
{
    return tm_mouse_output_status((const tm_mouse_output_t *)ctx);
}

tm_mouse_transport_t tm_mouse_output_transport(tm_mouse_output_t *output)
{
    return (tm_mouse_transport_t){
        .ctx = output,
        .publish_consume = transport_publish,
        .service = transport_service,
        .begin_quiesce = transport_quiesce,
        .status = transport_status,
    };
}
