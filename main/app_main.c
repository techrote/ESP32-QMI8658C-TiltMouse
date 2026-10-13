#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tiltmouse/app_pipeline.h"
#include "tiltmouse/board.h"
#include "tiltmouse/board_imu_frame.h"
#include "tiltmouse/board_pins.h"
#include "tiltmouse/qmi8658.h"
#include "tiltmouse/qmi8658_esp_idf.h"
#include "tiltmouse/touch_input.h"
#include "tiltmouse/usb_hid_mouse.h"
#include "tiltmouse/espnow_mouse.h"
#include "sdkconfig.h"

#if defined(CONFIG_TILTMOUSE_OUTPUT_ESPNOW)
#include "tiltmouse_sender_config.local.h"
#endif

/* Exactly one startup output owner, with no radio-to-USB fallback. */
#define TM_IMU_POLL_US UINT64_C(4000)
#define TM_TOUCH_POLL_US UINT64_C(8000)
#define TM_REPORT_POLL_US UINT64_C(8000)
#define TM_DIAG_POLL_US UINT64_C(10000000)
#define TM_DRIVER_RETRY_US UINT64_C(100000)
#define TM_DRIVER_FAILURE_THRESHOLD 3u
#define TM_RADIO_INIT_ATTEMPTS 3u
#define TM_RADIO_INIT_RETRY_MS 100u

static const char *TAG = "tiltmouse";
static tm_app_pipeline_t s_app;

typedef struct {
    uint32_t consecutive_failures;
    uint32_t reinit_attempts;
    uint32_t reinit_successes;
    uint64_t next_retry_us;
    uint64_t no_data_since_us;
    qmi8658_status_t last_status;
#if defined(CONFIG_TILTMOUSE_USB_DIAGNOSTICS)
    qmi8658_sample_t last_sample;
    uint32_t sample_count;
    uint32_t no_data_count;
    uint32_t window_samples;
    bool window_capped;
    float accel_sum[3];
    float accel_min[3];
    float accel_max[3];
    float gyro_sum[3];
    float gyro_min[3];
    float gyro_max[3];
#endif
} tm_imu_runtime_t;

typedef struct {
    uint32_t consecutive_failures;
    uint32_t reinit_attempts;
    uint32_t reinit_successes;
    uint64_t next_retry_us;
    esp_err_t last_error;
} tm_touch_runtime_t;

static tm_mouse_publish_result_t usb_publish(
    void *context, const tm_mouse_report_t *report,
    uint64_t now_us, uint64_t deadline_us)
{
    (void)context;
    return tiltmouse_usb_hid_mouse_publish(report, now_us, deadline_us);
}

static tm_mouse_publish_result_t usb_service(void *context)
{
    (void)context;
    return tiltmouse_usb_hid_mouse_service();
}

static bool usb_begin_quiesce(void *context)
{
    (void)context;
    return tiltmouse_usb_hid_mouse_begin_quiesce();
}

static bool usb_is_quiesced(void *context)
{
    (void)context;
    return tiltmouse_usb_hid_mouse_is_quiesced();
}

#if defined(CONFIG_TILTMOUSE_OUTPUT_ESPNOW)
static tm_mouse_publish_result_t radio_publish(
    void *context, const tm_mouse_report_t *report,
    uint64_t now_us, uint64_t deadline_us)
{
    (void)context;
    return tm_espnow_mouse_publish(report, now_us, deadline_us);
}

static tm_mouse_publish_result_t radio_service(void *context)
{
    (void)context;
    return tm_espnow_mouse_service();
}

static bool radio_begin_quiesce(void *context)
{
    (void)context;
    return tm_espnow_mouse_begin_quiesce();
}

static bool radio_is_quiesced(void *context)
{
    (void)context;
    return tm_espnow_mouse_is_quiesced();
}

