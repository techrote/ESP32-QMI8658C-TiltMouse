#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include "tiltmouse/espnow_codec.h"

static bool openssl_hmac(
    void *ctx, const uint8_t *key, size_t key_size,
    const uint8_t *message, size_t message_size, uint8_t digest[32])
{
    (void)ctx;
    unsigned out_len = 0;
    return HMAC(EVP_sha256(), key, (int)key_size, message,
                message_size, digest, &out_len) != NULL && out_len == 32u;
}

static const uint8_t key[16] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
};
static const tm_fnr_auth_context_t auth = {
    .sender_mac = {2, 3, 4, 5, 6, 7},
    .receiver_mac = {10, 11, 12, 13, 14, 15},
    .config_id = UINT64_C(0x123456789abcdef0),
    .generation = 1,
};

static tm_mouse_report_t make(int x, int y, uint32_t buttons)
{
    tm_mouse_report_t report = {0};
    assert(tm_mouse_report_make_current(x, y, buttons, &report));
    return report;
}

static void test_fnr006_exact_envelope(void)
{
    /* Canonical FNR-006 golden packet, from accepted receiver commit
     * 136ef3b612f83fd9cad74a5dd50c2e5f629b44de. */
    static const uint8_t expected[22] = {
        1, 1, 1, 0, 0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 1,
        0xef, 0xcd, 0xab, 0x89, 4, 0, 1, 3, 0xfb, 7
    };
    uint8_t packet[22] = {0};
    tm_mouse_report_t r = make(-5, 7, 3);
    assert(tm_fnr_platform_mouse_encode(&r,
        UINT64_C(0x0123456789abcdef), UINT32_C(0x89abcdef),
        packet, sizeof(packet)));
    assert(memcmp(packet, expected, sizeof(packet)) == 0);
    assert(!tm_fnr_platform_mouse_encode(&r, 0, 0, packet, sizeof(packet)));
    assert(!tm_fnr_platform_mouse_encode(&r, 1, 0, packet, 21));

    assert(tm_mouse_report_make_release(&r));
    assert(tm_fnr_platform_mouse_encode(&r, 1, 0, packet, sizeof(packet)));
    static const uint8_t neutral[4] = {1, 0, 0, 0};
    assert(memcmp(packet + 18, neutral, sizeof(neutral)) == 0);
    r = make(-127, 127, TM_MOUSE_BUTTON_ALL);
    assert(tm_fnr_platform_mouse_encode(&r, 1, 0, packet, sizeof(packet)));
    static const uint8_t extrema[4] = {1, 3, 0x81, 0x7f};
    assert(memcmp(packet + 18, extrema, sizeof(extrema)) == 0);

    r.kind = TM_MOUSE_REPORT_INVALID;
    assert(!tm_fnr_platform_mouse_encode(&r, 1, 0, packet, sizeof(packet)));
    r.kind = TM_MOUSE_REPORT_CURRENT;
    r.buttons = 0x80;
    assert(!tm_fnr_platform_mouse_encode(&r, 1, 0, packet, sizeof(packet)));
    r.buttons = 0;
    r.dx = -128;
    assert(!tm_fnr_platform_mouse_encode(&r, 1, 0, packet, sizeof(packet)));
}

static void test_fnr007_authentication_vectors(void)
{
    /* FNR-007 published independent public vector: inner 01 02 03. */
    static const uint8_t expected[25] = {
        0x46, 0x4e, 0x41, 0x31, 3, 0, 1, 2, 3, 0x0e, 0xe8, 0x09,
        0x1a, 0x2e, 0x32, 0x99, 0x19, 0x4c, 0xee, 0x54, 0x59,
        0xc9, 0xc8, 0xb0, 0x18
    };
    static const uint8_t inner[3] = {1, 2, 3};
    uint8_t wire[250] = {0};
    size_t written = 77;
    assert(tm_fnr_auth_wrap(&auth, key, openssl_hmac, NULL,
                            inner, sizeof(inner), wire, sizeof(wire), &written));
    assert(written == sizeof(expected));
    assert(memcmp(wire, expected, written) == 0);

    tm_fnr_auth_context_t wrong = auth;
    wrong.generation++;
    assert(tm_fnr_auth_wrap(&wrong, key, openssl_hmac, NULL,
                            inner, sizeof(inner), wire, sizeof(wire), &written));
    assert(memcmp(wire, expected, sizeof(expected)) != 0);
    wrong.config_id = 0;
    assert(!tm_fnr_auth_wrap(&wrong, key, openssl_hmac, NULL,
                             inner, sizeof(inner), wire, sizeof(wire), &written));
    assert(written == 0);

    uint8_t all_zero_key[16] = {0};
    assert(!tm_fnr_auth_wrap(&auth, all_zero_key, openssl_hmac, NULL,
                             inner, sizeof(inner), wire, sizeof(wire), &written));
    assert(!tm_fnr_auth_wrap(&auth, key, openssl_hmac, NULL,
                             inner, sizeof(inner), wire, 24, &written));
}

static void test_full_wire_and_sequence_wrap(void)
{
    /* Independent Python hashlib/hmac digest, not derived from codec output. */
    static const uint8_t golden[44] = {
        0x46, 0x4e, 0x41, 0x31, 0x16, 0x00,
        1, 1, 1, 0, 0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 1,
        0xef, 0xcd, 0xab, 0x89, 4, 0, 1, 3, 0xfb, 7,
        0x6b, 0x85, 0x8c, 0xf5, 0x90, 0xe2, 0x24, 0x88,
        0x90, 0x0e, 0x12, 0x4f, 0x24, 0xb6, 0xa6, 0xa9
    };
    tm_fnr_sender_t sender = {0};
    assert(tm_fnr_sender_init(&sender, &auth, key,
                             UINT64_C(0x0123456789abcdef)));
    sender.next_sequence = UINT32_C(0x89abcdef);
    uint8_t wire[44] = {0};
    size_t size = 0;
    tm_mouse_report_t r = make(-5, 7, TM_MOUSE_BUTTON_ALL);
    assert(tm_fnr_sender_encode(&sender, &r, openssl_hmac, NULL,
                                wire, sizeof(wire), &size));
    assert(size == 44 && memcmp(wire, golden, size) == 0);
    assert(sender.next_sequence == UINT32_C(0x89abcdf0));

    sender.next_sequence = UINT32_MAX;
    assert(tm_fnr_sender_encode(&sender, &r, openssl_hmac, NULL,
                                wire, sizeof(wire), &size));
    assert(wire[18] == 0xff && wire[19] == 0xff &&
           wire[20] == 0xff && wire[21] == 0xff);
    assert(sender.next_sequence == 0);
    r = make(0, 0, 0);
    assert(tm_fnr_sender_encode(&sender, &r, openssl_hmac, NULL,
                                wire, sizeof(wire), &size));
    assert(wire[18] == 0 && wire[19] == 0 &&
           wire[20] == 0 && wire[21] == 0);
    assert(sender.next_sequence == 1);
    assert(wire[25] == 0 && wire[26] == 0 && wire[27] == 0);

    const uint32_t old_seq = sender.next_sequence;
    r.kind = TM_MOUSE_REPORT_INVALID;
    assert(!tm_fnr_sender_encode(&sender, &r, openssl_hmac, NULL,
                                 wire, sizeof(wire), &size));
    assert(size == 0 && sender.next_sequence == old_seq);
    assert(!tm_fnr_sender_init(&sender, &auth, key, 0));
}

int main(void)
{
    test_fnr006_exact_envelope();
    test_fnr007_authentication_vectors();
    test_full_wire_and_sequence_wrap();
    return 0;
}
