/*
 * frs.c -- fast RSSI scan.
 *
 * The MAC asks for a scan over a set of channels in one band.  For each
 * channel the receiver is tuned, the AGC is allowed to settle (or a fixed gain
 * is forced), the 17-bin RSSI correlator is triggered and its bins are stored
 * around the channel position.  Up to three passes with channel offsets
 * -1/0/+1 are merged: the strongest valid reading wins.  The merged RSSI
 * (quarter dB, after the SysCfg trim) and the gain used land in the MAC
 * mailbox.
 */
#include "btp/btp.h"
#include "btp/frs.h"
#include "btp/nvv.h"
#include "btp/syscfg.h"
#include "btp/clock.h"
#include "btp/rtkit.h"
#include "btp/trace.h"

struct frs_state frs BTP_STATE(frs);

/* Channels covered by each band. */
static const uint8_t frs_band_span[BTP_BAND_COUNT] = { 79, 101, 126, 76, 126, 125, 125, 125 };

/* Default channel lists (used when the MAC does not supply a bitmap). */
static const uint8_t frs_channels_5[5] BTP_ROM(frs_channels_5) = { 8, 24, 40, 56, 72 };
static const uint8_t frs_channels_7[7] BTP_ROM(frs_channels_7) = { 8, 24, 40, 56, 72, 88, 104 };
static const uint8_t frs_channels_8a[8] BTP_ROM(frs_channels_8a) = { 8, 24, 40, 56, 72, 88, 104, 120 };
static const uint8_t frs_channels_5b[5] BTP_ROM(frs_channels_5b) = { 8, 24, 40, 56, 72 };
static const uint8_t frs_channels_8b[8] BTP_ROM(frs_channels_8b) = { 8, 24, 40, 56, 72, 88, 104, 120 };
static const uint8_t frs_channels_8c[8] BTP_ROM(frs_channels_8c) = { 8, 24, 40, 56, 72, 88, 104, 120 };
static const uint8_t frs_channels_8d[8] BTP_ROM(frs_channels_8d) = { 8, 24, 40, 56, 72, 88, 104, 120 };
static const uint8_t frs_channels_8e[8] BTP_ROM(frs_channels_8e) = { 8, 24, 40, 56, 72, 88, 104, 120 };

const uint8_t *frs_default_channel_lists[BTP_BAND_COUNT] BTP_DATA(frs_default_channel_lists) = {
    frs_channels_5, frs_channels_7, frs_channels_8a, frs_channels_5b,
    frs_channels_8b, frs_channels_8c, frs_channels_8d, frs_channels_8e,
};
static const uint8_t frs_default_channel_counts[BTP_BAND_COUNT] = { 5, 7, 8, 5, 8, 8, 8, 8 };

/* Max-gain calibration used when NVV_TAG_RX_MAXGAIN_CAL is absent: two words
 * per band in the NVV packing (three 10-bit fields each). */
static const uint32_t frs_maxgain_fallback[BTP_BAND_COUNT][2] = {
    { 0x339D0B4D, 0x2E6BBEFA }, { 0x354D7768, 0x2EFBF70D },
    { 0x339D0F4E, 0x2FAC1712 }, { 0x335CFF4B, 0x2F9C1711 },
    { 0x331CEF47, 0x2F7C0B0E }, { 0x32DCDF44, 0x2F7C0B0D },
    { 0x328CCF3F, 0x2F4BFB0A }, { 0x323CBB3A, 0x2F3BF707 },
};

/* Forced-gain settings selectable by the MAC's gain index 0..12. */
static const uint8_t frs_fixed_gain_index[FRS_GAIN_INDEX_MAX + 1] = {
    65, 60, 55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 5,
};
static const uint16_t frs_fixed_gain_code[FRS_GAIN_INDEX_MAX + 1] = {
    0x00FE, 0x00EA, 0x00CE, 0x00CD, 0x008D, 0x0094, 0x40C0,
    0x401C, 0x4008, 0x500C, 0x5800, 0x6808, 0x7318,
};

/* Reported in place of a reading that carries a status code. */
static const int16_t frs_status_sample[4] = { 0x7FFF, 0x7FFE, 0x7FFD, 0x7FFC };
static const char *const frs_status_name[4] = { "inv", "clp", "pll", "agc" };

/* The native scan's literal pool names these three adjacent ROM strings. */
struct frs_scan_messages {
    char pll[13];
    char agc[16];
    char timeout[14];
};
static const struct frs_scan_messages frs_scan_log BTP_ROM(frs_scan_log) = {
    "PLL NOT OK!\n", "AGC NOT OK! %x\n", "FRS timeout!\n",
};
_Static_assert(sizeof frs_scan_log == 43, "FRS scan message wire size");

/* Scan timing (SysTick cycles). */
#define FRS_PLL_SETTLE_CYCLES       0xF
#define FRS_AGC_START_CYCLES        4
#define FRS_AGC_TIMEOUT_CYCLES      0xD
#define FRS_TRIGGER_CYCLES          0xE
#define FRS_SCAN_TIMEOUT_CYCLES     0xC9
#define FRS_ENTER_SETTLE_SINGLE     0x19
#define FRS_ENTER_SETTLE_MULTI      0x21