static esp_err_t init_radio_bounded(const tm_espnow_mouse_config_t *credentials)
{
    esp_err_t err = ESP_FAIL;
    for (uint32_t attempt = 0; attempt < TM_RADIO_INIT_ATTEMPTS; ++attempt) {
        err = tm_espnow_mouse_init(credentials);
        if (err == ESP_OK || err == ESP_ERR_INVALID_ARG) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(TM_RADIO_INIT_RETRY_MS));
    }
    return err;
}
#endif

static void wake_app(void *arg)
{
    xTaskNotifyGive((TaskHandle_t)arg);
}

static void try_imu_reinit(
    qmi8658_device_t *imu, const qmi8658_bus_t *bus,
    tm_imu_runtime_t *runtime, uint64_t now_us)
{
    if (runtime->consecutive_failures < TM_DRIVER_FAILURE_THRESHOLD ||
        now_us < runtime->next_retry_us) {
        return;
    }
    ++runtime->reinit_attempts;
    const qmi8658_status_t recovery = qmi8658_init(imu, bus);
    runtime->last_status = recovery;
    runtime->next_retry_us = now_us + TM_DRIVER_RETRY_US;
    if (recovery == QMI8658_OK) {
        ++runtime->reinit_successes;
        runtime->consecutive_failures = 0;
        runtime->no_data_since_us = now_us;
    }
}

static void read_imu(
    qmi8658_device_t *imu, const qmi8658_bus_t *bus,
    tm_imu_runtime_t *runtime, uint64_t now_us)
{
    qmi8658_sample_t hardware = {0};
    const qmi8658_status_t status = qmi8658_read_sample(imu, &hardware);
    runtime->last_status = status;
    if (status == QMI8658_NO_DATA) {
#if defined(CONFIG_TILTMOUSE_USB_DIAGNOSTICS)
        if (runtime->no_data_count != UINT32_MAX) {
            ++runtime->no_data_count;
        }
#endif
        if (runtime->no_data_since_us == 0u) {
            runtime->no_data_since_us = now_us;
            return;
        }
        if (now_us >= runtime->no_data_since_us &&
            now_us - runtime->no_data_since_us >
                s_app.config.imu_fresh_us) {
            if (runtime->consecutive_failures != UINT32_MAX) {
                ++runtime->consecutive_failures;
            }
            try_imu_reinit(imu, bus, runtime, now_us);
        }
        return;
    }
    runtime->no_data_since_us = 0u;
    if (status != QMI8658_OK) {
        tm_app_imu_error(&s_app);
        if (runtime->consecutive_failures != UINT32_MAX) {
            ++runtime->consecutive_failures;
        }
        try_imu_reinit(imu, bus, runtime, now_us);
        return;
    }
    runtime->consecutive_failures = 0;
#if defined(CONFIG_TILTMOUSE_USB_DIAGNOSTICS)
    /* Observe exactly the acquisition consumed below; no extra sensor reads. */
    runtime->last_sample = hardware;
    if (runtime->sample_count != UINT32_MAX) {
        ++runtime->sample_count;
    }
    if (runtime->window_samples < 4096u) {
        for (size_t i = 0; i < 3u; ++i) {
            const float accel = hardware.accel_g[i];
            const float gyro = hardware.gyro_dps[i];
            if (runtime->window_samples == 0u) {
                runtime->accel_sum[i] = 0.0f;
                runtime->gyro_sum[i] = 0.0f;
                runtime->accel_min[i] = runtime->accel_max[i] = accel;
                runtime->gyro_min[i] = runtime->gyro_max[i] = gyro;
            }
            runtime->accel_sum[i] += accel;
            runtime->gyro_sum[i] += gyro;
            if (accel < runtime->accel_min[i]) {
                runtime->accel_min[i] = accel;
            }
            if (accel > runtime->accel_max[i]) {
                runtime->accel_max[i] = accel;
            }
            if (gyro < runtime->gyro_min[i]) {
                runtime->gyro_min[i] = gyro;
            }
            if (gyro > runtime->gyro_max[i]) {
                runtime->gyro_max[i] = gyro;
            }
        }
        ++runtime->window_samples;
    } else {
        runtime->window_capped = true;
    }
#endif
    tiltmouse_imu_sample_t sample = {.timestamp_us = hardware.host_time_us};
    tiltmouse_board_imu_to_mouse_frame(hardware.accel_g, sample.accel_g);
    tiltmouse_board_imu_to_mouse_frame(hardware.gyro_dps, sample.gyro_dps);
    (void)tm_app_on_imu(&s_app, &sample);
}

