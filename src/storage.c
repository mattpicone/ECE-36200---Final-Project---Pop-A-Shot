#include "storage.h"

#include <stddef.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

// SD commands
#define CMD0   0   // GO_IDLE_STATE
#define CMD8   8   // SEND_IF_COND
#define CMD16  16  // SET_BLOCKLEN
#define CMD17  17  // READ_SINGLE_BLOCK
#define CMD24  24  // WRITE_BLOCK
#define CMD55  55  // APP_CMD
#define CMD58  58  // READ_OCR
#define ACMD41 41  // SD_SEND_OP_COND

#define TOKEN_START_BLOCK 0xFE

static bool card_ready = false;
static bool card_is_sdhc = false; // SDHC/SDXC use block addressing, SDSC uses byte addressing

static bool sd_card_init(void);
static void table_load(void);

bool storage_init(void)
{
    bool ok = sd_card_init();
    table_load(); // falls back to an empty table if no card / no valid data
    return ok;
}

// ---------------------------------------------------------------------------
// Low-level SPI helpers
// ---------------------------------------------------------------------------

static uint8_t spi_xfer(uint8_t out)
{
    uint8_t in;
    spi_write_read_blocking(SD_SPI, &out, &in, 1);
    return in;
}

static void cs_high(void)
{
    gpio_put(SD_PIN_CS, 1);
    spi_xfer(0xFF); // extra clocks so the card releases MISO
}

static void cs_low(void)
{
    gpio_put(SD_PIN_CS, 0);
}

// Wait until card stops signalling busy (MISO high). Returns false on timeout.
static bool wait_ready(uint32_t timeout_ms)
{
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (spi_xfer(0xFF) != 0xFF) {
        if (time_reached(deadline)) return false;
    }
    return true;
}

