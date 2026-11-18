#ifndef POP_A_SHOT_SENSOR_H
#define POP_A_SHOT_SENSOR_H

#include <stdbool.h>

// Two HC-SR04 ultrasonic sensors: one ABOVE the hoop, one BELOW it.
// A basket counts only when the TOP sensor sees the ball and then the BOTTOM
// sensor sees it within SENSOR_PASS_WINDOW_MS (rejects rim bounces / balls
// that never go through, and balls coming back up).
//
// Wiring (each sensor): VCC -> 5V, GND -> GND, TRIG <- GPIO (3.3V is fine),
//   ECHO -> GPIO through a voltage divider (ECHO is a 5V signal, e.g. 1k/2k).

typedef enum {
    SENSOR_TOP = 0,
    SENSOR_BOTTOM = 1,
    SENSOR_COUNT
} sensor_id_t;

// ---- Pin config (PLACEHOLDERS - confirm against Proton board wiring) ----
#define SENSOR_TOP_PIN_TRIG     2
#define SENSOR_TOP_PIN_ECHO     3
#define SENSOR_BOTTOM_PIN_TRIG  4
#define SENSOR_BOTTOM_PIN_ECHO  5

// ---- Detection tuning (adjust once mounted) ----
#define SENSOR_MAX_RANGE_CM      150  // ignore anything farther (also sets echo timeout)
#define SENSOR_PING_INTERVAL_MS  15   // gap between pings; sensors alternate, so each
                                      // sensor is pinged every 2x this (avoids crosstalk)
#define SENSOR_DETECT_MARGIN_CM  8    // must be this much closer than baseline to count
#define SENSOR_PASS_WINDOW_MS    800  // max time from top hit to bottom hit
#define SENSOR_HOLDOFF_MS        300  // min time between counted baskets

void sensor_init(void);

// Fire one ping on one sensor; distance in cm, or -1.0f if nothing in range.
// Blocking: up to ~SENSOR_MAX_RANGE_CM * 58 us (~9 ms).
float sensor_read_distance_cm(sensor_id_t id);

// Measure each sensor's "empty hoop" distance. Call at startup / before each
// game with no ball near the hoop. Blocks ~1 s.
void sensor_calibrate(void);

// Call often from the main loop. Pings at most one sensor per call (each ping
// blocks up to ~9 ms). Returns true exactly once per ball through the hoop.
bool sensor_poll_basket(void);

// Debug helpers
float sensor_get_baseline_cm(sensor_id_t id);       // -1 if nothing seen in calibration
float sensor_get_last_distance_cm(sensor_id_t id);  // -1 if last ping saw nothing
bool  sensor_is_blocked(sensor_id_t id);            // ball currently in that beam

#endif
