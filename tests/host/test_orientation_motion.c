#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tiltmouse/mouse_motion.h"
#include "tiltmouse/orientation.h"

static void assert_close(float actual, float expected, float tolerance)
{
    assert(fabsf(actual - expected) <= tolerance);
}

static tiltmouse_imu_sample_t sample_at(
    uint64_t timestamp_us,
    float ax,
    float ay,
    float az,
    float gx,
    float gy,
    float gz)
{
    return (tiltmouse_imu_sample_t){
        .timestamp_us = timestamp_us,
        .accel_g = {ax, ay, az},
        .gyro_dps = {gx, gy, gz},
    };
}

static void test_stationary_bias_is_bounded(void)
{
    const tiltmouse_orientation_config_t config = tiltmouse_orientation_default_config();
    tiltmouse_orientation_state_t state;
    tiltmouse_orientation_reset(&state, &config);

    tiltmouse_imu_sample_t sample = sample_at(0, 0, 0, 1, 0.3f, -0.2f, 99.0f);
    assert(tiltmouse_orientation_update(&state, &sample) ==
           TILTMOUSE_ORIENTATION_INITIALIZED);

    for (uint64_t t = 10000; t <= UINT64_C(10000000); t += 10000) {
        sample.timestamp_us = t;
        assert(tiltmouse_orientation_update(&state, &sample) ==
               TILTMOUSE_ORIENTATION_OK);
    }

    assert(fabsf(state.roll_deg) < 0.2f);
    assert(fabsf(state.pitch_deg) < 0.2f);
}

static void test_variable_dt_and_gap_guard(void)
{
    const tiltmouse_orientation_config_t config = tiltmouse_orientation_default_config();
    tiltmouse_orientation_state_t state;
    tiltmouse_orientation_reset(&state, &config);

    tiltmouse_imu_sample_t sample = sample_at(1000, 0, 0, 1, 0, 0, 0);
    assert(tiltmouse_orientation_update(&state, &sample) ==
           TILTMOUSE_ORIENTATION_INITIALIZED);

    const uint32_t dts_us[] = {4000, 7000, 5000, 11000, 3000};
    float pitch = 0.0f;
    uint64_t timestamp = sample.timestamp_us;
    for (size_t i = 0; i < sizeof(dts_us) / sizeof(dts_us[0]); ++i) {
        const float dt = (float)dts_us[i] / 1000000.0f;
        pitch += 40.0f * dt;
        const float rad = pitch * 0.01745329251994329577f;
        timestamp += dts_us[i];
        sample = sample_at(timestamp, -sinf(rad), 0, cosf(rad), 0, 40.0f, 0);
        assert(tiltmouse_orientation_update(&state, &sample) ==
               TILTMOUSE_ORIENTATION_OK);
    }
    assert_close(state.pitch_deg, pitch, 0.02f);

    const float before_gap = state.pitch_deg;
    timestamp += 200000;
    sample = sample_at(timestamp, 0, 0, 1, 0, 500.0f, 0);
    assert(tiltmouse_orientation_update(&state, &sample) ==
           TILTMOUSE_ORIENTATION_GAP_REBASED);
    assert_close(state.pitch_deg, 0.0f, 0.01f);
    assert(fabsf(state.pitch_deg - before_gap) < 10.0f);

    const tiltmouse_orientation_state_t snapshot = state;
    sample.timestamp_us = timestamp;
    assert(tiltmouse_orientation_update(&state, &sample) ==
           TILTMOUSE_ORIENTATION_ERR_NON_MONOTONIC_TIME);
    assert(memcmp(&state, &snapshot, sizeof(state)) == 0);
}

static void test_dynamic_acceleration_is_rejected(void)
{
    const tiltmouse_orientation_config_t config = tiltmouse_orientation_default_config();
    tiltmouse_orientation_state_t state;
    tiltmouse_orientation_reset(&state, &config);

    tiltmouse_imu_sample_t sample = sample_at(0, 0, 0, 1, 0, 0, 0);
    assert(tiltmouse_orientation_update(&state, &sample) ==
           TILTMOUSE_ORIENTATION_INITIALIZED);

    sample = sample_at(10000, 0, 1.0f, 1.0f, 0, 0, 0);
    assert(tiltmouse_orientation_update(&state, &sample) ==
           TILTMOUSE_ORIENTATION_OK);
    assert_close(state.roll_deg, 0.0f, 0.001f);
    assert_close(state.pitch_deg, 0.0f, 0.001f);
}