/* Channel ranges with their own SysCfg RSSI trim. */
#define FRS_LOW_RANGE_END           0xF
#define FRS_HIGH_RANGE_START        0x4C

/* Gains at or below this get the low-gain RSSI compensation. */
#define FRS_LOW_GAIN_LIMIT          0x2C

/* Slots in frs.saved_regs[]. */
enum frs_saved_reg {
    FRS_SAVED_TIMING0 = 0,
    FRS_SAVED_THRESHOLD0,
    FRS_SAVED_TIMING1,
    FRS_SAVED_THRESHOLD1,
    FRS_SAVED_WINDOW,
    FRS_SAVED_SEQ_CFG,
    FRS_SAVED_SEQ_CTRL,
    FRS_SAVED_SEQ_RF_CTRL,
    FRS_SAVED_SEQ_CHAN,
    FRS_SAVED_FRS_MASK,
    FRS_SAVED_RADIO_GAIN0,
    FRS_SAVED_RADIO_GAIN1,
    FRS_SAVED_RSSI_CAL1,
    FRS_SAVED_GAIN_FORCE,
    FRS_SAVED_AGC_CFG,
    FRS_SAVED_FRS_CFG0,
    FRS_SAVED_FRS_CFG1,
    FRS_SAVED_IQMM,
    FRS_SAVED_AGC_TARGET,
    FRS_SAVED_DEMOD_CTRL,
    FRS_SAVED_RXFE_B,
    FRS_SAVED_RXFE_A,
};

/* Rows are indexed from FRS_SAMPLE_GUARD so that channel - half_span can go
 * negative. */
static inline int16_t *frs_rssi_row(unsigned pass)
{
    return &frs.rssi_rows[pass][FRS_SAMPLE_GUARD];
}

static inline uint8_t *frs_gain_row(unsigned pass)
{
    return &frs.gain_rows[pass][FRS_SAMPLE_GUARD];
}

/* ---- one scan pass ---------------------------------------------------------- */

#ifdef BTP_HOST
static void frs_tune(int channel)
{
    uint32_t rf_ctrl;

    REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_CHANNEL, (uint8_t)channel + SEQ_CHAN_FRS_CHANNEL_BIAS);

    /* pulse the channel latch */
    rf_ctrl = SEQ_RF_CTRL;
    SEQ_RF_CTRL = rf_ctrl & ~SEQ_RF_CTRL_CHAN_LATCH;
    REG_SET_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_CHAN_LATCH);
    systick_delay(FRS_PLL_SETTLE_CYCLES);
}

static bool frs_pll_locked(void)
{
    enum radio_bank bank = frs.band < 2 ? RADIO_BANK_B : RADIO_BANK_A;
    uint32_t first = RADIO_SYNTH_STATUS(bank);
    uint32_t second = RADIO_SYNTH_STATUS(bank);

    /* The two failure bits are sampled by different loads at 0x0DF4/0x0DF6. */
    return ((first & BIT(18)) | (second & BIT(17))) == 0;
}

struct frs_gain_table {
    uint32_t base;
    uint32_t address;
};

/* Run the AGC and hold its result; returns the AGC state reached. */
static unsigned frs_run_agc(uint8_t *gain_row, int channel, struct frs_gain_table *table)
{
    uint32_t start, status;
    unsigned state, gain_index, table_sel;

    REG_CLEAR_BITS(AGC_CFG, AGC_CFG_RESET);
    REG_SET_BITS(AGC_CFG, AGC_CFG_RESET);
    REG_CLEAR_BITS(AGC_CFG, AGC_CFG_RESET);

    REG_WRITE_FIELD(AGC_CTRL, AGC_CTRL_MODE, 0xC);
    REG_SET_BITS(AGC_CTRL, AGC_CTRL_START);

    start = systick_now();
    systick_delay(FRS_AGC_START_CYCLES);
    do {
        status = AGC_STATUS;
        state = FIELD_GET(AGC_STATUS_STATE, status);
        if (state == AGC_STATE_SETTLED)
            break;
    } while (systick_elapsed(systick_now(), start) < FRS_AGC_TIMEOUT_CYCLES);

    gain_index = FIELD_GET(AGC_STATUS_GAIN_INDEX, status);
    table_sel = FIELD_GET(AGC_STATUS_TABLE_SEL, status);

    /* Selector zero reuses the previous table. Initial host values are supplied
     * by the adapter; only the target entry below preserves native stack bytes. */
    switch (table_sel) {
    case 1:
        table->base = FIELD_GET(RX_GAIN_TABLE_BASE1, RX_GAIN_TABLE_BASE);
        table->address = RXDIG_BASE + 0x500;
        break;
    case 2:
        table->base = FIELD_GET(RX_GAIN_TABLE_BASE2, RX_GAIN_TABLE_BASE);
        table->address = RXDIG_BASE + 0x600;
        break;
    case 3:
        table->base = FIELD_GET(RX_GAIN_TABLE_BASE3, RX_GAIN_TABLE_BASE);
        table->address = RXDIG_BASE + 0x700;
        break;
    default: break;
    }

    /* Freeze the gain the AGC settled on. */
    uint32_t gain_word = MMIO32(table->address + 4u * (table->base - gain_index));
    REG_WRITE_FIELD(RX_GAIN_FORCE, RX_GAIN_FORCE_WORD, gain_word);
    REG_WRITE_FIELD(RX_GAIN_FORCE, RX_GAIN_FORCE_INDEX, gain_index);
    REG_WRITE_FIELD(AGC_CFG, AGC_CFG_GAIN_MODE, AGC_GAIN_MODE_HOLD);

    memset(&gain_row[channel - frs.half_span], (int)gain_index, 2u * frs.half_span + 1);
    return state;
}

