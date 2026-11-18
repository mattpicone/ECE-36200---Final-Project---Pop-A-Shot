#include "display.h"

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

// ---- ILI9341 commands ----
#define ILI_SWRESET 0x01
#define ILI_SLPOUT  0x11
#define ILI_DISPON  0x29
#define ILI_CASET   0x2A
#define ILI_PASET   0x2B
#define ILI_RAMWR   0x2C
#define ILI_MADCTL  0x36
#define ILI_PIXFMT  0x3A

// MADCTL: MV (row/col swap = landscape) | BGR panel order
#define MADCTL_LANDSCAPE 0x28

// Classic 5x7 font, ASCII 0x20..0x7E, 5 column bytes per char, bit0 = top row.
static const uint8_t font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, // ' ' ! "
    {0x14,0x7F,0x14,0x7F,0x14}, {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, // # $ %
    {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00}, {0x00,0x1C,0x22,0x41,0x00}, // & ' (
    {0x00,0x41,0x22,0x1C,0x00}, {0x08,0x2A,0x1C,0x2A,0x08}, {0x08,0x08,0x3E,0x08,0x08}, // ) * +
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, // , - .
    {0x20,0x10,0x08,0x04,0x02}, {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, // / 0 1
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10}, // 2 3 4
    {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03}, // 5 6 7
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00}, // 8 9 :
    {0x00,0x56,0x36,0x00,0x00}, {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, // ; < =
    {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06}, {0x32,0x49,0x79,0x41,0x3E}, // > ? @
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22}, // A B C
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x01,0x01}, // D E F
    {0x3E,0x41,0x41,0x51,0x32}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, // G H I
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40}, // J K L
    {0x7F,0x02,0x04,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E}, // M N O
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, // P Q R
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, // S T U
    {0x1F,0x20,0x40,0x20,0x1F}, {0x7F,0x20,0x18,0x20,0x7F}, {0x63,0x14,0x08,0x14,0x63}, // V W X
    {0x03,0x04,0x78,0x04,0x03}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00}, // Y Z [
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04}, // \ ] ^
    {0x40,0x40,0x40,0x40,0x40}, {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, // _ ` a
    {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20}, {0x38,0x44,0x44,0x48,0x7F}, // b c d
    {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x08,0x14,0x54,0x54,0x3C}, // e f g
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x44,0x3D,0x00}, // h i j
    {0x00,0x7F,0x10,0x28,0x44}, {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78}, // k l m
    {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38}, {0x7C,0x14,0x14,0x14,0x08}, // n o p
    {0x08,0x14,0x14,0x18,0x7C}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20}, // q r s
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C}, // t u v
    {0x3C,0x40,0x30,0x40,0x3C}, {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C}, // w x y
    {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00}, {0x00,0x00,0x7F,0x00,0x00}, // z { |
    {0x00,0x41,0x36,0x08,0x00}, {0x08,0x04,0x08,0x10,0x08},                             // } ~
};

// ---------------------------------------------------------------------------
// Low-level SPI helpers
// ---------------------------------------------------------------------------

static inline void cs_select(void)   { gpio_put(DISPLAY_PIN_CS, 0); }
static inline void cs_deselect(void) { gpio_put(DISPLAY_PIN_CS, 1); }

static void write_cmd(uint8_t cmd)
{
    gpio_put(DISPLAY_PIN_DC, 0); // command
    cs_select();
    spi_write_blocking(DISPLAY_SPI, &cmd, 1);
    cs_deselect();
}

static void write_data(const uint8_t *data, size_t len)
{
    gpio_put(DISPLAY_PIN_DC, 1); // data
    cs_select();
    spi_write_blocking(DISPLAY_SPI, data, len);
    cs_deselect();
}

static void write_cmd_data(uint8_t cmd, const uint8_t *data, size_t len)
{
    write_cmd(cmd);
    if (len) write_data(data, len);
}

