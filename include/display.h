#ifndef POP_A_SHOT_DISPLAY_H
#define POP_A_SHOT_DISPLAY_H

#include <stdint.h>

// 2.2" QVGA SPI TFT (240x320), assumed ILI9341 controller.
// Used in landscape: 320 wide x 240 tall.

// ---- Pin config (PLACEHOLDERS - confirm against Proton board wiring) ----
#define DISPLAY_SPI        spi0
#define DISPLAY_PIN_SCK    18   // module SCK
#define DISPLAY_PIN_MOSI   19   // module SDI(MOSI)
#define DISPLAY_PIN_CS     17   // module CS
#define DISPLAY_PIN_DC     20   // module DC/RS
#define DISPLAY_PIN_RST    21   // module RESET
#define DISPLAY_BAUD_HZ    (20 * 1000 * 1000)
// Module LED (backlight) -> 3.3V. Module SDO(MISO) not used.

#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240

// RGB565 colors
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_ORANGE  0xFD20
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

void display_init(void);
void display_fill_screen(uint16_t color);
void display_fill_rect(int x, int y, int w, int h, uint16_t color);
void display_draw_pixel(int x, int y, uint16_t color);

// Text: 5x7 font in a 6x8 cell, multiplied by `scale` (1 = 6x8 px per char).
// bg == fg draws transparent text (only foreground pixels).
void display_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale);
void display_draw_string(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);
void display_draw_string_centered(int y, const char *s, uint16_t fg, uint16_t bg, int scale);

// ---------------------------------------------------------------------------
// Scoreboard screens
// Flow: mode select -> (mode button) mode info + top 5 -> (same button) get ready
//       3-2-1-GO -> game (live score + timer) -> results + top 5 -> mode select
// The game state machine / buttons live elsewhere; these only draw.
// ---------------------------------------------------------------------------

// Title + numbered list of modes, e.g. {"CLASSIC","SPEED","DOUBLE"}.
// Keep mode names <= 11 chars so they fit on this screen.
void display_screen_mode_select(const char *const modes[], int num_modes);

// One mode's top-5 table and "press again to start".
void display_screen_mode_info(const char *mode, const uint32_t *scores, int count);

// Big pre-game countdown number; n <= 0 shows "GO!".
void display_screen_get_ready(int n);

// Static game layout, then call the two update functions as values change.
void display_screen_game(const char *mode, uint32_t score, int seconds_left);
void display_update_score(uint32_t score);
void display_update_time(int seconds_left);   // turns red at <= 10 s

// Final score, "NEW HIGH SCORE" if rank >= 0 (0-based), top-5 with that row highlighted.
void display_screen_results(const char *mode, uint32_t score, int rank,
                            const uint32_t *scores, int count);

#endif
