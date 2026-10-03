#include <string.h>

#include "esp_timer.h"
#include "tiltmouse/board_pins.h"
#include "tiltmouse/touch_input.h"

#define TILTMOUSE_TOUCH_INITIAL_SCANS 3
#define TILTMOUSE_TOUCH_ONESHOT_TIMEOUT_MS 1000

static const int s_touch_channel_ids[TILTMOUSE_TOUCH_CHANNEL_COUNT] = {
    1, 2, 3, 4, 5, 6, 7,
};

static const int s_expected_gpio[TILTMOUSE_TOUCH_CHANNEL_COUNT] = {
    TILTMOUSE_GPIO_TOUCH_LEFT_1,
    TILTMOUSE_GPIO_TOUCH_LEFT_2,
    TILTMOUSE_GPIO_TOUCH_LEFT_3,
    TILTMOUSE_GPIO_TOUCH_REFERENCE,
    TILTMOUSE_GPIO_TOUCH_RIGHT_1,
    TILTMOUSE_GPIO_TOUCH_RIGHT_2,
    TILTMOUSE_GPIO_TOUCH_RIGHT_3,
};

static esp_err_t remember_first_error(esp_err_t first, esp_err_t next)
{
    return first == ESP_OK ? next : first;
}

esp_err_t tiltmouse_touch_input_deinit(tiltmouse_touch_input_t *input)
{
    if (input == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;

    if (input->scanning && input->sensor != NULL) {
        esp_err_t err = touch_sensor_stop_continuous_scanning(input->sensor);
        result = remember_first_error(result, err);
        if (err == ESP_OK) {
            input->scanning = false;
        }
    }

    if (input->enabled && input->sensor != NULL) {
        esp_err_t err = touch_sensor_disable(input->sensor);
        result = remember_first_error(result, err);
        if (err == ESP_OK) {
            input->enabled = false;
            input->scanning = false;
        }
    }

    if (!input->enabled) {
        for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
            if (input->channels[i] != NULL) {
                esp_err_t err = touch_sensor_del_channel(input->channels[i]);
                result = remember_first_error(result, err);
                if (err == ESP_OK) {
                    input->channels[i] = NULL;
                }
            }
        }

        if (input->sensor != NULL) {
            esp_err_t err = touch_sensor_del_controller(input->sensor);
            result = remember_first_error(result, err);
            if (err == ESP_OK) {
                input->sensor = NULL;
            }
        }
    }

    return result;
}

esp_err_t tiltmouse_touch_input_init(tiltmouse_touch_input_t *input)
{
    if (input == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(input, 0, sizeof(*input));

    touch_sensor_sample_config_t sample_cfg[TOUCH_SAMPLE_CFG_NUM] = {
        TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(
            500,
            TOUCH_VOLT_LIM_L_0V5,
            TOUCH_VOLT_LIM_H_2V2),
    };
    touch_sensor_config_t sensor_cfg =
        TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(TOUCH_SAMPLE_CFG_NUM, sample_cfg);

    esp_err_t err = touch_sensor_new_controller(&sensor_cfg, &input->sensor);
    if (err != ESP_OK) {
        return err;
    }

    /*
     * The driver requires a channel threshold even though TiltMouse reads RAW
     * measurements and performs all button decisions in host-testable logic.
     * This value is deliberately not a product/button threshold.
     */
    const touch_channel_config_t channel_cfg = {
        .active_thresh = {2000},
        .charge_speed = TOUCH_CHARGE_SPEED_7,
        .init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT,
    };

    for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
        err = touch_sensor_new_channel(
            input->sensor,
            s_touch_channel_ids[i],
            &channel_cfg,
            &input->channels[i]);
        if (err != ESP_OK) {
            goto fail;
        }

        touch_chan_info_t info = {0};
        err = touch_sensor_get_channel_info(input->channels[i], &info);
        if (err != ESP_OK) {
            goto fail;
        }
        if (info.chan_gpio != s_expected_gpio[i]) {
            err = ESP_ERR_INVALID_STATE;
            goto fail;
        }
    }

    err = touch_sensor_enable(input->sensor);
    if (err != ESP_OK) {
        goto fail;
    }
    input->enabled = true;

    /* Prime the hardware registers before the first application read. */
    for (int i = 0; i < TILTMOUSE_TOUCH_INITIAL_SCANS; ++i) {
        err = touch_sensor_trigger_oneshot_scanning(
            input->sensor,
            TILTMOUSE_TOUCH_ONESHOT_TIMEOUT_MS);
        if (err != ESP_OK) {
            goto fail;
        }
    }

    err = touch_sensor_start_continuous_scanning(input->sensor);
    if (err != ESP_OK) {
        goto fail;
    }
    input->scanning = true;

    return ESP_OK;

fail:
    (void)tiltmouse_touch_input_deinit(input);
    return err;
}

esp_err_t tiltmouse_touch_input_read(
    tiltmouse_touch_input_t *input,
    tiltmouse_touch_raw_sample_t *sample)
{
    if (input == NULL || sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (input->sensor == NULL || !input->enabled || !input->scanning) {
        return ESP_ERR_INVALID_STATE;
    }

    tiltmouse_touch_raw_sample_t next = {0};
    for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
        uint32_t data[TOUCH_SAMPLE_CFG_NUM] = {0};
        esp_err_t err = touch_channel_read_data(
            input->channels[i],
            TOUCH_CHAN_DATA_TYPE_RAW,
            data);
        if (err != ESP_OK) {
            return err;
        }
        next.raw[i] = data[0];
    }

    next.timestamp_us = (uint64_t)esp_timer_get_time();
    *sample = next;
    return ESP_OK;
}
