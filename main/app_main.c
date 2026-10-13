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

static const char *TAG = "tiltmouse";
static tm_app_pipeline_t s_app;

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
#endif

static void wake_app(void *arg)
{
    xTaskNotifyGive((TaskHandle_t)arg);
}

static void read_imu(qmi8658_device_t *imu)
{
    qmi8658_sample_t hardware = {0};
    const qmi8658_status_t status = qmi8658_read_sample(imu, &hardware);
    if (status == QMI8658_NO_DATA) {
        return;
    }
    if (status != QMI8658_OK) {
        tm_app_imu_error(&s_app);
        return;
    }
    tiltmouse_imu_sample_t sample = {.timestamp_us = hardware.host_time_us};
    for (size_t i = 0; i < 3; ++i) {
        sample.accel_g[i] = hardware.accel_g[i];
        sample.gyro_dps[i] = hardware.gyro_dps[i];
    }
    (void)tm_app_on_imu(&s_app, &sample);
}

static void read_touch(tiltmouse_touch_input_t *input)
{
    tiltmouse_touch_raw_sample_t sample = {0};
    if (tiltmouse_touch_input_read(input, &sample) != ESP_OK ||
        !tm_app_on_touch(&s_app, sample.raw, sample.timestamp_us)) {
        tm_app_touch_error(&s_app);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(tiltmouse_board_init_safe());
#if defined(CONFIG_TILTMOUSE_OUTPUT_ESPNOW)
    /* No universal key, plaintext fallback or implicit pairing. The ignored
     * local header is required for an explicitly provisioned radio build. */
    const tm_espnow_mouse_config_t credentials = TM_ESPNOW_LOCAL_CONFIG;
    ESP_ERROR_CHECK(tm_espnow_mouse_init(&credentials));
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
    };
#else
    const tm_mouse_transport_id_t selected = TM_MOUSE_TRANSPORT_USB;
    const tm_app_output_t output = {
        .context = NULL,
        .owner = TM_MOUSE_TRANSPORT_USB,
        .publish = usb_publish,
        .service = usb_service,
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
    while (true) {
        /* Clear accumulated notifications; missed slots are never replayed. */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const uint64_t now_us = (uint64_t)esp_timer_get_time();
        tm_app_boot_sample(&s_app,
            gpio_get_level(TILTMOUSE_GPIO_BOOT) == 0, now_us);
        if (imu_poll_us == 0 || now_us - imu_poll_us >= TM_IMU_POLL_US) {
            imu_poll_us = now_us;
            read_imu(&imu);
        }
        if (touch_poll_us == 0 || now_us - touch_poll_us >= TM_TOUCH_POLL_US) {
            touch_poll_us = now_us;
            read_touch(&touch);
        }
        tm_app_service(&s_app);
        if (report_poll_us == 0 || now_us - report_poll_us >= TM_REPORT_POLL_US) {
            report_poll_us = now_us;
            tm_mouse_publish_result_t result = {0};
            (void)tm_app_dispatch(&s_app, now_us, &result);
        }
        if (diag_poll_us == 0 || now_us - diag_poll_us >= TM_DIAG_POLL_US) {
            diag_poll_us = now_us;
            ESP_LOGI(TAG, "reports=%" PRIu32 " dropped=%" PRIu32
                     " recenters=%" PRIu32 " neutral=%d touch=%d",
                     s_app.reports_published, s_app.dropped_motion,
                     s_app.recenters, (int)s_app.orientation.neutral_valid,
                     (int)s_app.touch_valid);
        }
    }
}
