#ifndef POP_A_SHOT_STORAGE_H
#define POP_A_SHOT_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

// MicroSD card over SPI (slot on the back of the 2.2" TFT module).
// No filesystem: the high-score table lives in one raw 512-byte block.
// NOTE: this ignores/overwrites whatever is at that block, so use a dedicated card.

// ---- Pin config (PLACEHOLDERS - confirm against Proton board wiring) ----
#define SD_SPI        spi1
#define SD_PIN_MISO   12   // module SD_MISO
#define SD_PIN_CS     13   // module SD_CS
#define SD_PIN_SCK    14   // module SD_SCK
#define SD_PIN_MOSI   15   // module SD_MOSI
#define SD_INIT_BAUD_HZ  (400 * 1000)          // required <= 400 kHz during init
#define SD_FAST_BAUD_HZ  (12 * 1000 * 1000)    // after init

#define SD_BLOCK_SIZE 512

// ---- High-score table ----
#define STORAGE_SCORE_BLOCK   2048  // raw block number holding the table
#define STORAGE_MAX_MODES     8     // distinct game-mode strings remembered
#define STORAGE_MODE_NAME_LEN 16    // incl. terminating '\0'
#define STORAGE_TOP_N         5

// Init SD card and load the score table. Returns true if the card works.
// If false, scores still work in RAM for this power cycle but are not saved.
bool storage_init(void);

// Copy the top scores for `mode` (highest first) into out[STORAGE_TOP_N].
// Returns how many entries are valid (0..5). Unknown mode -> 0.
int storage_get_top_scores(const char *mode, uint32_t out[STORAGE_TOP_N]);

// Offer a finished game's score. If it makes the top 5 it is inserted in order
// and the table is saved to the SD card.
// Returns the rank achieved (0 = 1st .. 4 = 5th), or -1 if it didn't place.
int storage_submit_score(const char *mode, uint32_t score);

// Wipe all high scores (RAM + card).
void storage_clear_scores(void);

// Raw block layer
bool sd_is_ready(void);
bool sd_read_block(uint32_t block, uint8_t *buf);        // buf: 512 bytes
bool sd_write_block(uint32_t block, const uint8_t *buf); // buf: 512 bytes

#endif
