#include "sensor.h"

#include <stdint.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define US_PER_CM_ROUNDTRIP   58.0f
#define ECHO_START_TIMEOUT_US 3000   // echo normally rises ~0.5 ms after trigger
#define ECHO_WIDTH_TIMEOUT_US ((uint32_t)(SENSOR_MAX_RANGE_CM * US_PER_CM_ROUNDTRIP))

typedef struct {
    uint     trig_pin;
    uint     echo_pin;
    float    baseline_cm;   // empty-hoop distance, -1 = nothing in range
    float    last_cm;
    bool     blocked;       // object currently in beam
} hcsr04_t;

static hcsr04_t sensors[SENSOR_COUNT] = {
    [SENSOR_TOP]    = { SENSOR_TOP_PIN_TRIG,    SENSOR_TOP_PIN_ECHO,    -1.0f, -1.0f, false },
    [SENSOR_BOTTOM] = { SENSOR_BOTTOM_PIN_TRIG, SENSOR_BOTTOM_PIN_ECHO, -1.0f, -1.0f, false },
};

static sensor_id_t next_sensor = SENSOR_TOP;
static uint32_t last_ping_ms = 0;
static uint32_t top_hit_ms = 0;
static bool     top_hit_pending = false;
static uint32_t last_basket_ms = 0;

static inline uint32_t now_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

void sensor_init(void)
{
    for (int i = 0; i < SENSOR_COUNT; i++) {
        gpio_init(sensors[i].trig_pin);
        gpio_set_dir(sensors[i].trig_pin, GPIO_OUT);
        gpio_put(sensors[i].trig_pin, 0);

        gpio_init(sensors[i].echo_pin);
        gpio_set_dir(sensors[i].echo_pin, GPIO_IN);
        gpio_pull_down(sensors[i].echo_pin);
    }
}

float sensor_read_distance_cm(sensor_id_t id)
{
    const hcsr04_t *s = &sensors[id];

    // If a previous echo is still high (no-object echoes can last ~38 ms),
    // the sensor won't accept a new trigger yet.
    if (gpio_get(s->echo_pin)) return -1.0f;

    // 10 us trigger pulse
    gpio_put(s->trig_pin, 1);
    sleep_us(10);
    gpio_put(s->trig_pin, 0);

    // Wait for echo to go high
    uint64_t t0 = time_us_64();
    while (!gpio_get(s->echo_pin)) {
        if (time_us_64() - t0 > ECHO_START_TIMEOUT_US) return -1.0f;
    }

    // Measure how long echo stays high; give up past max range
    uint64_t start = time_us_64();
    while (gpio_get(s->echo_pin)) {
        if (time_us_64() - start > ECHO_WIDTH_TIMEOUT_US) return -1.0f;
    }
    uint64_t width_us = time_us_64() - start;

    return (float)width_us / US_PER_CM_ROUNDTRIP;
}

void sensor_calibrate(void)
{
    float sum[SENSOR_COUNT] = {0};
    int   n[SENSOR_COUNT] = {0};

    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < SENSOR_COUNT; i++) {   // alternate to avoid crosstalk
            float d = sensor_read_distance_cm((sensor_id_t)i);
            if (d > 0.0f) {
                sum[i] += d;
                n[i]++;
            }
            sleep_ms(50);
        }
    }
    for (int i = 0; i < SENSOR_COUNT; i++) {
        sensors[i].baseline_cm = (n[i] > 0) ? sum[i] / n[i] : -1.0f;
        sensors[i].blocked = false;
    }
    top_hit_pending = false;
}

// Ping one sensor, update its blocked state. Returns true on clear->blocked edge.
static bool update_sensor(sensor_id_t id)
{
    hcsr04_t *s = &sensors[id];
    float d = sensor_read_distance_cm(id);
    s->last_cm = d;

    // "Object in beam" = valid reading noticeably closer than the empty-hoop baseline.
    // If calibration saw nothing (open space), any in-range reading counts.
    bool object;
    if (d <= 0.0f) {
        object = false;
    } else if (s->baseline_cm < 0.0f) {
        object = true;
    } else {
        object = d < (s->baseline_cm - SENSOR_DETECT_MARGIN_CM);
    }

    bool rising = object && !s->blocked;
    s->blocked = object;
    return rising;
}

bool sensor_poll_basket(void)
{
    uint32_t t = now_ms();
    if (t - last_ping_ms < SENSOR_PING_INTERVAL_MS) return false;
    last_ping_ms = t;

    sensor_id_t id = next_sensor;
    next_sensor = (next_sensor == SENSOR_TOP) ? SENSOR_BOTTOM : SENSOR_TOP;

    // Expire a stale top hit (ball hit top beam but never came through)
    if (top_hit_pending && (t - top_hit_ms) > SENSOR_PASS_WINDOW_MS) {
        top_hit_pending = false;
    }

    if (!update_sensor(id)) return false;

    if (id == SENSOR_TOP) {
        top_hit_ms = t;
        top_hit_pending = true;
        return false;
    }

    // Bottom edge: counts only if top saw it first, recently
    if (top_hit_pending && (t - last_basket_ms) >= SENSOR_HOLDOFF_MS) {
        top_hit_pending = false;
        last_basket_ms = t;
        return true;
    }
    return false;
}

float sensor_get_baseline_cm(sensor_id_t id)
{
    return sensors[id].baseline_cm;
}

float sensor_get_last_distance_cm(sensor_id_t id)
{
    return sensors[id].last_cm;
}

bool sensor_is_blocked(sensor_id_t id)
{
    return sensors[id].blocked;
}
