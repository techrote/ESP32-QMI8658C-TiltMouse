#ifndef TILTMOUSE_APP_PIPELINE_H
#define TILTMOUSE_APP_PIPELINE_H

#include <stdbool.h>
#include <stdint.h>

#include "tiltmouse/mouse_motion.h"
#include "tiltmouse/mouse_report.h"
#include "tiltmouse/mouse_transport.h"
#include "tiltmouse/orientation.h"
#include "tiltmouse/touch_buttons.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Provisional rates/thresholds; TM-008 owns physical usability tuning. */
typedef struct {
    uint64_t imu_fresh_us;
    uint64_t touch_fresh_us;
    uint64_t max_report_gap_us;
    uint64_t boot_hold_min_us;
    uint64_t boot_hold_max_us;
    uint32_t startup_quiet_samples;
    float startup_gyro_limit_dps;
    tiltmouse_orientation_config_t orientation;
    tiltmouse_mouse_motion_config_t motion;
    tiltmouse_touch_buttons_config_t touch;
} tm_app_config_t;

/* Immutable once app_init accepts it: exactly one selected transport. */
typedef struct {
    void *context;
    tm_mouse_transport_id_t owner;
    tm_mouse_publish_result_t (*publish)(
        void *context, const tm_mouse_report_t *report,
        uint64_t now_us, uint64_t deadline_us);
    tm_mouse_publish_result_t (*service)(void *context);
    bool (*begin_quiesce)(void *context);
    bool (*is_quiesced)(void *context);
} tm_app_output_t;

typedef struct {
    tm_app_config_t config;
    tiltmouse_orientation_state_t orientation;
    tiltmouse_mouse_motion_state_t motion;
    tiltmouse_touch_buttons_t touch;
    tiltmouse_touch_buttons_output_t buttons;
    tm_app_output_t output;
    tm_app_output_t pending_output;
    uint64_t last_imu_us;
    uint64_t last_touch_us;
    uint64_t last_report_us;
    uint64_t boot_down_us;
    uint64_t last_button_transition_us;
    uint32_t quiet_samples;
    uint32_t reports_published;
    uint32_t movement_reports;
    uint32_t dropped_motion;
    uint32_t recenters;
    uint32_t imu_errors;
    uint32_t imu_stale;
    uint32_t imu_recoveries;
    uint32_t touch_errors;
    uint32_t touch_stale;
    uint32_t touch_recalibrations;
    uint32_t touch_recoveries;
    uint32_t switch_requests;
    uint32_t switch_completions;
    int8_t last_dx;
    int8_t last_dy;
    uint8_t last_buttons;
    bool imu_valid;
    bool imu_faulted;
    bool touch_valid;
    bool touch_faulted;
    bool boot_armed;
    bool boot_down;
    bool inhibit_motion_once;
    bool safety_release_latched;
    bool switch_pending;
    bool switch_rearm;
    bool initialized;
} tm_app_pipeline_t;

typedef struct {
    tm_mouse_transport_id_t selected;
    uint64_t imu_age_us;
    uint64_t touch_age_us;
    uint64_t last_button_transition_us;
    uint32_t reports_published;
    uint32_t movement_reports;
    uint32_t dropped_motion;
    uint32_t recenters;
    uint32_t imu_errors;
    uint32_t imu_stale;
    uint32_t imu_recoveries;
    uint32_t touch_errors;
    uint32_t touch_stale;
    uint32_t touch_recalibrations;
    uint32_t touch_recoveries;
    uint32_t switch_requests;
    uint32_t switch_completions;
    int8_t last_dx;
    int8_t last_dy;
    uint8_t last_buttons;
    bool neutral_valid;
    bool touch_valid;
    bool switch_pending;
    bool switch_rearm;
} tm_app_diagnostics_t;

tm_app_config_t tm_app_default_config(void);
bool tm_app_init(tm_app_pipeline_t *app, const tm_app_config_t *config,
                 tm_mouse_transport_id_t selected, tm_app_output_t output);

/* Only timestamped, successful acquisitions can refresh input freshness. */
bool tm_app_on_imu(tm_app_pipeline_t *app,
                   const tiltmouse_imu_sample_t *sample);
bool tm_app_on_touch(tm_app_pipeline_t *app,
                     const uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT],
                     uint64_t timestamp_us);
void tm_app_imu_error(tm_app_pipeline_t *app);
void tm_app_touch_error(tm_app_pipeline_t *app);

/* BOOT must first be observed released after startup. A deliberate long press
 * followed by release re-centres without disturbing valid button state. */
void tm_app_boot_sample(tm_app_pipeline_t *app, bool pressed, uint64_t now_us);

/* At most one terminal logical report per due slot; false after a latched
 * safety release (service retained release obligations separately). */
bool tm_app_make_report(tm_app_pipeline_t *app, uint64_t now_us,
                        tm_mouse_report_t *report);
bool tm_app_dispatch(tm_app_pipeline_t *app, uint64_t now_us,
                     tm_mouse_publish_result_t *result);
void tm_app_service(tm_app_pipeline_t *app);

/* Runtime handoff is opt-in. The already initialized old transport first
 * quiesces behind an all-up barrier. New output is installed only after that
 * local fence, then waits for a fresh all-buttons-up observation before normal
 * reports resume. Main v0.1 startup does not automatically switch modes. */
bool tm_app_request_output_switch(tm_app_pipeline_t *app, tm_app_output_t next);
void tm_app_diagnostics(
    const tm_app_pipeline_t *app, uint64_t now_us, tm_app_diagnostics_t *out);

#ifdef __cplusplus
}
#endif

#endif
