#include "tiltmouse/espnow_mouse.h"

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "psa/crypto.h"
#include "tiltmouse/espnow_codec.h"

static tm_fnr_sender_t s_sender;
static tm_mouse_transport_t s_transport;
static uint8_t s_receiver_mac[6];
static atomic_int s_completion; /* 0=pending, 1=MAC success, 2=MAC failure */
static bool s_initialized;

static void wipe(void *buffer, size_t length)
{
    volatile uint8_t *p = buffer;
    for (size_t i = 0; i < length; ++i) {
        p[i] = 0;
    }
}

static bool valid_receiver_mac(const uint8_t mac[6])
{
    if ((mac[0] & 1u) != 0u) {
        return false;
    }
    uint8_t any = 0;
    for (size_t i = 0; i < 6; ++i) {
        any |= mac[i];
    }
    return any != 0;
}

static bool valid_sender_config(const tm_espnow_mouse_config_t *cfg)
{
    if (cfg == NULL || cfg->channel < 1u || cfg->channel > 14u ||
        cfg->config_id == 0 || cfg->generation == 0 ||
        !valid_receiver_mac(cfg->receiver_mac) ||
        !valid_receiver_mac(cfg->sender_mac)) {
        return false;
    }
    uint8_t any = 0;
    for (size_t i = 0; i < sizeof(cfg->peer_key); ++i) {
        any |= cfg->peer_key[i];
    }
    return any != 0;
}

static bool idf_hmac(void *context, const uint8_t *key, size_t key_size,
                     const uint8_t *message, size_t message_size,
                     uint8_t digest[32])
{
    (void)context;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, key_size * 8u);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_key_id_t key_id = 0;
    const psa_status_t imported =
        psa_import_key(&attributes, key, key_size, &key_id);
    psa_reset_key_attributes(&attributes);
    if (imported != PSA_SUCCESS) {
        return false;
    }
    size_t written = 0;
    const psa_status_t status = psa_mac_compute(
        key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256),
        message, message_size, digest, TM_FNR_SHA256_SIZE, &written);
    (void)psa_destroy_key(key_id);
    return status == PSA_SUCCESS && written == TM_FNR_SHA256_SIZE;
}

static bool radio_ready(void *context)
{
    (void)context;
    return s_initialized && atomic_load(&s_completion) == 0;
}

static bool radio_submit(void *context, const tm_mouse_report_t *report)
{
    (void)context;
    uint8_t wire[TM_FNR_AUTH_PACKET_SIZE] = {0};
    size_t length = 0;
    if (!tm_fnr_sender_encode(&s_sender, report, idf_hmac, NULL,
                              wire, sizeof(wire), &length)) {
        return false;
    }
    /* Successful API submission consumes motion. Callback status is MAC
     * evidence only; receiver-level application ACK is not defined in v1. */
    return esp_now_send(s_receiver_mac, wire, length) == ESP_OK;
}

static void on_send(const esp_now_send_info_t *info,
                    esp_now_send_status_t status)
{
    (void)info;
    atomic_store(&s_completion, status == ESP_NOW_SEND_SUCCESS ? 1 : 2);
}

static void drain_completion(void)
{
    const int result = atomic_exchange(&s_completion, 0);
    if (result != 0) {
        const uint64_t ticket = tm_mouse_transport_pending_ticket(&s_transport);
        if (ticket != 0) {
            (void)tm_mouse_transport_complete(&s_transport, ticket, result == 1);
        }
    }
}

