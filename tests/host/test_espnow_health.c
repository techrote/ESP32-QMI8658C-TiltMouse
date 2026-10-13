#include <assert.h>
#include <stdint.h>

#include "tiltmouse/espnow_health.h"

static void test_threshold_and_success_cancel(void)
{
    tm_espnow_health_t health;
    const tm_espnow_health_config_t config = {
        .failure_threshold = 3,
        .initial_backoff_us = 50,
        .max_backoff_us = 400,
    };
    assert(tm_espnow_health_init(&health, &config));
    tm_espnow_health_submit(&health, false, 10);
    tm_espnow_health_callback(&health, false, 20);
    assert(!health.recovery_pending);
    tm_espnow_health_submit(&health, false, 30);
    assert(health.recovery_pending);
    assert(health.next_recovery_us == 80);
    assert(!tm_espnow_health_recovery_due(&health, 79));
    assert(tm_espnow_health_recovery_due(&health, 80));

    tm_espnow_health_callback(&health, true, 81);
    assert(!health.recovery_pending);
    assert(health.consecutive_failures == 0);
    assert(health.backoff_us == 50);
    assert(health.callback_successes == 1);
}

static void test_bounded_exponential_recovery(void)
{
    tm_espnow_health_t health;
    const tm_espnow_health_config_t config = {
        .failure_threshold = 1,
        .initial_backoff_us = 50,
        .max_backoff_us = 200,
    };
    assert(tm_espnow_health_init(&health, &config));
    tm_espnow_health_submit(&health, false, 100);
    assert(health.next_recovery_us == 150);
    tm_espnow_health_recovery_result(&health, false, 150);
    assert(health.recovery_attempts == 1 && health.recovery_failures == 1);
    assert(health.next_recovery_us == 200);
    assert(health.backoff_us == 100);
    tm_espnow_health_recovery_result(&health, false, 200);
    assert(health.next_recovery_us == 300);
    assert(health.backoff_us == 200);
    tm_espnow_health_recovery_result(&health, false, 300);
    assert(health.next_recovery_us == 500);
    assert(health.backoff_us == 200);

    tm_espnow_health_recovery_result(&health, true, 500);
    assert(!health.recovery_pending);
    assert(health.recovery_successes == 1);
    assert(health.backoff_us == 50);
}

static void test_default_and_invalid_config(void)
{
    tm_espnow_health_t health;
    const tm_espnow_health_config_t def = tm_espnow_health_default_config();
    assert(def.failure_threshold == 3);
    assert(def.initial_backoff_us == UINT64_C(50000));
    assert(def.max_backoff_us == UINT64_C(1000000));
    assert(tm_espnow_health_init(&health, &def));
    assert(!tm_espnow_health_init(NULL, &def));
    tm_espnow_health_config_t bad = def;
    bad.failure_threshold = 0;
    assert(!tm_espnow_health_init(&health, &bad));
}

int main(void)
{
    test_threshold_and_success_cancel();
    test_bounded_exponential_recovery();
    test_default_and_invalid_config();
    return 0;
}
