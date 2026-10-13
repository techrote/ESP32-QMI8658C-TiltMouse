#ifndef TILTMOUSE_ESPNOW_CODEC_H
#define TILTMOUSE_ESPNOW_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tiltmouse/mouse_report.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exact faikeow FNR-006/FNR-007 v1 bytes, never a C struct wire ABI. */
enum {
    TM_FNR_PLATFORM_VERSION = 1,
    TM_FNR_RELATIVE_MOUSE_ID = 1,
    TM_FNR_PROFILE_DATA_TYPE = 1,
    TM_FNR_MOUSE_SCHEMA = 1,
    TM_FNR_INNER_SIZE = 22,
    TM_FNR_AUTH_OVERHEAD = 22,
    TM_FNR_AUTH_PACKET_SIZE = 44,
    TM_FNR_KEY_SIZE = 16,
    TM_FNR_AUTH_TAG_SIZE = 16,
    TM_FNR_SHA256_SIZE = 32,
};

typedef struct {
    uint8_t sender_mac[6];
    uint8_t receiver_mac[6];
    uint64_t config_id;
    uint32_t generation;
} tm_fnr_auth_context_t;

typedef bool (*tm_fnr_hmac_fn)(
    void *context, const uint8_t *key, size_t key_size,
    const uint8_t *message, size_t message_size, uint8_t digest[32]);

/* Validates the entire input before writing; zero-initialized reports are
 * invalid and cannot be transmitted. Sender session must be fresh/nonzero.
 */
bool tm_fnr_platform_mouse_encode(
    const tm_mouse_report_t *report, uint64_t session, uint32_t sequence,
    uint8_t *wire, size_t capacity);

/* FNR-007 paired-deployment radio authentication. No plaintext fallback.
 * The HMAC callback may be backed by mbedTLS (target) or OpenSSL (host tests).
 */
bool tm_fnr_auth_wrap(const tm_fnr_auth_context_t *auth,
                      const uint8_t peer_key[TM_FNR_KEY_SIZE],
                      tm_fnr_hmac_fn hmac, void *hmac_context,
                      const uint8_t *inner, size_t inner_size,
                      uint8_t *wire, size_t capacity, size_t *written);

typedef struct {
    tm_fnr_auth_context_t auth;
    uint8_t peer_key[TM_FNR_KEY_SIZE];
    uint64_t session;
    uint32_t next_sequence;
    bool initialized;
} tm_fnr_sender_t;

bool tm_fnr_sender_init(tm_fnr_sender_t *sender,
                        const tm_fnr_auth_context_t *auth,
                        const uint8_t peer_key[TM_FNR_KEY_SIZE],
                        uint64_t nonzero_new_session);

/* One fresh packet per call, no resend/queued movement, wrap-safe u32 seq.
 * Sequence advances after a valid encoded packet is consumed, including if a
 * lower-layer esp_now_send attempt later fails. */
bool tm_fnr_sender_encode(tm_fnr_sender_t *sender,
                          const tm_mouse_report_t *report,
                          tm_fnr_hmac_fn hmac, void *hmac_context,
                          uint8_t *wire, size_t capacity, size_t *written);

#ifdef __cplusplus
}
#endif

#endif
