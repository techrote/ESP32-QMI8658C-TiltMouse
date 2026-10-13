#include "tiltmouse/espnow_health.h"

#include <stddef.h>

#define TM_ESPNOW_FAILURE_THRESHOLD 3u
#define TM_ESPNOW_INITIAL_BACKOFF_US UINT64_C(50000)
#define TM_ESPNOW_MAX_BACKOFF_US UINT64_C(1000000)

tm_espnow_health_config_t tm_espnow_health_default_config(void)
{
    return (tm_espnow_health_config_t){
        .failure_threshold = TM_ESPNOW_FAILURE_THRESHOLD,
        .initial_backoff_us = TM_ESPNOW_INITIAL_BACKOFF_US,
        .max_backoff_us = TM_ESPNOW_MAX_BACKOFF_US,
    };
}

static bool config_valid(const tm_espnow_health_config_t *config)
{
    return config != NULL && config->failure_threshold > 0u &&
           config->initial_backoff_us > 0u &&
           config->max_backoff_us >= config->initial_backoff_us;
}

bool tm_espnow_health_init(
    tm_espnow_health_t *health, const tm_espnow_health_config_t *config)
{
    if (health == NULL || !config_valid(config)) {
        return false;
    }
    *health = (tm_espnow_health_t){0};
    health->config = *config;
    health->backoff_us = config->initial_backoff_us;
    return true;
}

static uint64_t saturating_add(uint64_t a, uint64_t b)
{
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}

static void note_failure(tm_espnow_health_t *health, uint64_t now_us)
{
    if (health->consecutive_failures != UINT32_MAX) {
        ++health->consecutive_failures;
    }
    if (health->consecutive_failures >= health->config.failure_threshold &&
        !health->recovery_pending) {
        health->recovery_pending = true;
        health->next_recovery_us =
            saturating_add(now_us, health->backoff_us);
    }
}

void tm_espnow_health_submit(
    tm_espnow_health_t *health, bool api_accepted, uint64_t now_us)
{
    if (health == NULL) {
        return;
    }
    if (health->send_attempts != UINT32_MAX) {
        ++health->send_attempts;
    }
    if (!api_accepted) {
        if (health->immediate_failures != UINT32_MAX) {
            ++health->immediate_failures;
        }
        note_failure(health, now_us);
    }
}

void tm_espnow_health_callback(
    tm_espnow_health_t *health, bool success, uint64_t now_us)
{
    if (health == NULL) {
        return;
    }
    if (success) {
        if (health->callback_successes != UINT32_MAX) {
            ++health->callback_successes;
        }
        health->consecutive_failures = 0u;
        health->recovery_pending = false;
        health->next_recovery_us = 0u;
        health->backoff_us = health->config.initial_backoff_us;
        return;
    }
    if (health->callback_failures != UINT32_MAX) {
        ++health->callback_failures;
    }
    note_failure(health, now_us);
}

bool tm_espnow_health_recovery_due(
    const tm_espnow_health_t *health, uint64_t now_us)
{
    return health != NULL && health->recovery_pending &&
           now_us >= health->next_recovery_us;
}

void tm_espnow_health_recovery_result(
    tm_espnow_health_t *health, bool success, uint64_t now_us)
{
    if (health == NULL) {
        return;
    }
    if (health->recovery_attempts != UINT32_MAX) {
        ++health->recovery_attempts;
    }
    if (success) {
        if (health->recovery_successes != UINT32_MAX) {
            ++health->recovery_successes;
        }
        health->consecutive_failures = 0u;
        health->recovery_pending = false;
        health->next_recovery_us = 0u;
        health->backoff_us = health->config.initial_backoff_us;
        return;
    }
    if (health->recovery_failures != UINT32_MAX) {
        ++health->recovery_failures;
    }
    health->recovery_pending = true;
    health->next_recovery_us = saturating_add(now_us, health->backoff_us);
    if (health->backoff_us < health->config.max_backoff_us) {
        const uint64_t doubled =
            health->backoff_us > UINT64_MAX / 2u
                ? UINT64_MAX : health->backoff_us * 2u;
        health->backoff_us = doubled > health->config.max_backoff_us
            ? health->config.max_backoff_us : doubled;
    }
}
