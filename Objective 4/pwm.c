
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define SPEAKER_PIN 36
#define PWM_CLOCK 1000000

uint speaker_slice;

//initialize PWM for the speaker
void init_speaker(void) {
    gpio_set_function(SPEAKER_PIN, GPIO_FUNC_PWM);

    speaker_slice = pwm_gpio_to_slice_num(SPEAKER_PIN);

    pwm_set_clkdiv(speaker_slice, 150);
    pwm_set_wrap(speaker_slice, 999);
    pwm_set_gpio_level(SPEAKER_PIN, 0);

    pwm_set_enabled(speaker_slice, true);
}

//play note at a given frequency
void play_note(uint frequency, uint duration) {
    if (frequency == 0) {
        pwm_set_gpio_level(SPEAKER_PIN, 0);
        sleep_ms(duration);
        return;
    }

    uint period = PWM_CLOCK / frequency;

    pwm_set_enabled(speaker_slice, false);

    pwm_set_wrap(speaker_slice, period - 1);
    pwm_set_counter(speaker_slice, 0);
    pwm_set_gpio_level(SPEAKER_PIN, period / 2);

    pwm_set_enabled(speaker_slice, true);

    sleep_ms(duration);

    pwm_set_gpio_level(SPEAKER_PIN, 0);
    sleep_ms(50);
}

//sound when basketball scored
void score_sound(void) {
    play_note(523, 100);
    play_note(659, 100);
    play_note(784, 200);
}

//sound for countdown warning
void countdown_sound(void) {
    play_note(880, 150);
}

//sound for game over melody
void game_over_sound(void) {
    play_note(784, 200);
    play_note(659, 200);
    play_note(523, 200);
    play_note(392, 400);
}

int main(void) {
    stdio_init_all();
    init_speaker();

    while (true) {
        score_sound();
        sleep_ms(1000);

        countdown_sound();
        sleep_ms(1000);

        game_over_sound();
        sleep_ms(3000);
    }
}