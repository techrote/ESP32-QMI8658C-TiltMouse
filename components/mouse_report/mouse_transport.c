#include "tiltmouse/mouse_transport.h"

#include <stddef.h>

bool tm_mouse_router_init(
    tm_mouse_router_t *router, tm_route_id_t selected,
    tm_mouse_transport_t transport)
{
    if (router == NULL || (selected != TM_ROUTE_USB && selected != TM_ROUTE_ESPNOW) ||
        transport.ctx == NULL || transport.publish_consume == NULL ||
        transport.service == NULL || transport.begin_quiesce == NULL ||
        transport.status == NULL) {
        return false;
    }

    *router = (tm_mouse_router_t){
        .transport = transport,
        .selected = selected,
        .initialized = true,
        .accepting = true,
    };
    return true;
}

tm_publish_result_t tm_mouse_router_publish(
    tm_mouse_router_t *router, tm_route_id_t requester,
    const tm_mouse_report_t *report, uint64_t motion_deadline_us)
{
    if (router == NULL || !router->initialized || !router->accepting ||
        requester != router->selected) {
        return (tm_publish_result_t){
            TM_MOTION_DROPPED, TM_STATE_UNCHANGED, TM_REASON_INACTIVE
        };
    }

    return router->transport.publish_consume(
        router->transport.ctx, report, motion_deadline_us);
}

void tm_mouse_router_service(tm_mouse_router_t *router)
{
    if (router != NULL && router->initialized) {
        router->transport.service(router->transport.ctx);
    }
}

void tm_mouse_router_begin_quiesce(tm_mouse_router_t *router)
{
    if (router != NULL && router->initialized && router->accepting) {
        router->accepting = false;
        router->transport.begin_quiesce(router->transport.ctx);
    }
}

tm_transport_status_t tm_mouse_router_status(const tm_mouse_router_t *router)
{
    if (router == NULL || !router->initialized) {
        return (tm_transport_status_t){TM_LINK_DOWN, false, false};
    }

    return router->transport.status(router->transport.ctx);
}
