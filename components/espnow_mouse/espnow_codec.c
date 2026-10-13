#include "tiltmouse/espnow_codec.h"

#include <string.h>

static void put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8u);
}

static void put_u32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4u; ++i) {
        p[i] = (uint8_t)(value >> (8u * i));
    }
}

static void put_u64(uint8_t *p, uint64_t value)
{
    for (unsigned i = 0; i < 8u; ++i) {
        p[i] = (uint8_t)(value >> (8u * i));
    }
}

static bool mac_valid(const uint8_t mac[6])
{
    if (mac == NULL || (mac[0] & 1u) != 0u) {
        return false;
    }
    uint8_t nonzero = 0;
    for (size_t i = 0; i < 6; ++i) {
        nonzero |= mac[i];
    }
    return nonzero != 0;
}

static bool auth_valid(const tm_fnr_auth_context_t *auth,
                       const uint8_t key[TM_FNR_KEY_SIZE])
{
    if (auth == NULL || key == NULL || auth->config_id == 0 ||
        auth->generation == 0 || !mac_valid(auth->sender_mac) ||
        !mac_valid(auth->receiver_mac)) {
        return false;
    }
    uint8_t nonzero = 0;
    for (size_t i = 0; i < TM_FNR_KEY_SIZE; ++i) {
        nonzero |= key[i];
    }
    return nonzero != 0;
}

bool tm_fnr_platform_mouse_encode(
    const tm_mouse_report_t *report, uint64_t session, uint32_t sequence,
    uint8_t *wire, size_t capacity)
{
    if (wire == NULL || capacity < TM_FNR_INNER_SIZE || session == 0 ||
        !tm_mouse_report_is_valid(report)) {
        return false;
    }
    const bool release = report->kind == TM_MOUSE_REPORT_RELEASE_ALL;
    uint8_t encoded[TM_FNR_INNER_SIZE] = {0};
    encoded[0] = TM_FNR_PLATFORM_VERSION;
    encoded[1] = TM_FNR_RELATIVE_MOUSE_ID;
    encoded[2] = TM_FNR_PROFILE_DATA_TYPE;
    put_u64(encoded + 4, session);
    put_u32(encoded + 12, sequence);
    put_u16(encoded + 16, 4);
    encoded[18] = TM_FNR_MOUSE_SCHEMA;
    encoded[19] = release ? 0u : report->buttons;
    encoded[20] = release ? 0u : (uint8_t)report->dx;
    encoded[21] = release ? 0u : (uint8_t)report->dy;
    memcpy(wire, encoded, sizeof(encoded));
    return true;
}

static void clear_secret(uint8_t *buffer, size_t len)
{
    volatile uint8_t *p = buffer;
    for (size_t i = 0; i < len; ++i) {
        p[i] = 0;
    }
}

bool tm_fnr_auth_wrap(const tm_fnr_auth_context_t *auth,
                      const uint8_t peer_key[TM_FNR_KEY_SIZE],
                      tm_fnr_hmac_fn hmac, void *hmac_context,
                      const uint8_t *inner, size_t inner_size,
                      uint8_t *wire, size_t capacity, size_t *written)
{
    if (written != NULL) {
        *written = 0;
    }
    if (!auth_valid(auth, peer_key) || hmac == NULL || inner == NULL ||
        wire == NULL || written == NULL || inner_size == 0 ||
        inner_size > 228u || capacity < inner_size + TM_FNR_AUTH_OVERHEAD) {
        return false;
    }

    static const uint8_t kdf_domain[] = "FNR-radio-auth-key-v1";
    static const uint8_t auth_domain[] = "FNR-radio-auth-v1";
    uint8_t derived[TM_FNR_SHA256_SIZE] = {0};
    uint8_t digest[TM_FNR_SHA256_SIZE] = {0};
    uint8_t message[41 + 6 + 228] = {0};
    uint8_t frame[250] = {0};
    bool ok = false;

    do {
        if (!hmac(hmac_context, peer_key, TM_FNR_KEY_SIZE,
                  kdf_domain, sizeof(kdf_domain) - 1, derived)) {
            break;
        }
        memcpy(frame, "FNA1", 4);
        put_u16(frame + 4, (uint16_t)inner_size);
        memcpy(frame + 6, inner, inner_size);

        memcpy(message, auth_domain, sizeof(auth_domain) - 1);
        put_u64(message + 17, auth->config_id);
        put_u32(message + 25, auth->generation);
        memcpy(message + 29, auth->sender_mac, 6);
        memcpy(message + 35, auth->receiver_mac, 6);
        memcpy(message + 41, frame, inner_size + 6);

        if (!hmac(hmac_context, derived, sizeof(derived), message,
                  41 + inner_size + 6, digest)) {
            break;
        }
        memcpy(frame + 6 + inner_size, digest, TM_FNR_AUTH_TAG_SIZE);
        memcpy(wire, frame, inner_size + TM_FNR_AUTH_OVERHEAD);
        *written = inner_size + TM_FNR_AUTH_OVERHEAD;
        ok = true;
    } while (false);

    clear_secret(derived, sizeof(derived));
    clear_secret(digest, sizeof(digest));
    clear_secret(message, sizeof(message));
    clear_secret(frame, sizeof(frame));
    return ok;
}

bool tm_fnr_sender_init(tm_fnr_sender_t *sender,
                        const tm_fnr_auth_context_t *auth,
                        const uint8_t peer_key[TM_FNR_KEY_SIZE],
                        uint64_t nonzero_new_session)
{
    if (sender == NULL) {
        return false;
    }
    if (nonzero_new_session == 0 || !auth_valid(auth, peer_key)) {
        clear_secret((uint8_t *)sender, sizeof(*sender));
        return false;
    }
    *sender = (tm_fnr_sender_t){0};
    sender->auth = *auth;
    memcpy(sender->peer_key, peer_key, TM_FNR_KEY_SIZE);
    sender->session = nonzero_new_session;
    sender->initialized = true;
    return true;
}

bool tm_fnr_sender_encode(tm_fnr_sender_t *sender,
                          const tm_mouse_report_t *report,
                          tm_fnr_hmac_fn hmac, void *hmac_context,
                          uint8_t *wire, size_t capacity, size_t *written)
{
    if (written != NULL) {
        *written = 0;
    }
    if (sender == NULL || !sender->initialized || wire == NULL ||
        capacity < TM_FNR_AUTH_PACKET_SIZE || written == NULL) {
        return false;
    }

    uint8_t inner[TM_FNR_INNER_SIZE] = {0};
    if (!tm_fnr_platform_mouse_encode(
            report, sender->session, sender->next_sequence,
            inner, sizeof(inner))) {
        return false;
    }

    const bool ok = tm_fnr_auth_wrap(
        &sender->auth, sender->peer_key, hmac, hmac_context, inner,
        sizeof(inner), wire, capacity, written);
    if (ok) {
        ++sender->next_sequence; /* unsigned u32 wrap is defined */
    }
    return ok;
}