// Send a command frame and return the R1 response (0xFF = no response).
// Leaves CS low so caller can read extra response bytes / data.
static uint8_t send_cmd(uint8_t cmd, uint32_t arg)
{
    if (cmd == ACMD41) {
        uint8_t r = send_cmd(CMD55, 0);
        if (r > 1) return r;
    }

    cs_high();
    cs_low();
    if (cmd != CMD0) wait_ready(500);

    uint8_t crc = 0x01;              // CRC ignored in SPI mode except CMD0/CMD8
    if (cmd == CMD0) crc = 0x95;
    if (cmd == CMD8) crc = 0x87;

    uint8_t frame[6] = {
        0x40 | cmd,
        arg >> 24, arg >> 16, arg >> 8, arg,
        crc,
    };
    spi_write_blocking(SD_SPI, frame, 6);

    uint8_t r1;
    for (int i = 0; i < 10; i++) {   // response arrives within 8 bytes
        r1 = spi_xfer(0xFF);
        if (!(r1 & 0x80)) break;
    }
    return r1;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

static bool sd_card_init(void)
{
    card_ready = false;
    card_is_sdhc = false;

    spi_init(SD_SPI, SD_INIT_BAUD_HZ);
    spi_set_format(SD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SD_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SD_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SD_PIN_MISO, GPIO_FUNC_SPI);
    gpio_pull_up(SD_PIN_MISO);

    gpio_init(SD_PIN_CS);
    gpio_set_dir(SD_PIN_CS, GPIO_OUT);
    gpio_put(SD_PIN_CS, 1);

    sleep_ms(10);

    // >= 74 clocks with CS high to enter native mode
    for (int i = 0; i < 10; i++) spi_xfer(0xFF);

    // Enter SPI mode / idle
    uint8_t r = 0xFF;
    for (int tries = 0; tries < 10 && r != 0x01; tries++) {
        r = send_cmd(CMD0, 0);
    }
    if (r != 0x01) goto fail;

    // Check voltage / card version
    bool v2 = false;
    if (send_cmd(CMD8, 0x1AA) == 0x01) {
        uint8_t r7[4];
        for (int i = 0; i < 4; i++) r7[i] = spi_xfer(0xFF);
        if (r7[2] != 0x01 || r7[3] != 0xAA) goto fail; // voltage not accepted
        v2 = true;
    }

    // Wait for card to leave idle state (up to 1 s)
    absolute_time_t deadline = make_timeout_time_ms(1000);
    do {
        r = send_cmd(ACMD41, v2 ? 0x40000000 : 0);
        if (time_reached(deadline)) goto fail;
    } while (r != 0x00);

    if (v2) {
        // Read OCR, check CCS bit (bit 30) for block addressing
        if (send_cmd(CMD58, 0) != 0x00) goto fail;
        uint8_t ocr[4];
        for (int i = 0; i < 4; i++) ocr[i] = spi_xfer(0xFF);
        card_is_sdhc = (ocr[0] & 0x40) != 0;
    }

    if (!card_is_sdhc) {
        if (send_cmd(CMD16, SD_BLOCK_SIZE) != 0x00) goto fail;
    }

    cs_high();
    spi_set_baudrate(SD_SPI, SD_FAST_BAUD_HZ);
    card_ready = true;
    return true;

fail:
    cs_high();
    return false;
}

bool sd_is_ready(void)
{
    return card_ready;
}

bool sd_read_block(uint32_t block, uint8_t *buf)
{
    if (!card_ready) return false;
    uint32_t addr = card_is_sdhc ? block : block * SD_BLOCK_SIZE;

    bool ok = false;
    if (send_cmd(CMD17, addr) == 0x00) {
        absolute_time_t deadline = make_timeout_time_ms(200);
        uint8_t token;
        do {
            token = spi_xfer(0xFF);
        } while (token == 0xFF && !time_reached(deadline));

        if (token == TOKEN_START_BLOCK) {
            spi_read_blocking(SD_SPI, 0xFF, buf, SD_BLOCK_SIZE);
            spi_xfer(0xFF); // CRC (ignored)
            spi_xfer(0xFF);
            ok = true;
        }
    }
    cs_high();
    return ok;
}

bool sd_write_block(uint32_t block, const uint8_t *buf)
{
    if (!card_ready) return false;
    uint32_t addr = card_is_sdhc ? block : block * SD_BLOCK_SIZE;

    bool ok = false;
    if (send_cmd(CMD24, addr) == 0x00) {
        spi_xfer(0xFF);
        spi_xfer(TOKEN_START_BLOCK);
        spi_write_blocking(SD_SPI, buf, SD_BLOCK_SIZE);
        spi_xfer(0xFF); // dummy CRC
        spi_xfer(0xFF);

        uint8_t resp = spi_xfer(0xFF);
        if ((resp & 0x1F) == 0x05) {      // data accepted
            ok = wait_ready(500);         // wait for internal write to finish
        }
    }
    cs_high();
    return ok;
}

// ---------------------------------------------------------------------------
// High-score table (one raw block)
// ---------------------------------------------------------------------------

#define TABLE_MAGIC   0x504F5053u  // "POPS"
#define TABLE_VERSION 1

typedef struct {
    char     name[STORAGE_MODE_NAME_LEN];
    uint32_t scores[STORAGE_TOP_N];
    uint32_t count;
} mode_entry_t;

typedef struct {
    uint32_t     magic;
    uint32_t     version;
    uint32_t     num_modes;
    mode_entry_t modes[STORAGE_MAX_MODES];
    uint32_t     checksum;   // must stay last
} score_table_t;

_Static_assert(sizeof(score_table_t) <= SD_BLOCK_SIZE, "score table must fit in one block");

static score_table_t table;

static uint32_t table_checksum(const score_table_t *t)
{
    // Simple FNV-1a over everything except the checksum field
    const uint8_t *p = (const uint8_t *)t;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < offsetof(score_table_t, checksum); i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

static void table_reset(void)
{
    memset(&table, 0, sizeof(table));
    table.magic = TABLE_MAGIC;
    table.version = TABLE_VERSION;
}

static void table_load(void)
{
    static uint8_t buf[SD_BLOCK_SIZE];
    if (sd_read_block(STORAGE_SCORE_BLOCK, buf)) {
        memcpy(&table, buf, sizeof(table));
        if (table.magic == TABLE_MAGIC &&
            table.version == TABLE_VERSION &&
            table.num_modes <= STORAGE_MAX_MODES &&
            table.checksum == table_checksum(&table)) {
            return; // valid table on card
        }
    }
    table_reset(); // blank / corrupt / no card
}

static bool table_save(void)
{
    static uint8_t buf[SD_BLOCK_SIZE];
    table.checksum = table_checksum(&table);
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &table, sizeof(table));
    return sd_write_block(STORAGE_SCORE_BLOCK, buf);
}

static mode_entry_t *find_mode(const char *mode, bool create)
{
    for (uint32_t i = 0; i < table.num_modes; i++) {
        if (strncmp(table.modes[i].name, mode, STORAGE_MODE_NAME_LEN - 1) == 0) {
            return &table.modes[i];
        }
    }
    if (!create || table.num_modes >= STORAGE_MAX_MODES) return NULL;

    mode_entry_t *e = &table.modes[table.num_modes++];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, mode, STORAGE_MODE_NAME_LEN - 1);
    return e;
}

int storage_get_top_scores(const char *mode, uint32_t out[STORAGE_TOP_N])
{
    mode_entry_t *e = find_mode(mode, false);
    if (!e) return 0;
    memcpy(out, e->scores, sizeof(e->scores));
    return (int)e->count;
}

int storage_submit_score(const char *mode, uint32_t score)
{
    mode_entry_t *e = find_mode(mode, true);
    if (!e) return -1; // too many distinct modes

    // Find insert position: after any equal-or-higher scores (ties keep older first)
    int pos = 0;
    while (pos < (int)e->count && e->scores[pos] >= score) pos++;
    if (pos >= STORAGE_TOP_N) return -1;

    // Shift lower scores down, dropping the last if full
    int last = (e->count < STORAGE_TOP_N) ? (int)e->count : STORAGE_TOP_N - 1;
    for (int i = last; i > pos; i--) e->scores[i] = e->scores[i - 1];
    e->scores[pos] = score;
    if (e->count < STORAGE_TOP_N) e->count++;

    table_save(); // no-op failure if card missing; RAM copy still updated
    return pos;
}

void storage_clear_scores(void)
{
    table_reset();
    table_save();
}