// Set drawing window and issue RAMWR; caller then streams pixels with DC=1.
static void set_window(int x0, int y0, int x1, int y1)
{
    uint8_t col[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t row[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    write_cmd_data(ILI_CASET, col, 4);
    write_cmd_data(ILI_PASET, row, 4);
    write_cmd(ILI_RAMWR);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void display_init(void)
{
    spi_init(DISPLAY_SPI, DISPLAY_BAUD_HZ);
    spi_set_format(DISPLAY_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(DISPLAY_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(DISPLAY_PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(DISPLAY_PIN_CS);
    gpio_set_dir(DISPLAY_PIN_CS, GPIO_OUT);
    gpio_put(DISPLAY_PIN_CS, 1);

    gpio_init(DISPLAY_PIN_DC);
    gpio_set_dir(DISPLAY_PIN_DC, GPIO_OUT);
    gpio_put(DISPLAY_PIN_DC, 1);

    gpio_init(DISPLAY_PIN_RST);
    gpio_set_dir(DISPLAY_PIN_RST, GPIO_OUT);

    // Hardware reset
    gpio_put(DISPLAY_PIN_RST, 1);
    sleep_ms(5);
    gpio_put(DISPLAY_PIN_RST, 0);
    sleep_ms(20);
    gpio_put(DISPLAY_PIN_RST, 1);
    sleep_ms(150);

    write_cmd(ILI_SWRESET);
    sleep_ms(150);
    write_cmd(ILI_SLPOUT);
    sleep_ms(120);

    uint8_t pixfmt = 0x55;            // 16 bits/pixel (RGB565)
    write_cmd_data(ILI_PIXFMT, &pixfmt, 1);

    uint8_t madctl = MADCTL_LANDSCAPE;
    write_cmd_data(ILI_MADCTL, &madctl, 1);

    write_cmd(ILI_DISPON);
    sleep_ms(20);

    display_fill_screen(COLOR_BLACK);
}

void display_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    // Clip to screen
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    set_window(x, y, x + w - 1, y + h - 1);

    // Stream one row-sized buffer repeatedly
    static uint8_t line[DISPLAY_WIDTH * 2];
    for (int i = 0; i < w; i++) {
        line[2 * i]     = color >> 8;
        line[2 * i + 1] = color & 0xFF;
    }
    gpio_put(DISPLAY_PIN_DC, 1);
    cs_select();
    for (int r = 0; r < h; r++) {
        spi_write_blocking(DISPLAY_SPI, line, w * 2);
    }
    cs_deselect();
}

void display_fill_screen(uint16_t color)
{
    display_fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
}

void display_draw_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
    set_window(x, y, x, y);
    uint8_t px[2] = { color >> 8, color & 0xFF };
    write_data(px, 2);
}

void display_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale)
{
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *glyph = font5x7[c - 0x20];
    bool transparent = (fg == bg);

    // Cell is 6x8 (5x7 glyph + 1px spacing)
    for (int col = 0; col < 6; col++) {
        uint8_t bits = (col < 5) ? glyph[col] : 0x00;
        for (int row = 0; row < 8; row++) {
            bool on = bits & (1 << row);
            if (on) {
                display_fill_rect(x + col * scale, y + row * scale, scale, scale, fg);
            } else if (!transparent) {
                display_fill_rect(x + col * scale, y + row * scale, scale, scale, bg);
            }
        }
    }
}

void display_draw_string(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale)
{
    int start_x = x;
    for (; *s; s++) {
        if (*s == '\n') {
            x = start_x;
            y += 8 * scale;
            continue;
        }
        display_draw_char(x, y, *s, fg, bg, scale);
        x += 6 * scale;
    }
}

void display_draw_string_centered(int y, const char *s, uint16_t fg, uint16_t bg, int scale)
{
    int w = (int)strlen(s) * 6 * scale - scale; // drop trailing spacing column
    display_draw_string((DISPLAY_WIDTH - w) / 2, y, s, fg, bg, scale);
}

// ---------------------------------------------------------------------------
// Scoreboard screens
// ---------------------------------------------------------------------------

#define UI_BG        COLOR_BLACK
#define UI_TITLE     COLOR_ORANGE
#define UI_TEXT      COLOR_WHITE
#define UI_HINT      COLOR_GREEN
#define UI_HIGHLIGHT COLOR_YELLOW

// Game screen layout: left half = score, right half = time
#define GAME_VALUE_Y     88
#define SCORE_SCALE      8      // 48x64 px per char
#define TIME_SCALE       6      // 36x48 px per char
#define LOW_TIME_SECONDS 10

static void draw_top5(int y, const uint32_t *scores, int count, int highlight)
{
    char line[24];
    for (int i = 0; i < 5; i++) {
        if (i < count) {
            snprintf(line, sizeof(line), "%d.  %6lu", i + 1, (unsigned long)scores[i]);
        } else {
            snprintf(line, sizeof(line), "%d.  %6s", i + 1, "---");
        }
        uint16_t fg = (i == highlight) ? UI_HIGHLIGHT : UI_TEXT;
        display_draw_string_centered(y + i * 22, line, fg, UI_BG, 2);
    }
}

void display_screen_mode_select(const char *const modes[], int num_modes)
{
    char line[32];
    display_fill_screen(UI_BG);
    display_draw_string_centered(16, "POP-A-SHOT", UI_TITLE, UI_BG, 4);
    for (int i = 0; i < num_modes; i++) {
        snprintf(line, sizeof(line), "%d  %s", i + 1, modes[i]);
        display_draw_string(60, 80 + i * 36, line, UI_TEXT, UI_BG, 3);
    }
    display_draw_string_centered(212, "Press a mode button", UI_HINT, UI_BG, 2);
}

void display_screen_mode_info(const char *mode, const uint32_t *scores, int count)
{
    display_fill_screen(UI_BG);
    display_draw_string_centered(10, mode, UI_TITLE, UI_BG, 3);
    display_draw_string_centered(48, "HIGH SCORES", UI_TEXT, UI_BG, 2);
    draw_top5(78, scores, count, -1);
    display_draw_string_centered(212, "Press again to start", UI_HINT, UI_BG, 2);
}

void display_screen_get_ready(int n)
{
    char buf[8];
    display_fill_screen(UI_BG);
    if (n > 0) {
        snprintf(buf, sizeof(buf), "%d", n);
        display_draw_string_centered(72, buf, UI_HIGHLIGHT, UI_BG, 12);   // 96 px tall
    } else {
        display_draw_string_centered(84, "GO!", UI_HINT, UI_BG, 10);
    }
}

void display_screen_game(const char *mode, uint32_t score, int seconds_left)
{
    display_fill_screen(UI_BG);
    display_draw_string_centered(6, mode, UI_TITLE, UI_BG, 2);
    display_fill_rect(DISPLAY_WIDTH / 2 - 1, 36, 2, 160, COLOR_WHITE);   // divider
    display_draw_string(80 - 30, 48, "SCORE", UI_TEXT, UI_BG, 2);
    display_draw_string(240 - 24, 48, "TIME", UI_TEXT, UI_BG, 2);
    display_update_score(score);
    display_update_time(seconds_left);
}

void display_update_score(uint32_t score)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)(score > 999 ? 999 : score));
    int w = (int)strlen(buf) * 6 * SCORE_SCALE - SCORE_SCALE;
    display_fill_rect(0, GAME_VALUE_Y, DISPLAY_WIDTH / 2 - 2, 8 * SCORE_SCALE, UI_BG);
    display_draw_string(80 - w / 2, GAME_VALUE_Y, buf, UI_TEXT, UI_BG, SCORE_SCALE);
}