/* Trigger the correlator and wait for it to finish. */
static void frs_trigger_and_wait(void)
{
    uint32_t start;

    REG_CLEAR_BITS(FRS_TRIGGER, FRS_TRIGGER_START);
    REG_SET_BITS(FRS_TRIGGER, FRS_TRIGGER_START);
    REG_CLEAR_BITS(FRS_TRIGGER, FRS_TRIGGER_START);

    systick_delay(FRS_TRIGGER_CYCLES);
    start = systick_now();
    while (FRS_STATUS & FRS_STATUS_BUSY) {
        if (systick_elapsed(systick_now(), start) >= FRS_SCAN_TIMEOUT_CYCLES) {
            dbg_printf(frs_scan_log.timeout);
            break;
        }
    }
}

/* Unpack the correlator output: eight words of two 10-bit bins (sign-extended
 * to 16 bits by OR-ing the top six bits) around the centre bin. */
static void frs_read_bins(int16_t *bins)
{
    for (unsigned n = 0; n < 8; n++) {
        uint32_t pair = FRS_RSSI_PAIR(n) | 0xFC00FC00u;
        unsigned first = n < 4 ? 2 * n : 2 * n + 1;

        bins[first] = (int16_t)(pair & 0xFFFF);
        bins[first + 1] = (int16_t)(pair >> 16);
    }
}

static void frs_fill_bins(int16_t *bins, int16_t value)
{
    for (unsigned i = 0; i < FRS_BINS; i++)
        bins[i] = value;
}

/* Store the bins covering channel +- half_span; bins are indexed high to low. */
static void frs_store_bins(int16_t *rssi_row, int channel, const int16_t *bins)
{
    unsigned span = 2u * frs.half_span;

    for (unsigned k = 0; k <= span; k++)
        rssi_row[channel - frs.half_span + k] = bins[FRS_BINS / 2 + frs.half_span - k];
}

/* @provenance RVA 0x0D7C frs_scan_channel_pass */
int frs_scan_pass(int16_t *rssi_row, const uint8_t *channels, int count, int offset)
{
    unsigned pass = offset == 0 ? 0 : (offset < 0 ? 1 : 2);
    uint8_t *gain_row = frs_gain_row(pass);
    int16_t bins[FRS_BINS];
    struct frs_gain_table table = { 0, RXDIG_BASE + 0x500 };

    while (count != 0) {
        int channel = offset + *channels++;
        uint32_t center;

        if (!frs.single_channel) {
            frs_tune(channel);
            if (!frs_pll_locked()) {
                dbg_printf(frs_scan_log.pll);
                frs_fill_bins(bins, FRS_STATUS_PLL_UNLOCKED);
                goto store;
            }
        }

        if (frs.fixed_gain_word != 0) {
            memset(&gain_row[channel - frs.half_span], frs.fixed_gain_index,
                   2u * frs.half_span + 1);
        } else {
            unsigned state = frs_run_agc(gain_row, channel, &table);

            if (state != AGC_STATE_SETTLED) {
                dbg_printf(frs_scan_log.agc, state);
                frs_fill_bins(bins, FRS_STATUS_AGC_FAILED);
                goto store;
            }
        }

        frs_trigger_and_wait();
        frs_read_bins(bins);
        if (frs.fixed_gain_word == 0)
            REG_WRITE_FIELD(AGC_CFG, AGC_CFG_GAIN_MODE, AGC_GAIN_MODE_AUTO);

        center = FRS_RSSI_CENTER;
        if ((center & FRS_RSSI_CENTER_FLAGS_MASK) == FRS_RSSI_CENTER_CLIPPED) {
            frs_fill_bins(bins, FRS_STATUS_CLIPPED);
            goto store;
        }
        bins[FRS_BINS / 2] = frs.center_bin_enable
                          ? (int16_t)(FRS_RSSI_CENTER | 0xFC00) : 0;

store:
        count--;
        frs_store_bins(rssi_row, channel, bins);
    }
    return 0;
}
#else
/* These outlined helpers use the original register contracts. The elapsed
 * helper takes now in R0 and start in R5; the clear helpers take a base in R1
 * and preserve R2. Their bodies are checked against the reference instructions. */
