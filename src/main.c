#include "pico/stdlib.h"

#include "audio.h"
#include "display.h"
#include "lights.h"
#include "storage.h"

int main(void)
{
    display_init();
    lights_init();
    storage_init();
    audio_init();

    while (true) {
        // TODO: Read inputs and update the game and its outputs.
        lights_update();
        sleep_ms(10);
    }
}