void display_update_time(int seconds_left)
{
    char buf[8];
    if (seconds_left < 0) seconds_left = 0;
    snprintf(buf, sizeof(buf), "%d:%02d", seconds_left / 60, seconds_left % 60);
    uint16_t fg = (seconds_left <= LOW_TIME_SECONDS) ? COLOR_RED : UI_TEXT;
    int w = (int)strlen(buf) * 6 * TIME_SCALE - TIME_SCALE;
    display_fill_rect(DISPLAY_WIDTH / 2 + 2, GAME_VALUE_Y + 8, DISPLAY_WIDTH / 2 - 2, 8 * TIME_SCALE, UI_BG);
    display_draw_string(240 - w / 2, GAME_VALUE_Y + 8, buf, fg, UI_BG, TIME_SCALE);
}

void display_screen_results(const char *mode, uint32_t score, int rank,
                            const uint32_t *scores, int count)
{
    char line[32];
    display_fill_screen(UI_BG);
    display_draw_string_centered(6, "GAME OVER", UI_TITLE, UI_BG, 3);
    snprintf(line, sizeof(line), "%s: %lu", mode, (unsigned long)score);
    display_draw_string_centered(40, line, UI_TEXT, UI_BG, 2);
    if (rank >= 0) {
        snprintf(line, sizeof(line), "NEW HIGH SCORE! #%d", rank + 1);
        display_draw_string_centered(64, line, UI_HIGHLIGHT, UI_BG, 2);
    }
    draw_top5(94, scores, count, rank);
    display_draw_string_centered(218, "Press a mode button", UI_HINT, UI_BG, 2);
}
