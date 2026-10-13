#ifndef TILTMOUSE_ESPNOW_MOUSE_H
#define TILTMOUSE_ESPNOW_MOUSE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "tiltmouse/mouse_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Provide only an explicitly paired FNR-007 deployment; NEVER commit a
 * universal key or select plaintext automatically. Receiver station MAC,
 * public config identity/generation and LMK must match the receiver build.
 */
typedef struct {
    uint8_t sender_mac[6]; /* Must match this ESP32-S3 station MAC. */
    uint8_t receiver_mac[6];
    uint8_t peer_key[16];
    uint8_t channel;
    uint64_t config_id;
    uint32_t generation;
} tm_espnow_mouse_config_t;

/* Initializing this component is an explicit choice of wireless mode.
 * Never call from normal wired startup. Radio only: no AP, DHCP or sockets. */
esp_err_t tm_espnow_mouse_init(const tm_espnow_mouse_config_t *config);
bool tm_espnow_mouse_initialized(void);

typedef struct {
    bool configured;
    bool radio_up;
    bool recovery_pending;
    bool quiescing;
    bool quiesced;
    uint64_t session;
    uint32_t next_sequence;
    uint32_t consecutive_failures;
    uint32_t send_attempts;
    uint32_t immediate_failures;
    uint32_t callback_successes;
    uint32_t callback_failures;
    uint32_t recovery_attempts;
    uint32_t recovery_successes;
    uint32_t recovery_failures;
    uint64_t next_recovery_us;
    esp_err_t last_error;
} tm_espnow_mouse_status_t;

tm_espnow_mouse_status_t tm_espnow_mouse_status(void);
bool tm_espnow_mouse_begin_quiesce(void);
bool tm_espnow_mouse_is_quiesced(void);

/* Fully mirrors the TM-005A logical publication contract. One selected
 * sender, full-state button updates, terminal deltas, no retry queue.
 * ESP_NOW_SEND_SUCCESS does not prove remote application delivery. */
tm_mouse_publish_result_t tm_espnow_mouse_publish(
    const tm_mouse_report_t *report, uint64_t now_us, uint64_t deadline_us);
tm_mouse_publish_result_t tm_espnow_mouse_service(void);

#ifdef __cplusplus
}
#endif

#endif