static void test_neutral_capture_and_reset(void)
{
    const tiltmouse_orientation_config_t config = tiltmouse_orientation_default_config();
    tiltmouse_orientation_state_t state;
    tiltmouse_orientation_reset(&state, &config);

    const float angle = 12.0f * 0.01745329251994329577f;
    tiltmouse_imu_sample_t sample =
        sample_at(0, 0, sinf(angle), cosf(angle), 0, 0, 0);
    assert(tiltmouse_orientation_update(&state, &sample) ==
           TILTMOUSE_ORIENTATION_INITIALIZED);
    assert(tiltmouse_orientation_capture_neutral(&state));

    tiltmouse_orientation_pose_t pose = tiltmouse_orientation_pose(&state);
    assert(pose.neutral_valid);
    assert_close(pose.roll_deg - pose.neutral_roll_deg, 0.0f, 0.001f);

    tiltmouse_orientation_clear_neutral(&state);
    pose = tiltmouse_orientation_pose(&state);
    assert(!pose.neutral_valid);

    tiltmouse_orientation_reset(&state, &config);
    assert(!state.initialized);
    assert(!state.neutral_valid);
}

static tiltmouse_mouse_motion_config_t unit_motion_config(void)
{
    return (tiltmouse_mouse_motion_config_t){
        .x = {
            .source = TILTMOUSE_MOTION_AXIS_ROLL,
            .invert = false,
            .deadzone_deg = 2.0f,
            .max_tilt_deg = 12.0f,
            .response_exponent = 2.0f,
            .gain_counts_per_s = 100.0f,
            .max_velocity_counts_per_s = 60.0f,
        },
        .y = {
            .source = TILTMOUSE_MOTION_AXIS_PITCH,
            .invert = true,
            .deadzone_deg = 2.0f,
            .max_tilt_deg = 12.0f,
            .response_exponent = 2.0f,
            .gain_counts_per_s = 100.0f,
            .max_velocity_counts_per_s = 60.0f,
        },
        .max_report_dt_s = 0.1f,
        .max_delta_per_report = 127,
    };
}

static void test_mapping_deadzone_curve_axis_and_clamp(void)
{
    tiltmouse_mouse_motion_config_t config = unit_motion_config();
    tiltmouse_orientation_pose_t pose = {
        .roll_deg = 7.0f,
        .pitch_deg = 3.0f,
        .neutral_roll_deg = 5.0f,
        .neutral_pitch_deg = 5.0f,
        .neutral_valid = true,
    };

    tiltmouse_mouse_velocity_t velocity;
    assert(tiltmouse_mouse_motion_velocity(&config, &pose, &velocity) ==
           TILTMOUSE_MOUSE_MOTION_OK);
    assert_close(velocity.x_counts_per_s, 0.0f, 0.0001f);
    assert_close(velocity.y_counts_per_s, 0.0f, 0.0001f);

    pose.roll_deg = 12.0f;
    pose.pitch_deg = 12.0f;
    assert(tiltmouse_mouse_motion_velocity(&config, &pose, &velocity) ==
           TILTMOUSE_MOUSE_MOTION_OK);
    assert_close(velocity.x_counts_per_s, 25.0f, 0.001f);
    assert_close(velocity.y_counts_per_s, -25.0f, 0.001f);

    pose.roll_deg = -50.0f;
    pose.pitch_deg = -50.0f;
    assert(tiltmouse_mouse_motion_velocity(&config, &pose, &velocity) ==
           TILTMOUSE_MOUSE_MOTION_OK);
    assert_close(velocity.x_counts_per_s, -60.0f, 0.001f);
    assert_close(velocity.y_counts_per_s, 60.0f, 0.001f);

    pose.neutral_valid = false;
    assert(tiltmouse_mouse_motion_velocity(&config, &pose, &velocity) ==
           TILTMOUSE_MOUSE_MOTION_NOT_READY);
    assert_close(velocity.x_counts_per_s, 0.0f, 0.0001f);
    assert_close(velocity.y_counts_per_s, 0.0f, 0.0001f);
}