/* @provenance RVA 0x1BA8 _frs_outlined_elapsed_systick */
__attribute__((naked, used, aligned(4)))
static void frs_elapsed_systick_arm(void)
{
    __asm__ volatile(
        "mov r1, r0\n"
        "mov r0, r5\n"
        "b.w .Lfrs_elapsed_core\n"
        ".Lfrs_elapsed_core:\n"
        "cmp r0, r1\n"
        "itt hs\n"
        "subhs r0, r0, r1\n"
        "bxhs lr\n"
        "subs r0, r0, r1\n"
        "ldr r1, .Lfrs_elapsed_reload\n"
        "ldr r1, [r1]\n"
        "add r0, r1\n"
        "bx lr\n"
        "nop\n"
        ".Lfrs_elapsed_reload:\n"
        ".word 0x4D289908\n");
}

/* @provenance RVA 0x1BB0 _frs_outlined_clear_agc_bit */
__attribute__((naked, used))
static void frs_clear_agc_arm(void)
{
    __asm__ volatile(
        "ldr.w r0, [r1, #0x10C]\n"
        "bic.w r0, r0, #0x40\n"
        "str.w r0, [r1, #0x10C]\n"
        "bx lr\n");
}

/* @provenance RVA 0x1BBE _frs_outlined_clear_trigger_bit */
__attribute__((naked, used))
static void frs_clear_trigger_arm(void)
{
    __asm__ volatile(
        "ldr.w r0, [r1, #0x88]\n"
        "bic.w r0, r0, #1\n"
        "str.w r0, [r1, #0x88]\n"
        "bx lr\n");
}

/* The 112-byte native frame retains the first selector-zero stack slots and
 * their updates across channels. C uninitialized locals cannot express this.
 * The host implementation above is an ordinary-memory adapter. */
__attribute__((naked, aligned(4)))
int frs_scan_pass(int16_t *rssi_row, const uint8_t *channels, int count, int offset)
{
    __asm__ volatile(
#include "frs_scan_arm.inc"
    );
}
#endif

/* @provenance RVA 0x1064 frs_unpack_channel_bitmap */
uint32_t frs_unpack_channel_bitmap(const uint8_t *bitmap, int bytes, uint8_t *channels,
                                   int *count)
{
    int limit = *count;
    int n = 0;
    uint8_t base = 0;

    frs.channel_list = NULL;
    frs.channel_count = 0;

    for (; bytes != 0; bytes--, base += 8) {
        uint8_t bits = *bitmap++;

        for (unsigned i = 0; bits != 0 && i < 8; i++, bits >>= 1) {
            if (bits & 1) {
                if (n >= limit)
                    return (uint32_t)-1;
                channels[n++] = (uint8_t)(base + i);
            }
        }
    }

    *count = n;
    frs.channel_list = channels;
    frs.channel_count = (uint32_t)n;
    return 0;
}

/* @provenance RVA 0x1120 frs_run_multipass_scan */
uint32_t frs_run_scan(const uint8_t *channels, int count)
{
    if (frs.band >= BTP_BAND_COUNT)
        return (uint32_t)-1;

    if (channels == NULL) {
        channels = frs_default_channel_lists[frs.band];
        if (channels == NULL)
            return (uint32_t)-2;
        count = frs_default_channel_counts[frs.band];
    }

    if (frs.pass_count == 0)
        return 0;

    frs_scan_pass(frs_rssi_row(0), channels, count, 0);
    if (frs.pass_count >= 2) {
        frs_scan_pass(frs_rssi_row(1), channels, count, -1);
        if (frs.pass_count >= 3)
            frs_scan_pass(frs_rssi_row(2), channels, count, 1);
    }
    return 0;
}

/* ---- result processing ------------------------------------------------------- */

struct frs_trim {
    int16_t low;        /* channels below FRS_LOW_RANGE_END */
    int16_t mid;
    int16_t high;       /* channels above FRS_HIGH_RANGE_START */
};

static struct frs_trim frs_load_trim(unsigned band)
{
    struct frs_trim trim = { 0, 0, 0 };

    if (syscfg.rssi_cal != NULL) {
        const int8_t *cal = &syscfg.rssi_cal[3 * band];

        trim.low = (int16_t)(4 * cal[0]);
        trim.mid = (int16_t)(4 * cal[1]);
        trim.high = (int16_t)(4 * cal[2]);
    }
    return trim;
}

static int16_t frs_trim_for(const struct frs_trim *trim, unsigned channel)
{
    if (channel > FRS_HIGH_RANGE_START - 1)
        return trim->high;
    if (channel < FRS_LOW_RANGE_END)
        return trim->low;
    return trim->mid;
}

