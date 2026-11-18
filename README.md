# Team 58: Pop-a-Shot

ECE 36200 final project: an arcade basketball game with a graphical scoreboard,
event lighting, saved scores, and sound.

## Objectives

- [ ] **Objective 1:** An ability to use SPI to interface with a TFT LCD and implement a graphical basketball scoreboard that includes: countdowns, gamemodes, results, live scoring.
- [ ] **Objective 2:** An ability to use PIO to control an RGB LED strip and implement lighting animations for specific events like: scoring, countdown warning, game completion.
- [ ] **Objective 3:** An ability to use SPI to interface with an SD card to create persistent storage of player scores and game statistics by using a file system.
- [ ] **Objective 4:** An ability to use PWM to synthesize arcade music and sound effects and play it on a speaker.

## Lights

Purchased strip: BTF-LIGHTING WS2812B RGB, 5 V, 60 LEDs.
This setup uses **all 60 LEDs** with a suitable external 5 V supply.
This code targets 24-bit GRB WS2812B strips, not RGBW strips.

1. Set `LIGHTS_PIN` in `src/lights.c` to an unused **GPIO 0-31**. It is currently
   `-1`, which disables lights without touching a pin. Use the GPIO number printed
   on the Proton, not a header position.
2. The full strip is configured with `LIGHTS_COUNT` set to 60. If you cut the strip,
   change this to the number of LEDs in the connected section. Start with
   `LIGHTS_BRIGHTNESS` at 32 out of 255.
3. Call the event function **once when the event happens**, from the main loop:
   `lights_score()` flashes green twice; `lights_countdown_warning()` flashes red
   three times; `lights_game_over()` flashes blue six times.

`main.c` already calls `lights_update()` each loop to advance the animation.
`lights_off()` cancels it and turns the strip off. A new event replaces the current
animation. The code waits only while sending each frame (about 2.1 ms for 60 LEDs),
not during the 100-150 ms intervals between flashes. Keep these calls on the main
loop; the driver is not designed for concurrent calls or interrupt handlers.

For all 60 LEDs, use a separate regulated **5 V supply** sized to the strip's
specifications and connect its ground to Proton GND. Budget up to **3.6 A** for
60 conventional WS2812B LEDs at full white (a 5 V, 4 A supply covers that budget).
Connect the supply's **+5 V** to strip **5V**, supply **GND** to strip **GND**
and Proton **GND**, and the chosen GPIO to strip **DIN**. Follow printed labels,
not wire colors. Do not power the full strip from GPIO, 3V3, or the Proton's 5V pin.
Do not connect the external supply's 5 V output to the USB-powered Proton's 5V pin.
A **74AHCT125 level shifter** is needed if the delivered chip revision does not
accept the Proton's 3.3 V data signal; software cannot correct that voltage mismatch.
Follow the [NeoPixel wiring guide](https://learn.adafruit.com/adafruit-neopixel-uberguide/best-practices)
for a series data resistor, supply capacitor, and power wiring.

`src/lights_ws2812.pio` generates the data timing in hardware. Its generated
`.pio.h` is included because the course PlatformIO setup may skip generation.
Keep both files in Git. If you edit the PIO program, regenerate the header with
`pioasm -o c-sdk src/lights_ws2812.pio src/lights_ws2812.pio.h` before building.
Build with the PlatformIO checkmark or `pio run`.
References:
[RP2350 datasheet, chapter 11 (PIO)](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf),
[Worldsemi WS2812B V5 datasheet, pages 3-4 (timing and GRB order)](https://files.keeb.supply/products/ws2812b-rgb-led/datasheet.pdf),
and [Raspberry Pi's PIO WS2812 example](https://github.com/raspberrypi/pico-examples/blob/master/pio/ws2812/ws2812.pio).
The vendor does not specify the chip revision, so confirm the supplied strip's
datasheet when buying. Physical LED behavior still needs testing on hardware.