static esp_err_t init_wifi_and_peer(const tm_espnow_mouse_config_t *cfg,
                                    tm_fnr_auth_context_t *auth)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    wifi_init_config_t wifi = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi);
    if (err != ESP_OK) {
        return err;
    }
    bool wifi_started = false;
    bool now_started = false;
    do {
        err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
        if (err != ESP_OK) break;
        err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK) break;
        err = esp_wifi_start();
        if (err != ESP_OK) break;
        wifi_started = true;
        err = esp_wifi_set_channel(cfg->channel, WIFI_SECOND_CHAN_NONE);
        if (err != ESP_OK) break;
        err = esp_wifi_get_mac(WIFI_IF_STA, auth->sender_mac);
        if (err != ESP_OK) break;
        if (memcmp(auth->sender_mac, cfg->sender_mac, 6) != 0) {
            err = ESP_ERR_INVALID_STATE;
            break; /* Fail closed if receiver provisioning expects another sender. */
        }
        memcpy(auth->receiver_mac, cfg->receiver_mac, 6);
        auth->config_id = cfg->config_id;
        auth->generation = cfg->generation;

        err = esp_now_init();
        if (err != ESP_OK) break;
        now_started = true;

        /* Derive a local PMK separate from the over-air LMK. Receiver PMK
         * protects its own local key storage and need not equal this PMK. */
        static const uint8_t pmk_domain[] = "TM-ESP-NOW-PMK-v1";
        uint8_t pmk_digest[32] = {0};
        const bool derived = idf_hmac(
            NULL, cfg->peer_key, 16, pmk_domain,
            sizeof(pmk_domain) - 1, pmk_digest);
        if (!derived) {
            wipe(pmk_digest, sizeof(pmk_digest));
            err = ESP_FAIL;
            break;
        }
        err = esp_now_set_pmk(pmk_digest);
        wipe(pmk_digest, sizeof(pmk_digest));
        if (err != ESP_OK) break;

        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, cfg->receiver_mac, 6);
        memcpy(peer.lmk, cfg->peer_key, 16);
        peer.channel = cfg->channel;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = true;
        err = esp_now_add_peer(&peer);
        wipe(peer.lmk, sizeof(peer.lmk));
        if (err != ESP_OK) break;

        err = esp_now_register_send_cb(on_send);
        if (err == ESP_OK) {
            return ESP_OK;
        }
    } while (false);

    if (now_started) {
        (void)esp_now_deinit();
    }
    if (wifi_started) {
        (void)esp_wifi_stop();
    }
    (void)esp_wifi_deinit();
    return err;
}

esp_err_t tm_espnow_mouse_init(const tm_espnow_mouse_config_t *cfg)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (psa_crypto_init() != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    if (!valid_sender_config(cfg)) {
        return ESP_ERR_INVALID_ARG;
    }
    tm_fnr_auth_context_t auth = {0};
    esp_err_t err = init_wifi_and_peer(cfg, &auth);
    if (err != ESP_OK) {
        return err; /* Fail closed; never try unencrypted transmission. */
    }

    /* RF-backed ESP random value, fresh nonzero u64 per sender activation. */
    uint64_t session = 0;
    do {
        session = ((uint64_t)esp_random() << 32) | esp_random();
    } while (session == 0);

    if (!tm_fnr_sender_init(&s_sender, &auth, cfg->peer_key, session)) {
        (void)esp_now_deinit();
        (void)esp_wifi_stop();
        (void)esp_wifi_deinit();
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(s_receiver_mac, cfg->receiver_mac, 6);
    atomic_store(&s_completion, 0);
    const tm_mouse_transport_sink_t sink = {
        .ready = radio_ready,
        .submit = radio_submit,
        .context = NULL,
    };
    if (!tm_mouse_transport_activate(
            &s_transport, TM_MOUSE_TRANSPORT_ESPNOW, sink)) {
        wipe(&s_sender, sizeof(s_sender));
        (void)esp_now_deinit();
        (void)esp_wifi_stop();
        (void)esp_wifi_deinit();
        return ESP_FAIL;
    }
    s_initialized = true;
    return ESP_OK;
}

bool tm_espnow_mouse_initialized(void)
{
    return s_initialized;
}

tm_mouse_publish_result_t tm_espnow_mouse_publish(
    const tm_mouse_report_t *report, uint64_t now_us, uint64_t deadline_us)
{
    drain_completion();
    return tm_mouse_transport_publish(
        &s_transport, TM_MOUSE_TRANSPORT_ESPNOW,
        report, now_us, deadline_us);
}

tm_mouse_publish_result_t tm_espnow_mouse_service(void)
{
    drain_completion();
    return tm_mouse_transport_service(
        &s_transport, TM_MOUSE_TRANSPORT_ESPNOW);
}