/* Convert a raw reading into the reported sample. */
static int16_t frs_calibrate_sample(int16_t raw, uint8_t gain, int16_t trim)
{
    int16_t value;

    if (raw >= 0)
        return (uint16_t)raw > 3 ? FRS_SAMPLE_BAD_STATUS : frs_status_sample[raw];

    value = (int16_t)(raw + trim);
    if (gain <= FRS_LOW_GAIN_LIMIT) {
        if ((trim - 0xF0) - 4 * gain > value)
            value = (int16_t)(value + 4 * (gain & 0x3F) - 0xB4);
        if (value <= FRS_SAMPLE_FLOOR)
            value = FRS_SAMPLE_FLOOR;
    }
    return value;
}

/* @provenance RVA 0x11B4 frs_process_and_calibrate_results */
uint32_t frs_process_results(int16_t *rssi_out, uint8_t *gain_out)
{
    struct frs_trim trim;
    unsigned band = frs.band;

    if (frs.pass_count == 0 || band >= BTP_BAND_COUNT)
        return (uint32_t)-1;

    frs.result_gain = gain_out;
    frs.result_rssi = rssi_out;
    trim = frs_load_trim(band);

    if (frs.pass_count == 1) {
        /* Single pass: report the bins around each scanned channel. */
        while (frs.channel_count-- != 0) {
            int channel = *frs.channel_list;
            int first = channel - frs.half_span;

            if (first < 0)
                first = 0;
            for (int k = first; k <= channel + frs.half_span; k++) {
                uint8_t gain = frs_gain_row(0)[k];

                gain_out[k] = gain;
                rssi_out[k] = frs_calibrate_sample(frs_rssi_row(0)[k], gain,
                                                   frs_trim_for(&trim, (unsigned)k));
            }
            frs.channel_list++;
        }
        return 0;
    }

    /* Several passes: for every channel of the band take the strongest valid
     * reading across the passes (any reading beats a status code). */
    for (unsigned k = 0; k < frs_band_span[band]; k++) {
        int16_t best = frs_rssi_row(0)[k];
        uint8_t gain = frs_gain_row(0)[k];

        for (unsigned pass = 1; pass < frs.pass_count; pass++) {
            int16_t sample = frs_rssi_row(pass)[k];

            if (best >= 0 || (sample < 0 && sample > best)) {
                best = sample;
                gain = frs_gain_row(pass)[k];
            }
        }
        gain_out[k] = gain;
        rssi_out[k] = frs_calibrate_sample(best, gain, frs_trim_for(&trim, k));
    }
    return 0;
}

static const char *frs_sample_name(int16_t sample)
{
    if (sample < 0)
        return frs_format_rssi(sample, FRS_RSSI_SCALE);
    return (uint16_t)sample <= 3 ? frs_status_name[sample] : "bad";
}

/* @provenance RVA 0x1430 frs_dump_rssi_readings */
uint32_t frs_dump_readings(unsigned band)
{
    uint32_t limit = frs_band_span[band] + FRS_SAMPLE_GUARD;

    dbg_printf("RSSI readings:\n");
    dbg_printf("%4s %15s     %15s", "Ch", "#1", "#2");
    dbg_printf("%15s     %15s\n", "#3", "##");

    for (unsigned ch = 0; ch + FRS_SAMPLE_GUARD < limit; ch++) {
        uint16_t second = (uint16_t)frs_rssi_row(1)[ch];
        const char *s1, *s2, *s3, *s4;

        s1 = frs_sample_name(frs_rssi_row(0)[ch]);
        dbg_printf("%4d %15s|%3d ", ch, s1, frs_gain_row(0)[ch]);

        /* the second pass is only ever shown as a status */
        s2 = second <= 3 ? frs_status_name[(int16_t)second] : "bad";
        s3 = frs_sample_name(frs_rssi_row(2)[ch]);
        s4 = frs_format_rssi(frs.result_rssi[ch], FRS_RSSI_SCALE);
        dbg_printf("%15s|%3d %15s|%3d %15s|%3d\n", s2, frs_gain_row(1)[ch],
                   s3, frs_gain_row(2)[ch], s4, frs.result_gain[ch]);
    }
    return limit;
}

/* ---- scan setup / teardown -------------------------------------------------- */

/* @provenance RVA 0x1584 frs_configure_scan_params */
bool frs_configure(uint8_t band, uint32_t half_span, uint32_t passes, bool allow_single)
{
    bool single = false;

    frs.band = band;
    frs.half_span = (uint8_t)MIN(half_span, FRS_HALF_SPAN_MAX);

    /* passes above 4 request the centre bin as well */
    frs.center_bin_enable = passes > 4;
    if (passes > 4)
        passes -= 4;
    frs.pass_count = (uint8_t)passes;

    if (allow_single)
        single = frs.channel_count == 1 && (uint8_t)passes == 1;
    frs.single_channel = single;
    return single;
}