static void read_touch(
    tiltmouse_touch_input_t *input, tm_touch_runtime_t *runtime,
    uint64_t now_us)
{
    tiltmouse_touch_raw_sample_t sample = {0};
    const esp_err_t err = tiltmouse_touch_input_read(input, &sample);
    runtime->last_error = err;
    if (err != ESP_OK || !tm_app_on_touch(&s_app, sample.raw, sample.timestamp_us)) {
        tm_app_touch_error(&s_app);
        if (runtime->consecutive_failures != UINT32_MAX) {
            ++runtime->consecutive_failures;
        }
        if (runtime->consecutive_failures >= TM_DRIVER_FAILURE_THRESHOLD &&
            now_us >= runtime->next_retry_us) {
            ++runtime->reinit_attempts;
            (void)tiltmouse_touch_input_deinit(input);
            runtime->last_error = tiltmouse_touch_input_init(input);
            runtime->next_retry_us = now_us + TM_DRIVER_RETRY_US;
            if (runtime->last_error == ESP_OK) {
                ++runtime->reinit_successes;
                runtime->consecutive_failures = 0;
            }
        }
        return;
    }
    runtime->consecutive_failures = 0;
}

void app_main(void)
{
    ESP_ERROR_CHECK(tiltmouse_board_init_safe());
#if defined(CONFIG_TILTMOUSE_OUTPUT_ESPNOW)
    /* No universal key, plaintext fallback or implicit pairing. The ignored
     * local header is required for an explicitly provisioned radio build. */
    const tm_espnow_mouse_config_t credentials = TM_ESPNOW_LOCAL_CONFIG;
    ESP_ERROR_CHECK(init_radio_bounded(&credentials));
#else
    ESP_ERROR_CHECK(tiltmouse_usb_hid_mouse_init());
#endif

    qmi8658_bus_t bus = {0};
    ESP_ERROR_CHECK(qmi8658_esp_idf_bus_init(&bus));
    qmi8658_device_t imu = {0};
    if (qmi8658_init(&imu, &bus) != QMI8658_OK) {
        ESP_LOGE(TAG, "QMI8658 unavailable; stopping without mouse input");
        qmi8658_esp_idf_bus_deinit(&bus);
        return;
    }

    tiltmouse_touch_input_t touch = {0};
    ESP_ERROR_CHECK(tiltmouse_touch_input_init(&touch));
    const gpio_config_t boot = {
        .pin_bit_mask = (UINT64_C(1) << TILTMOUSE_GPIO_BOOT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&boot));

    const tm_app_config_t config = tm_app_default_config();
#if defined(CONFIG_TILTMOUSE_OUTPUT_ESPNOW)
    const tm_mouse_transport_id_t selected = TM_MOUSE_TRANSPORT_ESPNOW;
    const tm_app_output_t output = {
        .context = NULL,
        .owner = TM_MOUSE_TRANSPORT_ESPNOW,
        .publish = radio_publish,
        .service = radio_service,
        .begin_quiesce = radio_begin_quiesce,
        .is_quiesced = radio_is_quiesced,
    };
#else
    const tm_mouse_transport_id_t selected = TM_MOUSE_TRANSPORT_USB;
    const tm_app_output_t output = {
        .context = NULL,
        .owner = TM_MOUSE_TRANSPORT_USB,
        .publish = usb_publish,
        .service = usb_service,
        .begin_quiesce = usb_begin_quiesce,
        .is_quiesced = usb_is_quiesced,
    };
#endif
    if (!tm_app_init(&s_app, &config, selected, output)) {
        ESP_LOGE(TAG, "Mouse pipeline configuration rejected");
        (void)tiltmouse_touch_input_deinit(&touch);
        qmi8658_esp_idf_bus_deinit(&bus);
        return;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = wake_app,
        .arg = xTaskGetCurrentTaskHandle(),
        .dispatch_method = ESP_TIMER_TASK,
        .name = "tm-tick",
    };
    esp_timer_handle_t timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, TM_IMU_POLL_US));

    ESP_LOGI(TAG, "TM-006 selected=%s; neutral calibration required",
             selected == TM_MOUSE_TRANSPORT_USB ? "USB HID" : "ESP-NOW");
    uint64_t imu_poll_us = 0;
    uint64_t touch_poll_us = 0;
    uint64_t report_poll_us = 0;
    uint64_t diag_poll_us = 0;
    uint64_t last_loop_us = 0;
    uint64_t max_loop_gap_us = 0;
    uint64_t last_logged_button_transition_us = 0;
    uint64_t last_logged_click_window_us = 0;
    uint32_t collapsed_ticks = 0;
    tm_imu_runtime_t imu_runtime = {.last_status = QMI8658_OK};
    tm_touch_runtime_t touch_runtime = {.last_error = ESP_OK};
    while (true) {
        /* Clear accumulated notifications; missed slots are never replayed. */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const uint64_t now_us = (uint64_t)esp_timer_get_time();
        if (last_loop_us != 0 && now_us >= last_loop_us) {
            const uint64_t gap_us = now_us - last_loop_us;
            if (gap_us > max_loop_gap_us) {
                max_loop_gap_us = gap_us;
            }
            const uint64_t slots = gap_us / TM_IMU_POLL_US;
            if (slots > 1u) {
                const uint64_t missed = slots - 1u;
                collapsed_ticks += missed > UINT32_MAX - collapsed_ticks
                    ? UINT32_MAX - collapsed_ticks : (uint32_t)missed;
            }
        }
        last_loop_us = now_us;
        tm_app_boot_sample(&s_app,
            gpio_get_level(TILTMOUSE_GPIO_BOOT) == 0, now_us);
        if (imu_poll_us == 0 || now_us - imu_poll_us >= TM_IMU_POLL_US) {
            imu_poll_us = now_us;
            read_imu(&imu, &bus, &imu_runtime, now_us);
        }
        if (touch_poll_us == 0 || now_us - touch_poll_us >= TM_TOUCH_POLL_US) {
            touch_poll_us = now_us;
            read_touch(&touch, &touch_runtime, now_us);
        }
        tm_app_service(&s_app);
        /* Acquisitions timestamp their successful completion, after loop entry.
         * Keep slot scheduling above on the original clock, but assess freshness
         * and publish only against a clock sampled after those acquisitions. */
        const uint64_t output_now_us = (uint64_t)esp_timer_get_time();
        if (report_poll_us == 0 || now_us - report_poll_us >= TM_REPORT_POLL_US) {
            report_poll_us = now_us;
            tm_mouse_publish_result_t result = {0};
            (void)tm_app_dispatch(&s_app, output_now_us, &result);
            tm_app_diagnostics_t event_diag;
            tm_app_diagnostics(&s_app, output_now_us, &event_diag);
            if (event_diag.last_button_transition_us != 0 &&
                event_diag.last_button_transition_us !=
                    last_logged_button_transition_us) {
                last_logged_button_transition_us =
                    event_diag.last_button_transition_us;
                ESP_LOGI(TAG,
                         "button-transition t=%" PRIu64
                         " buttons=%u motion=(%d,%d) window_until=%" PRIu64,
                         event_diag.last_button_transition_us,
                         (unsigned)event_diag.last_buttons,
                         (int)event_diag.last_dx, (int)event_diag.last_dy,
                         event_diag.click_motion_window_until_us);
            }
            if (event_diag.last_button_transition_us != 0 &&
                event_diag.last_button_transition_us != last_logged_click_window_us &&
                output_now_us >= event_diag.click_motion_window_until_us) {
                last_logged_click_window_us = event_diag.last_button_transition_us;
                ESP_LOGI(TAG,
                         "button-window t=%" PRIu64 " until=%" PRIu64
                         " peak=(%d,%d) samples=%" PRIu32,
                         event_diag.last_button_transition_us,
                         event_diag.click_motion_window_until_us,
                         (int)event_diag.click_peak_dx, (int)event_diag.click_peak_dy,
                         event_diag.click_motion_samples);
            }
        }
        if (diag_poll_us == 0 || now_us - diag_poll_us >= TM_DIAG_POLL_US) {
            diag_poll_us = now_us;
            tm_app_diagnostics_t diag;
            tm_app_diagnostics(&s_app, output_now_us, &diag);
#if defined(CONFIG_TILTMOUSE_USB_DIAGNOSTICS)
            const qmi8658_sample_t *observed = &imu_runtime.last_sample;
            ESP_LOGI(TAG,
                     "imu-observe t=%" PRIu64 " sensor=%" PRIu32
                     " samples=%" PRIu32 " no_data=%" PRIu32 " status=%s"
                     " accel_mg=(%" PRId32 ",%" PRId32 ",%" PRId32 ")"
                     " gyro_mdps=(%" PRId32 ",%" PRId32 ",%" PRId32 ")"
                     " quiet=%" PRIu32 "/%" PRIu32
                     " pose_mdeg=(%" PRId32 ",%" PRId32 ")"
                     " neutral_mdeg=(%" PRId32 ",%" PRId32 ")"
                     " boot_armed=%d boot_down=%d",
                     observed->host_time_us, observed->sensor_timestamp,
                     imu_runtime.sample_count, imu_runtime.no_data_count,
                     qmi8658_status_string(imu_runtime.last_status),
                     (int32_t)(observed->accel_g[0] * 1000.0f),
                     (int32_t)(observed->accel_g[1] * 1000.0f),
                     (int32_t)(observed->accel_g[2] * 1000.0f),
                     (int32_t)(observed->gyro_dps[0] * 1000.0f),
                     (int32_t)(observed->gyro_dps[1] * 1000.0f),
                     (int32_t)(observed->gyro_dps[2] * 1000.0f),
                     s_app.quiet_samples, s_app.config.startup_quiet_samples,
                     (int32_t)(s_app.orientation.roll_deg * 1000.0f),
                     (int32_t)(s_app.orientation.pitch_deg * 1000.0f),
                     (int32_t)(s_app.orientation.neutral_roll_deg * 1000.0f),
                     (int32_t)(s_app.orientation.neutral_pitch_deg * 1000.0f),
                     (int)s_app.boot_armed, (int)s_app.boot_down);
            if (imu_runtime.window_samples != 0u) {
                const float scale = 1000.0f / (float)imu_runtime.window_samples;
                ESP_LOGI(TAG,
                         "imu-window n=%" PRIu32 " capped=%d"
                         " accel_mean_mg=(%" PRId32 ",%" PRId32 ",%" PRId32 ")"
                         " accel_span_mg=(%" PRId32 ",%" PRId32 ",%" PRId32 ")"
                         " gyro_mean_mdps=(%" PRId32 ",%" PRId32 ",%" PRId32 ")"
                         " gyro_span_mdps=(%" PRId32 ",%" PRId32 ",%" PRId32 ")",
                         imu_runtime.window_samples, (int)imu_runtime.window_capped,
                         (int32_t)(imu_runtime.accel_sum[0] * scale),
                         (int32_t)(imu_runtime.accel_sum[1] * scale),
                         (int32_t)(imu_runtime.accel_sum[2] * scale),
                         (int32_t)((imu_runtime.accel_max[0] - imu_runtime.accel_min[0]) * 1000.0f),
                         (int32_t)((imu_runtime.accel_max[1] - imu_runtime.accel_min[1]) * 1000.0f),
                         (int32_t)((imu_runtime.accel_max[2] - imu_runtime.accel_min[2]) * 1000.0f),
                         (int32_t)(imu_runtime.gyro_sum[0] * scale),
                         (int32_t)(imu_runtime.gyro_sum[1] * scale),
                         (int32_t)(imu_runtime.gyro_sum[2] * scale),
                         (int32_t)((imu_runtime.gyro_max[0] - imu_runtime.gyro_min[0]) * 1000.0f),
                         (int32_t)((imu_runtime.gyro_max[1] - imu_runtime.gyro_min[1]) * 1000.0f),
                         (int32_t)((imu_runtime.gyro_max[2] - imu_runtime.gyro_min[2]) * 1000.0f));
                imu_runtime.window_samples = 0u;
                imu_runtime.window_capped = false;
            }
#endif
            ESP_LOGI(TAG,
                     "app reports=%" PRIu32 " move=%" PRIu32
                     " neutral=%d touch_ready=%d age_us=(%" PRIu64 ",%" PRIu64 ")"
                     " dropped=%" PRIu32 " recenter=%" PRIu32
                     " imu(err=%" PRIu32 " stale=%" PRIu32 " rec=%" PRIu32
                     " drv_retry=%" PRIu32 "/%" PRIu32 ")"
                     " touch(err=%" PRIu32 " stale=%" PRIu32
                     " recal=%" PRIu32 " rec=%" PRIu32
                     " drv_retry=%" PRIu32 "/%" PRIu32 ")"
                     " click_peak=(%d,%d)/%" PRIu32
                     " loop_max=%" PRIu64 " collapsed=%" PRIu32
                     " stack_words=%u",
                     diag.reports_published, diag.movement_reports,
                     (int)diag.neutral_valid, (int)diag.touch_valid,
                     diag.imu_age_us, diag.touch_age_us,
                     diag.dropped_motion, diag.recenters,
                     diag.imu_errors, diag.imu_stale, diag.imu_recoveries,
                     imu_runtime.reinit_successes, imu_runtime.reinit_attempts,
                     diag.touch_errors, diag.touch_stale,
                     diag.touch_recalibrations, diag.touch_recoveries,
                     touch_runtime.reinit_successes,
                     touch_runtime.reinit_attempts,
                     (int)diag.click_peak_dx, (int)diag.click_peak_dy,
                     diag.click_motion_samples,
                     max_loop_gap_us, collapsed_ticks,
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
#if defined(CONFIG_TILTMOUSE_OUTPUT_ESPNOW)
            const tm_espnow_mouse_status_t radio = tm_espnow_mouse_status();
            ESP_LOGI(TAG,
                     "radio up=%d recover=%d fail_streak=%" PRIu32
                     " send=%" PRIu32 " api_fail=%" PRIu32
                     " cb_ok=%" PRIu32 " cb_fail=%" PRIu32
                     " recovery=%" PRIu32 "/%" PRIu32
                     " seq=%" PRIu32 " session=%" PRIu64,
                     (int)radio.radio_up, (int)radio.recovery_pending,
                     radio.consecutive_failures, radio.send_attempts,
                     radio.immediate_failures, radio.callback_successes,
                     radio.callback_failures, radio.recovery_successes,
                     radio.recovery_attempts, radio.next_sequence,
                     radio.session);
#else
            const tm_usb_hid_mouse_status_t usb =
                tiltmouse_usb_hid_mouse_status();
            ESP_LOGI(TAG,
                     "usb mounted=%d suspended=%d attach=%" PRIu32
                     " detach=%" PRIu32 " suspend=%" PRIu32
                     " resume=%" PRIu32 " complete=%" PRIu32
                     " fail=%" PRIu32 " reset=%" PRIu32,
                     (int)usb.mounted, (int)usb.suspended,
                     usb.attaches, usb.detaches, usb.suspends, usb.resumes,
                     usb.completions, usb.transfer_failures,
                     usb.lifecycle_resets);
#endif
        }
    }
}
