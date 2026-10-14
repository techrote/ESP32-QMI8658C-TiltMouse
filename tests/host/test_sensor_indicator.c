#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driver/rmt_tx.h"
#include "tiltmouse/sensor_indicator.h"
#include "tiltmouse/indicator_frame.h"

static const void *pending_payload;
static size_t pending_bytes;
static uint8_t payload_copy[(TM_INDICATOR_BYTES * 8u + 1u) * sizeof(rmt_symbol_word_t)];
static bool busy;
static bool fail_send;
static unsigned sends, waits, disables, logs;

const char *esp_err_to_name(esp_err_t error) { (void)error; return "fake"; }
esp_err_t tiltmouse_board_init_safe(void) { return ESP_OK; }
void test_indicator_log(const char *tag, const char *format, ...)
{
    (void)tag; (void)format; ++logs;
}
esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t *cfg, rmt_channel_handle_t *out)
{
    assert(cfg->gpio_num == 14 && cfg->flags.with_dma);
    assert(cfg->trans_queue_depth == 1u && cfg->resolution_hz == 10000000u);
    assert((cfg->mem_block_symbols & 1u) == 0u);
    *out = (void *)1;
    return ESP_OK;
}
esp_err_t rmt_new_copy_encoder(const rmt_copy_encoder_config_t *cfg, rmt_encoder_handle_t *out)
{
    (void)cfg; *out = (void *)2; return ESP_OK;
}
esp_err_t rmt_enable(rmt_channel_handle_t channel) { assert(channel == (void *)1); return ESP_OK; }
esp_err_t rmt_disable(rmt_channel_handle_t channel)
{
    assert(channel == (void *)1); ++disables; busy = false; return ESP_OK;
}
esp_err_t rmt_del_channel(rmt_channel_handle_t channel) { (void)channel; return ESP_OK; }
esp_err_t rmt_del_encoder(rmt_encoder_handle_t encoder) { (void)encoder; return ESP_OK; }
esp_err_t rmt_tx_wait_all_done(rmt_channel_handle_t channel, int timeout)
{
    assert(channel == (void *)1 && timeout == 0);
    ++waits;
    assert(memcmp(pending_payload, payload_copy, pending_bytes) == 0);
    return busy ? ESP_ERR_TIMEOUT : ESP_OK;
}
esp_err_t rmt_transmit(rmt_channel_handle_t channel, rmt_encoder_handle_t encoder,
                       const void *payload, size_t bytes, const rmt_transmit_config_t *cfg)
{
    assert(channel == (void *)1 && encoder == (void *)2);
    assert(!busy && cfg->loop_count == 0 && cfg->flags.queue_nonblocking && !cfg->flags.eot_level);
    assert(bytes == sizeof(payload_copy));
    if (fail_send) {
        return ESP_FAIL;
    }
    pending_payload = payload; pending_bytes = bytes;
    memcpy(payload_copy, payload, bytes);
    busy = true; ++sends;
    return ESP_OK;
}

int main(int argc, char **argv)
{
    assert(tm_sensor_indicator_init() == ESP_OK);
    tm_sensor_indicator_update(0.0f, 0.0f, false, 1);
    assert(sends == 1u);
    /* No early resubmit or payload mutation while DMA still owns the buffer. */
    tm_sensor_indicator_update(15.0f, 15.0f, true, 50000);
    assert(sends == 1u && waits == 0u);
    tm_sensor_indicator_update(15.0f, 15.0f, true, 100001);
    assert(sends == 1u && tm_sensor_indicator_status().busy_skips == 1u);
    assert(memcmp(pending_payload, payload_copy, pending_bytes) == 0);
    if (argc > 1 && strcmp(argv[1], "stuck") == 0) {
        tm_sensor_indicator_update(15.0f, 15.0f, true, 500001);
        assert(!tm_sensor_indicator_status().running && disables == 1u && logs == 1u);
        tm_sensor_indicator_update(0.0f, 0.0f, true, 900001);
        assert(sends == 1u && disables == 1u);
        return 0;
    }
    busy = false;
    tm_sensor_indicator_update(15.0f, 15.0f, true, 200001);
    assert(sends == 2u);
    busy = false;
    /* An unchanged frame does not generate a continuous electrical stream. */
    tm_sensor_indicator_update(15.0f, 15.0f, true, 300001);
    assert(sends == 2u);
    fail_send = true;
    tm_sensor_indicator_update(-15.0f, 0.0f, true, 400001);
    assert(!tm_sensor_indicator_status().running && disables == 1u && logs == 1u);
    tm_sensor_indicator_update(0.0f, 0.0f, true, 900001);
    assert(sends == 2u && disables == 1u);
    return 0;
}