/* Receiver offset and gain from the max-gain calibration word pair. */
static void frs_apply_maxgain(unsigned band)
{
    const uint32_t *cal = nvv.rx_maxgain_cal != NULL
                        ? (const uint32_t *)nvv.rx_maxgain_cal
                        : frs_maxgain_fallback[0];
    uint32_t level = (cal[2 * band] >> 18) & 0xFFC;        /* 4 x third field */
    uint32_t gain_ref = (cal[2 * band + 1] >> 18) & 0xFFC;
    uint32_t offset = level < 0x5DC ? 0x7F : (0xFC - level / 12);
    int ref = band == 0 ? 0xD2 : 0xD0;

    REG_WRITE_FIELD(FRS_CTRL, FRS_CTRL_OFFSET, offset);
    REG_WRITE_FIELD(FRS_CTRL, FRS_CTRL_GAIN, (uint8_t)cpu_ssat(ref - (int)(gain_ref / 12), 8));
}

/* @provenance RVA 0x15E0 frs_save_state_and_enter */
uint32_t frs_enter(uint32_t gain_index, int16_t *rssi_out, uint8_t *gain_out)
{
    uint32_t *saved = frs.saved_regs;
    unsigned band = frs.band;
    uint32_t chan, start, settle;

    saved[FRS_SAVED_SEQ_RF_CTRL] = SEQ_RF_CTRL;
    SEQ_RF_CTRL = FIELD_REPLACE(saved[FRS_SAVED_SEQ_RF_CTRL],
                                SEQ_RF_CTRL_FRS_MODE, SEQ_RF_CTRL_FRS_MODE_SCAN);
    saved[FRS_SAVED_RSSI_CAL1] = RX_RSSI_CAL1;
    RX_RSSI_CAL1 = saved[FRS_SAVED_RSSI_CAL1] & 0xFF00FF00u;

    phy_set_synth_band(band != 0, false, band);

    saved[FRS_SAVED_SEQ_CHAN] = SEQ_CHAN;
    chan = saved[FRS_SAVED_SEQ_CHAN]
         & ~(RFIELD_MASK(SEQ_CHAN_CHANNEL) | RFIELD_MASK(SEQ_CHAN_BAND));
    chan |= FIELD_PREP(SEQ_CHAN_BAND, band);
    if (frs.single_channel) {
        SEQ_CHAN = chan | FIELD_PREP(SEQ_CHAN_CHANNEL,
                                     *frs.channel_list + SEQ_CHAN_FRS_CHANNEL_BIAS);
        REG_CLEAR_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_CHAN_LATCH);
        REG_SET_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_CHAN_LATCH);
    } else {
        SEQ_CHAN = chan;
    }

    saved[FRS_SAVED_SEQ_CTRL] = SEQ_CTRL;
    SEQ_CTRL = saved[FRS_SAVED_SEQ_CTRL] | SEQ_CTRL_FRS_MODE_BITS;
    saved[FRS_SAVED_SEQ_CFG] = SEQ_CFG;
    SEQ_CFG = FIELD_REPLACE(saved[FRS_SAVED_SEQ_CFG], SEQ_CFG_FRS_MODE, SEQ_CFG_FRS_TONE_CODE);

    REG_WRITE_FIELD(FRS_CTRL, FRS_CTRL_MODE, 1);

    saved[FRS_SAVED_RADIO_GAIN1] = RADIO_FRS_GAIN1;
    RADIO_FRS_GAIN1 = FIELD_REPLACE(saved[FRS_SAVED_RADIO_GAIN1], RADIO_FRS_GAIN_CODE,
                                    frs.single_channel ? 0x2C : 0x39);
    saved[FRS_SAVED_RADIO_GAIN0] = RADIO_FRS_GAIN0;
    RADIO_FRS_GAIN0 = FIELD_REPLACE(saved[FRS_SAVED_RADIO_GAIN0], RADIO_FRS_GAIN_CODE,
                                    frs.single_channel ? 0x28 : 0x36);

    saved[FRS_SAVED_DEMOD_CTRL] = RX_DEMOD_CTRL;
    RX_DEMOD_CTRL = saved[FRS_SAVED_DEMOD_CTRL] & ~RX_DEMOD_CTRL_WIDEBAND;
    saved[FRS_SAVED_RXFE_B] = RADIO_RXFE_CTRL(RADIO_BANK_B);
    saved[FRS_SAVED_RXFE_A] = RADIO_RXFE_CTRL(RADIO_BANK_A);
    REG_CLEAR_BITS(RADIO_RXFE_CTRL(RADIO_BANK_B), RADIO_RXFE_CTRL_LNA_BYPASS);
    REG_CLEAR_BITS(RADIO_RXFE_CTRL(RADIO_BANK_A), RADIO_RXFE_CTRL_LNA_BYPASS);
    saved[FRS_SAVED_IQMM] = RX_IQMM;
    RX_IQMM = saved[FRS_SAVED_IQMM] | RX_IQMM_BYPASS;
    REG_CLEAR_BITS(RADIO_SYNTH_FREQ(RADIO_BANK_B), RADIO_SYNTH_FREQ_FROM_SEQ);
    REG_CLEAR_BITS(RADIO_SYNTH_FREQ(RADIO_BANK_A), RADIO_SYNTH_FREQ_FROM_SEQ);

    if (frs.single_channel) {
        saved[FRS_SAVED_FRS_CFG0] = RX_FRS_CFG0;
        RX_FRS_CFG0 = (saved[FRS_SAVED_FRS_CFG0] & RX_FRS_CFG0_KEEP_MASK) | RX_FRS_CFG0_VALUE;
        saved[FRS_SAVED_FRS_CFG1] = RX_FRS_CFG1;
        RX_FRS_CFG1 = (saved[FRS_SAVED_FRS_CFG1] & RX_FRS_CFG1_KEEP_MASK) | RX_FRS_CFG1_VALUE;
    }

    saved[FRS_SAVED_FRS_MASK] = RX_FRS_MASK;
    RX_FRS_MASK = saved[FRS_SAVED_FRS_MASK] | RX_FRS_MASK_ALL;
    saved[FRS_SAVED_THRESHOLD0] = RX_FRS_THRESHOLD0;
    RX_FRS_THRESHOLD0 = FIELD_REPLACE(saved[FRS_SAVED_THRESHOLD0], RX_FRS_THRESHOLD_VALUE, 0x30);
    saved[FRS_SAVED_THRESHOLD1] = RX_FRS_THRESHOLD1;
    RX_FRS_THRESHOLD1 = FIELD_REPLACE(saved[FRS_SAVED_THRESHOLD1], RX_FRS_THRESHOLD_VALUE, 0x30);
    saved[FRS_SAVED_TIMING0] = RX_FRS_TIMING0;
    RX_FRS_TIMING0 = FIELD_REPLACE(saved[FRS_SAVED_TIMING0], RX_FRS_TIMING_VALUE, 0x8F9);
    saved[FRS_SAVED_TIMING1] = RX_FRS_TIMING1;
    RX_FRS_TIMING1 = FIELD_REPLACE(saved[FRS_SAVED_TIMING1], RX_FRS_TIMING_VALUE, 0x8F9);
    saved[FRS_SAVED_WINDOW] = RX_FRS_WINDOW;
    RX_FRS_WINDOW = FIELD_REPLACE(saved[FRS_SAVED_WINDOW], RX_FRS_WINDOW_LEN, 0x1E);
    REG_SET_BITS(SEQ_CFG, SEQ_CFG_TONE_RUN);

    frs_apply_maxgain(band);

    if (nvv.frs_iqmm_ab_mix1st_bw3 != NULL && nvv.frs_iqmm_ab_lna1st_bw3 != NULL) {
        const uint16_t *mix = (const uint16_t *)nvv.frs_iqmm_ab_mix1st_bw3;
        const uint16_t *lna = (const uint16_t *)nvv.frs_iqmm_ab_lna1st_bw3;

        REG_WRITE_FIELD(RX_IQMM_SCALE, RX_IQMM_SCALE_VALUE, mix[band]);
        REG_WRITE_FIELD(RX_IQMM_PHASE, RX_IQMM_PHASE_VALUE, mix[band + 8]);
        REG_WRITE_FIELD(RX_IQMM, RX_IQMM_A, lna[band]);
        REG_WRITE_FIELD(RX_IQMM, RX_IQMM_B, lna[band + 8]);
    }

    /* Clear the result area while the receiver settles. */
    start = systick_now();
    if (rssi_out != NULL) {
        for (unsigned i = 0; i < MAC_FRS_RESULT_CHANNELS; i++)
            rssi_out[i] = FRS_SAMPLE_UNSCANNED;
    }
    if (gain_out != NULL)
        memset(gain_out, 0, MAC_FRS_GAIN_CHANNELS);
    do {
        settle = frs.single_channel ? FRS_ENTER_SETTLE_SINGLE : FRS_ENTER_SETTLE_MULTI;
    } while (systick_elapsed(systick_now(), start) < settle);

    /* Gain: automatic, or forced from the gain-index tables. */
    if (gain_index == FRS_GAIN_AUTO) {
        saved[FRS_SAVED_AGC_CFG] = AGC_CFG;
        AGC_CFG = FIELD_REPLACE(saved[FRS_SAVED_AGC_CFG], AGC_CFG_GAIN_MODE, AGC_GAIN_MODE_AUTO);
        saved[FRS_SAVED_AGC_TARGET] = RX_AGC_TARGET;
        RX_AGC_TARGET = saved[FRS_SAVED_AGC_TARGET] & ~RX_AGC_TARGET_FRS;
        frs.fixed_gain_word = 0;
        saved[FRS_SAVED_GAIN_FORCE] = RX_GAIN_FORCE;
    } else {
        gain_index = MIN(gain_index, FRS_GAIN_INDEX_MAX);
        frs.fixed_gain_index = frs_fixed_gain_index[gain_index];
        frs.fixed_gain_word = frs_fixed_gain_code[gain_index];
        saved[FRS_SAVED_AGC_TARGET] = RX_AGC_TARGET;
        RX_AGC_TARGET = saved[FRS_SAVED_AGC_TARGET] & ~RX_AGC_TARGET_FRS;
        saved[FRS_SAVED_GAIN_FORCE] = RX_GAIN_FORCE;
        RX_GAIN_FORCE = FIELD_PREP(RX_GAIN_FORCE_WORD, frs.fixed_gain_word)
                      | FIELD_PREP(RX_GAIN_FORCE_INDEX, frs.fixed_gain_index);
        saved[FRS_SAVED_AGC_CFG] = AGC_CFG;
        AGC_CFG = FIELD_REPLACE(saved[FRS_SAVED_AGC_CFG], AGC_CFG_GAIN_MODE, AGC_GAIN_MODE_FORCED);
    }

    /* Note: this re-applies the saved AGC_CFG (minus the freeze bit) and so
     * undoes the gain-mode selection just made; the scan pass sets the mode
     * again before each channel. */
    AGC_CFG = saved[FRS_SAVED_AGC_CFG] & ~AGC_CFG_FREEZE;
    return saved[FRS_SAVED_AGC_CFG] & ~AGC_CFG_FREEZE;
}