static void test_fractional_accumulation_and_recentre_reset(void)
{
    tiltmouse_mouse_motion_config_t config = unit_motion_config();
    config.x.deadzone_deg = 0.0f;
    config.x.max_tilt_deg = 10.0f;
    config.x.response_exponent = 1.0f;
    config.x.gain_counts_per_s = 10.0f;
    config.x.max_velocity_counts_per_s = 10.0f;
    config.y = config.x;
    config.y.source = TILTMOUSE_MOTION_AXIS_PITCH;
    config.max_report_dt_s = 0.1f;

    tiltmouse_orientation_pose_t pose = {
        .roll_deg = 2.5f,
        .pitch_deg = 0.0f,
        .neutral_roll_deg = 0.0f,
        .neutral_pitch_deg = 0.0f,
        .neutral_valid = true,
    };

    tiltmouse_mouse_motion_state_t state = {0};
    tiltmouse_mouse_delta_t delta;
    tiltmouse_mouse_velocity_t velocity;

    for (int i = 0; i < 3; ++i) {
        assert(tiltmouse_mouse_motion_step(
                   &state, &config, &pose, 0.1f, &delta, &velocity) ==
               TILTMOUSE_MOUSE_MOTION_OK);
        assert(delta.x == 0);
    }
    assert_close(state.residual_x, 0.75f, 0.0001f);

    assert(tiltmouse_mouse_motion_step(
               &state, &config, &pose, 0.1f, &delta, &velocity) ==
           TILTMOUSE_MOUSE_MOTION_OK);
    assert(delta.x == 1);
    assert_close(state.residual_x, 0.0f, 0.0001f);

    for (int i = 0; i < 3; ++i) {
        assert(tiltmouse_mouse_motion_step(
                   &state, &config, &pose, 0.1f, &delta, &velocity) ==
               TILTMOUSE_MOUSE_MOTION_OK);
    }
    assert_close(state.residual_x, 0.75f, 0.0001f);

    tiltmouse_mouse_motion_reset(&state);
    assert_close(state.residual_x, 0.0f, 0.0001f);
    pose.neutral_roll_deg = pose.roll_deg;
    assert(tiltmouse_mouse_motion_step(
               &state, &config, &pose, 0.1f, &delta, &velocity) ==
           TILTMOUSE_MOUSE_MOTION_OK);
    assert(delta.x == 0);
    assert_close(state.residual_x, 0.0f, 0.0001f);
}

static void test_report_delta_clamp_discards_excess_backlog(void)
{
    tiltmouse_mouse_motion_config_t config = unit_motion_config();
    config.x.deadzone_deg = 0.0f;
    config.x.max_tilt_deg = 1.0f;
    config.x.response_exponent = 1.0f;
    config.x.gain_counts_per_s = 1000.0f;
    config.x.max_velocity_counts_per_s = 1000.0f;
    config.max_report_dt_s = 1.0f;
    config.max_delta_per_report = 10;

    tiltmouse_orientation_pose_t pose = {
        .roll_deg = 20.0f,
        .pitch_deg = 0.0f,
        .neutral_roll_deg = 0.0f,
        .neutral_pitch_deg = 0.0f,
        .neutral_valid = true,
    };

    tiltmouse_mouse_motion_state_t state = {0};
    tiltmouse_mouse_delta_t delta;
    tiltmouse_mouse_velocity_t velocity;

    assert(tiltmouse_mouse_motion_step(
               &state, &config, &pose, 1.0f, &delta, &velocity) ==
           TILTMOUSE_MOUSE_MOTION_OK);
    assert(delta.x == 10);
    assert_close(state.residual_x, 0.0f, 0.0001f);
}

static void test_replay_fixture(void)
{
    FILE *file = fopen(IMU_FIXTURE_PATH, "r");
    assert(file != NULL);

    char line[256];
    assert(fgets(line, sizeof(line), file) != NULL);

    const tiltmouse_orientation_config_t config = tiltmouse_orientation_default_config();
    tiltmouse_orientation_state_t state;
    tiltmouse_orientation_reset(&state, &config);

    size_t count = 0;
    float peak_pitch = 0.0f;
    while (fgets(line, sizeof(line), file) != NULL) {
        char phase[32];
        unsigned long long timestamp_us = 0;
        float ax, ay, az, gx, gy, gz, expected_pitch;
        const int fields = sscanf(
            line,
            "%31[^,],%llu,%f,%f,%f,%f,%f,%f,%f",
            phase,
            &timestamp_us,
            &ax,
            &ay,
            &az,
            &gx,
            &gy,
            &gz,
            &expected_pitch);
        assert(fields == 9);

        const tiltmouse_imu_sample_t sample =
            sample_at((uint64_t)timestamp_us, ax, ay, az, gx, gy, gz);
        const tiltmouse_orientation_status_t status =
            tiltmouse_orientation_update(&state, &sample);
        assert(status == TILTMOUSE_ORIENTATION_OK ||
               status == TILTMOUSE_ORIENTATION_INITIALIZED);
        assert_close(state.pitch_deg, expected_pitch, 0.05f);

        if (state.pitch_deg > peak_pitch) {
            peak_pitch = state.pitch_deg;
        }
        ++count;
    }

    fclose(file);
    assert(count > 30);
    assert(peak_pitch > 19.9f);
    assert_close(state.pitch_deg, 0.0f, 0.05f);
}

int main(void)
{
    test_stationary_bias_is_bounded();
    test_variable_dt_and_gap_guard();
    test_dynamic_acceleration_is_rejected();
    test_neutral_capture_and_reset();
    test_mapping_deadzone_curve_axis_and_clamp();
    test_fractional_accumulation_and_recentre_reset();
    test_report_delta_clamp_discards_excess_backlog();
    test_replay_fixture();
    return 0;
}
