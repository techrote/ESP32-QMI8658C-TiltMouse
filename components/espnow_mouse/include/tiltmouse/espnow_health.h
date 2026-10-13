#ifndef TILTMOUSE_ESPNOW_HEALTH_H
#define TILTMOUSE_ESPNOW_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t failure_threshold;
    uint64_t initial_backoff_us;
    uint64_t max_backoff_us;
} tm_espnow_health_config_t;

typedef struct {
    tm_espnow_health_config_t config;
    uint32_t consecutive_failures;
    uint32_t send_attempts;
    uint32_t immediate_failures;
    uint32_t callback_successes;
    uint32_t callback_failures;
    uint32_t recovery_attempts;
    uint32_t recovery_successes;
    uint32_t recovery_failures;
    uint64_t next_recovery_us;
    uint64_t backoff_us;
    bool recovery_pending;
} tm_espnow_health_t;

tm_espnow_health_config_t tm_espnow_health_default_config(void);
bool tm_espnow_health_init(
    tm_espnow_health_t *health, const tm_espnow_health_config_t *config);
void tm_espnow_health_submit(
    tm_espnow_health_t *health, bool api_accepted, uint64_t now_us);
void tm_espnow_health_callback(
    tm_espnow_health_t *health, bool success, uint64_t now_us);
bool tm_espnow_health_recovery_due(
    const tm_espnow_health_t *health, uint64_t now_us);
void tm_espnow_health_recovery_result(
    tm_espnow_health_t *health, bool success, uint64_t now_us);

#ifdef __cplusplus
}
#endif

#endif
