# Pop-A-Shot (ECE 36200 Final Project, Team 58) — Claude notes

## Project
Electronic Pop-A-Shot arcade basketball on an RP2350 (Purdue "Proton" board), PlatformIO + Pico SDK (C).
Features: game modes, live scoring, countdown timer, saved top-5 high scores per mode.
Build: `~/.platformio/penv/Scripts/pio.exe run` (env `proton`, `-O0`). Upload/debug via picoprobe.

Five subsystems:
1. HC-SR04 ultrasonic sensor (GPIO) — detect ball through hoop  — **this user**
2. TFT LCD scoreboard (SPI)                                      — **this user**
3. MicroSD persistence of high scores (SPI)                      — **this user**
4. Speaker/buzzer music + SFX (PWM)                              — teammate
5. RGB LED strip (README says PIO, user said GPIO)               — teammate

## Scope rules (from user)
- Only edit: `src/display.c`, `include/display.h`, `src/storage.c`, `include/storage.h`,
  `src/sensor.c`, `include/sensor.h`, and this file.
- Do NOT touch `main.c`, `audio.*`, `lights.*`, `platformio.ini`, README without asking.
  (So nothing of ours is called from `main.c` yet except `display_init`/`storage_init`.)

## Decisions (2026-11-18)
- Display: ILI9341 confirmed. Landscape 320x240, RGB565, MADCTL 0x28, `spi0` @ 20 MHz.
- SD: separate bus `spi1`. **No PC access needed -> no filesystem.** High-score table is one
  raw 512-B block (`STORAGE_SCORE_BLOCK` = 2048) with magic/version/FNV checksum.
  Up to 8 modes (name <= 15 chars), top 5 scores each, highest first, ties keep older first.
  API: `storage_submit_score(mode, score)` -> rank 0..4 or -1 (saves if placed);
  `storage_get_top_scores(mode, out[5])`. Works RAM-only if no card.
- Pins: **undecided** — all pin #defines in headers are placeholders.
- UI flow: 3 buttons, one per game mode. Mode select screen -> press mode N -> that mode's
  top-5 screen -> press same button again -> 3-2-1-GO -> game screen (live score + M:SS timer,
  red at <=10 s) -> results (score, NEW HIGH SCORE #k, top-5 with row highlighted) -> back.
  `display_screen_*` functions only draw; game state machine + buttons not ours / not written yet.
- Sensor: **two HC-SR04s, one above and one below the hoop.** Basket = TOP clear->blocked edge,
  then BOTTOM clear->blocked edge within `SENSOR_PASS_WINDOW_MS` (800), plus `SENSOR_HOLDOFF_MS`
  (300) between baskets. Each sensor has its own calibrated empty-hoop baseline; "blocked" =
  closer than baseline by `SENSOR_DETECT_MARGIN_CM`. Pings alternate top/bottom every 15 ms
  (no crosstalk). `sensor_calibrate()` then poll `sensor_poll_basket()` in main loop.
  User wants these tunable — all #defines in `sensor.h`. Untested.
- Game state machine in main.c: user will write it after all subsystem files are done.
- Game modes (names/rules): undecided.

## Status
All three modules compile (2026-11-18); none tested on hardware yet.

## Open questions
- Pin assignments (and which pins teammates use)
- Button pins
- Game mode names + rules (time length, point values)
- Exact sensor mounting/aim (what each sees when empty) -> tune margins/window
