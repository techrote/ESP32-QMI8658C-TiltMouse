#include "tiltmouse/sensor_indicator.h"

#include <stddef.h>
#include <string.h>

#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "tiltmouse/board.h"
#include "tiltmouse/indicator_frame.h"

#define TM_INDICATOR_POLL_US UINT64_C(100000)
#define TM_INDICATOR_STUCK_US UINT64_C(500000)
#define TM_INDICATOR_SYMBOLS (TM_INDICATOR_BYTES * 8u + 1u)

static rmt_channel_handle_t s_channel;
static rmt_encoder_handle_t s_encoder;
static rmt_symbol_word_t s_symbols[TM_INDICATOR_SYMBOLS];
static uint8_t s_last_frame[TM_INDICATOR_BYTES];
static tm_sensor_indicator_status_t s_status;
static uint64_t s_last_poll_us;
static uint64_t s_submitted_us;
static bool s_have_frame;
static bool s_pending;

static void increment(uint32_t *count)
{
    if (*count != UINT32_MAX) {
        ++*count;
    }
}

esp_err_t tm_sensor_indicator_init(void)
{
    const rmt_tx_channel_config_t channel_config = {
        .gpio_num = TILTMOUSE_GPIO_RGB_DATA,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        /* Whole fixed frame plus EOF fits; even DMA buffer below IDF's limit. */
        .mem_block_symbols = TM_INDICATOR_SYMBOLS + 1u,
        .trans_queue_depth = 1,
        .flags.with_dma = true,
    };
    esp_err_t err = rmt_new_tx_channel(&channel_config, &s_channel);
    if (err == ESP_OK) {
        const rmt_copy_encoder_config_t encoder_config = {0};
        err = rmt_new_copy_encoder(&encoder_config, &s_encoder);
    }
    if (err == ESP_OK) {
        err = rmt_enable(s_channel);
    }
    s_status.last_error = err;
    s_status.running = err == ESP_OK;
    if (err != ESP_OK) {
        if (s_encoder != NULL) {
            (void)rmt_del_encoder(s_encoder);
            s_encoder = NULL;
        }
        if (s_channel != NULL) {
            (void)rmt_del_channel(s_channel);
            s_channel = NULL;
        }
        (void)tiltmouse_board_init_safe();
    }
    return err;
}

static void stop(esp_err_t err)
{
    s_status.running = false;
    s_status.last_error = err;
    /* Cancel once; never reuse the payload or retry a failed hardware path. */
    (void)rmt_disable(s_channel);
    ESP_LOGE("tm-indicator", "disabled after RMT error: %s", esp_err_to_name(err));
}

void tm_sensor_indicator_update(float x_deg, float y_deg, bool ready,
                                uint64_t now_us)
{
    if (!s_status.running || (s_last_poll_us != 0u &&
        (now_us < s_last_poll_us ||
         now_us - s_last_poll_us < TM_INDICATOR_POLL_US))) {
        return;
    }
    s_last_poll_us = now_us;
    if (s_pending) {
        const esp_err_t done = rmt_tx_wait_all_done(s_channel, 0);
        if (done == ESP_ERR_TIMEOUT) {
            increment(&s_status.busy_skips);
            if (now_us >= s_submitted_us &&
                now_us - s_submitted_us >= TM_INDICATOR_STUCK_US) {
                stop(done);
            }
            return;
        }
        if (done != ESP_OK) {
            stop(done);
            return;
        }
        s_pending = false;
    }
    uint8_t rgb[TM_INDICATOR_BYTES];
    tm_indicator_frame(x_deg, y_deg, ready, rgb);
    if (s_have_frame && memcmp(rgb, s_last_frame, sizeof(rgb)) == 0) {
        return;
    }
    /* Payload is static and remains immutable until wait_all_done succeeds. */
    for (size_t i = 0; i < sizeof(rgb); ++i) {
        for (size_t bit = 0; bit < 8u; ++bit) {
            const bool one = (rgb[i] & (0x80u >> bit)) != 0u;
            s_symbols[i * 8u + bit] = (rmt_symbol_word_t){
                .level0 = 1, .duration0 = one ? 9 : 3,
                .level1 = 0, .duration1 = one ? 3 : 9,
            };
        }
    }
    /* 300 us low reset/latch; no bright clearing frame or animation. */
    s_symbols[TM_INDICATOR_SYMBOLS - 1u] = (rmt_symbol_word_t){
        .level0 = 0, .duration0 = 1500,
        .level1 = 0, .duration1 = 1500,
    };
    const rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
        .flags = {.eot_level = 0, .queue_nonblocking = 1},
    };
    const esp_err_t err = rmt_transmit(s_channel, s_encoder, s_symbols,
                                      sizeof(s_symbols), &transmit_config);
    if (err != ESP_OK) {
        stop(err);
        return;
    }
    memcpy(s_last_frame, rgb, sizeof(rgb));
    s_have_frame = true;
    s_pending = true;
    s_submitted_us = now_us;
    increment(&s_status.submitted);
}

tm_sensor_indicator_status_t tm_sensor_indicator_status(void)
{
    return s_status;
}