/* @provenance RVA 0x1A1C frs_restore_state_and_exit */
uint32_t frs_exit(void)
{
    const uint32_t *saved = frs.saved_regs;

    RX_RSSI_CAL1 = saved[FRS_SAVED_RSSI_CAL1];
    AGC_CFG = saved[FRS_SAVED_AGC_CFG];
    RX_GAIN_FORCE = saved[FRS_SAVED_GAIN_FORCE];
    RX_IQMM = saved[FRS_SAVED_IQMM];
    RX_AGC_TARGET = saved[FRS_SAVED_AGC_TARGET];
    SEQ_CTRL = saved[FRS_SAVED_SEQ_CTRL];
    SEQ_RF_CTRL = saved[FRS_SAVED_SEQ_RF_CTRL];
    SEQ_CFG = saved[FRS_SAVED_SEQ_CFG];
    SEQ_CHAN = saved[FRS_SAVED_SEQ_CHAN];
    RX_FRS_TIMING0 = saved[FRS_SAVED_TIMING0];
    RX_FRS_TIMING1 = saved[FRS_SAVED_TIMING1];
    RX_FRS_THRESHOLD0 = saved[FRS_SAVED_THRESHOLD0];
    RX_FRS_THRESHOLD1 = saved[FRS_SAVED_THRESHOLD1];
    RX_FRS_MASK = saved[FRS_SAVED_FRS_MASK];
    RX_FRS_WINDOW = saved[FRS_SAVED_WINDOW];
    if (frs.single_channel) {
        RX_FRS_CFG0 = saved[FRS_SAVED_FRS_CFG0];
        RX_FRS_CFG1 = saved[FRS_SAVED_FRS_CFG1];
    }
    RX_DEMOD_CTRL = saved[FRS_SAVED_DEMOD_CTRL];
    RADIO_FRS_GAIN1 = saved[FRS_SAVED_RADIO_GAIN1];
    RADIO_FRS_GAIN0 = saved[FRS_SAVED_RADIO_GAIN0];
    RADIO_RXFE_CTRL(RADIO_BANK_B) = saved[FRS_SAVED_RXFE_B];
    RADIO_RXFE_CTRL(RADIO_BANK_A) = saved[FRS_SAVED_RXFE_A];

    memset(frs.rssi_rows, 0, sizeof frs.rssi_rows);
    memset(frs.gain_rows, 0, sizeof frs.gain_rows);
    frs.single_channel = 0;

    REG_SET_BITS(SEQ_IRQ, SEQ_IRQ_RX_SETUP_DONE);
    nvic_clear_pending_irq(IRQ_RX_SETUP);
    return nvic_clear_pending_irq(IRQ_RX_DONE);
}

/* @provenance RVA 0x1B7C frs_correlator_hw_init */
void frs_correlator_init(void)
{
    uint32_t ctrl = FRS_CORR_CTRL;

    FRS_CORR_CTRL = ctrl | FRS_CORR_CTRL_EN;
    FRS_CORR_CTRL = FIELD_REPLACE(ctrl, FRS_CORR_CTRL_LEN, 0x20) | FRS_CORR_CTRL_EN;
}

/* Format a quarter-dB reading as "%d.%02d" into the next shared slot. */
/* @provenance RVA 0x1B98 frs_format_rssi_fixed_point */
char *frs_format_rssi(int value, float scale)
{
    float x = (float)value * scale;
    int whole = (int)x;
    uint32_t next;
    uint32_t slot = fmt_take_slot(&next);
    int fraction = (int)((x - (float)whole) * 100.0f);

    if (fraction < 0)
        fraction = -fraction;
    return fmt_fixed_point((int)slot, fraction, (int)next, whole);
}
