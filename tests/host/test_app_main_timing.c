#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Exercise the actual target scheduler, with acquisitions that finish after
 * its loop-entry clock. No copied scheduler or weakened freshness predicate. */
#include "../../main/app_main.c"

static jmp_buf done;
static uint64_t clock_us = UINT64_C(1000000);
static unsigned iterations;
static unsigned movement;
static unsigned publications;

void test_log(const char *tag, const char *format, ...)
{
    (void)tag;
    (void)format;
}

int64_t esp_timer_get_time(void) { return (int64_t)clock_us; }
esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *timer)
{
    assert(args->callback != NULL);
    *timer = &clock_us;
    return ESP_OK;
}
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us)
{
    assert(timer != NULL && period_us == 4000);
    return ESP_OK;
}
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return &clock_us; }
void xTaskNotifyGive(TaskHandle_t task) { assert(task != NULL); }
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task) { (void)task; return 1024; }
uint32_t ulTaskNotifyTake(int clear, uint32_t timeout)
{
    assert(clear == pdTRUE && timeout == portMAX_DELAY);
    if (++iterations > 600) {
        longjmp(done, 1);
    }
    clock_us = UINT64_C(1000000) + (uint64_t)iterations * 4000;
    return 1;
}
esp_err_t gpio_config(const gpio_config_t *config) { assert(config != NULL); return ESP_OK; }
int gpio_get_level(int pin) { assert(pin == 0); return 1; }
esp_err_t tiltmouse_board_init_safe(void) { return ESP_OK; }
esp_err_t qmi8658_esp_idf_bus_init(qmi8658_bus_t *bus) { (void)bus; return ESP_OK; }
void qmi8658_esp_idf_bus_deinit(qmi8658_bus_t *bus) { (void)bus; }
qmi8658_status_t qmi8658_init(qmi8658_device_t *imu, const qmi8658_bus_t *bus)
{
    (void)bus;
    imu->initialized = true;
    return QMI8658_OK;
}
qmi8658_status_t qmi8658_read_sample(qmi8658_device_t *imu, qmi8658_sample_t *sample)
{
    assert(imu->initialized);
    memset(sample, 0, sizeof(*sample));
    clock_us += 100; /* The target driver timestamps only after its I2C burst. */
    sample->host_time_us = clock_us;
    /* Confirmed Matrix LED-face-up mounting: native sensor Z is negative. */
    sample->accel_g[2] = -1.0f;
    if (iterations > 160 && iterations < 500) {
        sample->accel_g[1] = -0.3f;
        sample->accel_g[2] = -0.9539392f;
    }
    return QMI8658_OK;
}
esp_err_t tiltmouse_touch_input_init(tiltmouse_touch_input_t *touch) { (void)touch; return ESP_OK; }
esp_err_t tiltmouse_touch_input_deinit(tiltmouse_touch_input_t *touch) { (void)touch; return ESP_OK; }
esp_err_t tiltmouse_touch_input_read(tiltmouse_touch_input_t *touch, tiltmouse_touch_raw_sample_t *sample)
{
    (void)touch;
    clock_us += 100;
    sample->timestamp_us = clock_us;
    for (unsigned i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
        sample->raw[i] = 10000;
    }
    return ESP_OK;
}
esp_err_t tiltmouse_usb_hid_mouse_init(void) { return ESP_OK; }
bool tiltmouse_usb_hid_mouse_begin_quiesce(void) { return true; }
bool tiltmouse_usb_hid_mouse_is_quiesced(void) { return true; }
tm_usb_hid_mouse_status_t tiltmouse_usb_hid_mouse_status(void)
{
    return (tm_usb_hid_mouse_status_t){.initialized = true, .mounted = true};
}
tm_mouse_publish_result_t tiltmouse_usb_hid_mouse_service(void)
{
    return (tm_mouse_publish_result_t){0};
}
tm_mouse_publish_result_t tiltmouse_usb_hid_mouse_publish(
    const tm_mouse_report_t *report, uint64_t now_us, uint64_t deadline_us)
{
    assert(now_us == deadline_us && tm_mouse_report_is_valid(report));
    ++publications;
    movement += report->dx != 0 || report->dy != 0;
    return (tm_mouse_publish_result_t){.motion = TM_MOUSE_MOTION_ACCEPTED,
        .state = TM_MOUSE_STATE_BUTTONS_RETAINED, .reason = TM_MOUSE_REASON_OK};
}

int main(void)
{
    if (setjmp(done) == 0) {
        app_main();
        assert(!"Target application returned unexpectedly");
    }
    printf("main timing: publications=%u motion=%u imu_stale=%u touch_stale=%u\n",
           publications, movement, s_app.imu_stale, s_app.touch_stale);
    fflush(stdout);
    assert(s_app.imu_stale == 0);
    assert(s_app.touch_stale == 0);
    assert(s_app.orientation.neutral_valid);
    assert(fabsf(s_app.orientation.neutral_roll_deg) < 1.0f);
    assert(fabsf(s_app.orientation.neutral_pitch_deg) < 1.0f);
    assert(s_app.touch_valid);
    assert(movement > 0);
    return 0;
}
