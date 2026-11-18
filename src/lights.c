#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "lights.h"
#include "lights_ws2812.pio.h"

// Set these after choosing the wiring. -1 leaves the lights disabled.
#ifndef LIGHTS_PIN
#define LIGHTS_PIN (-1)     // Choose an unused GPIO from 0 to 31.
#endif
#ifndef LIGHTS_COUNT
#define LIGHTS_COUNT 60      // Full 60-LED strip; use a suitable external 5 V supply.
#endif
#define LIGHTS_BRIGHTNESS 32 // 0 to 255. Start dim.

static PIO lights_pio = pio0;
static uint lights_sm;
static bool initialized = false;

static uint32_t flash_color;
static int changes_left = 0;
static bool lights_on;
static uint32_t interval_us;
static uint64_t next_change;

// Send the same GRB color to every LED. Each LED takes 24 bits, MSB first.
static void lights_fill(uint32_t color) {
    for(int i = 0; i < LIGHTS_COUNT; i++){
        pio_sm_put_blocking(lights_pio, lights_sm, color << 8);
    }

    // An empty FIFO can still leave one LED's word in the shift register.
    while(!pio_sm_is_tx_fifo_empty(lights_pio, lights_sm)){
        tight_loop_contents();
    }
    // Allow 30 us for that word, then 300 us LOW to latch (>280 us in V5).
    sleep_us(330);
}

void lights_init(void) {
    if(initialized || LIGHTS_PIN < 0){
        return;
    }
    // This simple setup uses PIO0's default GPIO range, 0 through 31.
    hard_assert(LIGHTS_PIN <= 31 && LIGHTS_COUNT > 0);
    uint pin = LIGHTS_PIN;

    // Claim one state machine and load the four-instruction PIO program.
    lights_sm = pio_claim_unused_sm(lights_pio, true);
    uint offset = pio_add_program(lights_pio, &lights_ws2812_program);

    pio_gpio_init(lights_pio, pin);
    pio_sm_set_pins_with_mask(lights_pio, lights_sm, 0, 1u << pin);
    pio_sm_set_consecutive_pindirs(lights_pio, lights_sm, pin, 1, true);

    pio_sm_config c = lights_ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, pin);
    // Shift left, automatically pulling a new word after 24 bits.
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // 32 PIO cycles per bit, 800,000 bits per second.
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (800000 * 32));
    pio_sm_init(lights_pio, lights_sm, offset, &c);
    pio_sm_set_enabled(lights_pio, lights_sm, true);

    initialized = true;
    sleep_us(300);
    lights_off();
}

void lights_off(void) {
    changes_left = 0;
    if(initialized){
        lights_fill(0);
    }
}

// Start a flash sequence. A new event replaces the previous sequence.
static void lights_flash(uint8_t red, uint8_t green, uint8_t blue,
                         int flashes, int milliseconds) {
    if(!initialized){
        return;
    }

    flash_color = ((uint32_t)green << 16) | ((uint32_t)red << 8) | blue;
    changes_left = 2 * flashes - 1;
    interval_us = milliseconds * 1000;
    lights_on = true;
    lights_fill(flash_color);
    next_change = time_us_64() + interval_us;
}

// Call from the main loop so the game can keep running between flashes.
void lights_update(void) {
    if(!initialized || changes_left == 0 || time_us_64() < next_change){
        return;
    }

    lights_on = !lights_on;
    changes_left--;
    lights_fill(lights_on ? flash_color : 0);
    next_change = time_us_64() + interval_us;
}

void lights_score(void) {
    lights_flash(0, LIGHTS_BRIGHTNESS, 0, 2, 150);
}

void lights_countdown_warning(void) {
    lights_flash(LIGHTS_BRIGHTNESS, 0, 0, 3, 100);
}

void lights_game_over(void) {
    lights_flash(0, 0, LIGHTS_BRIGHTNESS, 6, 150);
}
