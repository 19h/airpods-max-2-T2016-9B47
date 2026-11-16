/*
 * txpwr.c -- transmit power control.
 *
 * Every packet carries a power step in SEQ_PKT_CMD1.  The MAC's per-band
 * table turns that into a target in half-dB units; the target is clamped by
 * the per-channel edge limits and the coexistence floor, then matched against
 * the interpolated maximum-power calibration (NVV_TAG_TX_MAXPWR_CAL_V2 or
 * SysCfg TLV 1) to choose a PA stage and the gain/sequencer code pair within
 * that stage.  Several test modes bypass the lookup and drive the PA directly.
 */
#include "btp/btp.h"
#include "btp/txpwr.h"
#include "btp/tx.h"
#include "btp/rx.h"
#include "btp/nvv.h"
#include "btp/clock.h"
#include "btp/trace.h"
#include "btp/rtkit.h"

struct txpwr_mode  txpwr_mode BTP_STATE(txpwr_mode);
struct txpwr_state txpwr      BTP_STATE(txpwr);

/* NVV register-write records can replace this original initialized RAM cell. */
volatile uint32_t txpwr_workspace_address BTP_DATA(txpwr_workspace_cell) = 0x4D28B900;

static uint8_t *txpwr_workspace_base(void)
{
#ifdef BTP_HOST
    /* Native module state is separate from the sparse physical bus adapter. */
    return &txpwr.maxpwr[0][0][0];
#else
    return BTP_PTR(txpwr_workspace_address);
#endif
}

static const uint8_t *txpwr_calibration_base(void)
{
    return *(const uint8_t * volatile *)&nvv.tx_maxpwr_cal;
}

uint32_t txpwr_iq_scale BTP_DATA(txpwr_iq_scale) = TXPWR_IQ_SCALE_DEFAULT;
int32_t  txpwr_coex_floor BTP_DATA(txpwr_coex_floor) = TXPWR_COEX_FLOOR_DEFAULT;
uint8_t  txpwr_debug_flags BTP_DATA(txpwr_debug_flags) = 3;

/* PA gain nibble (RADIO_PA_CTRL_GAIN) and sequencer power code per PA stage. */
static uint8_t txpwr_pa_gain_code[4] BTP_DATA(txpwr_pa_gain_code) = { 0x00, 0x0A, 0x07, 0x0C };
static uint8_t txpwr_pa_seq_code[4] BTP_DATA(txpwr_pa_seq_code) = { 0x00, 0x40, 0x80, 0x7C };

/* Power-table column for a packet, by alternate-format rate / by packet type. */
static const uint8_t txpwr_alt_fmt_to_pwr_type[8] = { 3, 3, 4, 3, 3, 3, 3, 3 };
static const uint8_t txpwr_pkt_type_to_pwr_type[BTP_PKT_TYPE_COUNT] = {
    2, 2, 2, 2, 2, 2, 0, 1, 5, 6, 2, 2, 7, 8, 2, 2,
};

/* Default per-stage code tables, indexed by back-off from the stage maximum. */
static struct txpwr_pa_code txpwr_pa_codes[TXPWR_PA_STAGES][TXPWR_PA_CODES] BTP_DATA(txpwr_pa_codes) = {
    {   /* stage 1 */
        {0x0F,0x40}, {0x0F,0x3E}, {0x0F,0x3C}, {0x0E,0x3F}, {0x0E,0x3D}, {0x0D,0x3F}, {0x0D,0x3E}, {0x0D,0x3C},
        {0x0C,0x3F}, {0x0C,0x3D}, {0x0B,0x40}, {0x0B,0x3E}, {0x0B,0x3C}, {0x0B,0x3B}, {0x0A,0x3E}, {0x0A,0x3C},
        {0x0A,0x3B}, {0x09,0x3F}, {0x09,0x3D}, {0x09,0x3B}, {0x08,0x40}, {0x08,0x3E}, {0x08,0x3C}, {0x08,0x3B},
        {0x08,0x39}, {0x07,0x3E}, {0x07,0x3D}, {0x07,0x3B}, {0x07,0x39}, {0x06,0x3F}, {0x06,0x3E}, {0x06,0x3C},
        {0x06,0x3A}, {0x06,0x39}, {0x06,0x37}, {0x05,0x3E}, {0x05,0x3D}, {0x05,0x3B}, {0x05,0x39}, {0x05,0x38},
        {0x05,0x36}, {0x04,0x3F}, {0x04,0x3D}, {0x04,0x3B}, {0x04,0x3A}, {0x04,0x38}, {0x04,0x36}, {0x04,0x35},
        {0x04,0x33}, {0x03,0x3E}, {0x03,0x3D}, {0x03,0x3B}, {0x03,0x39}, {0x03,0x38}, {0x03,0x36}, {0x03,0x35},
        {0x03,0x33}, {0x03,0x32}, {0x03,0x30}, {0x02,0x3E}, {0x02,0x3D}, {0x02,0x3B}, {0x02,0x39}, {0x02,0x38},
        {0x02,0x36}, {0x02,0x35}, {0x02,0x33}, {0x02,0x32}, {0x02,0x30}, {0x02,0x2F}, {0x02,0x2E}, {0x02,0x2C},
        {0x02,0x2B}, {0x01,0x3F}, {0x01,0x3D}, {0x01,0x3B}, {0x01,0x39}, {0x01,0x38}, {0x01,0x36}, {0x01,0x35},
        {0x01,0x33}, {0x01,0x32}, {0x01,0x30}, {0x01,0x2F}, {0x01,0x2E}, {0x01,0x2C}, {0x01,0x2B}, {0x01,0x2A},
        {0x01,0x29}, {0x01,0x28}, {0x01,0x26}, {0x01,0x25}, {0x01,0x24}, {0x01,0x23}, {0x01,0x22}, {0x01,0x21},
        {0x01,0x20}, {0x00,0x3F}, {0x00,0x3D}, {0x00,0x3B}, {0x00,0x3A}, {0x00,0x38}, {0x00,0x36}, {0x00,0x35},
        {0x00,0x33}, {0x00,0x32}, {0x00,0x30}, {0x00,0x2F}, {0x00,0x2E}, {0x00,0x2C}, {0x00,0x2B}, {0x00,0x2A},
        {0x00,0x29}, {0x00,0x28}, {0x00,0x26}, {0x00,0x25}, {0x00,0x24}, {0x00,0x23}, {0x00,0x22}, {0x00,0x21},
        {0x00,0x20},
    },
    {   /* stage 2 */
        {0x0F,0x80}, {0x0F,0x7C}, {0x0F,0x79}, {0x0E,0x7D}, {0x0E,0x7A}, {0x0D,0x7F}, {0x0D,0x7B}, {0x0D,0x78},
        {0x0C,0x7D}, {0x0C,0x7A}, {0x0B,0x80}, {0x0B,0x7C}, {0x0B,0x79}, {0x0B,0x75}, {0x0A,0x7C}, {0x0A,0x79},
        {0x0A,0x75}, {0x09,0x7E}, {0x09,0x7A}, {0x09,0x77}, {0x08,0x80}, {0x08,0x7C}, {0x08,0x79}, {0x08,0x75},
        {0x08,0x72}, {0x07,0x7D}, {0x07,0x79}, {0x07,0x76}, {0x07,0x72}, {0x06,0x7F}, {0x06,0x7B}, {0x06,0x78},
        {0x06,0x74}, {0x06,0x71}, {0x06,0x6E}, {0x05,0x7D}, {0x05,0x79}, {0x05,0x76}, {0x05,0x72}, {0x05,0x6F},
        {0x05,0x6C}, {0x04,0x7E}, {0x04,0x7A}, {0x04,0x77}, {0x04,0x73}, {0x04,0x70}, {0x04,0x6D}, {0x04,0x6A},
        {0x04,0x67}, {0x03,0x7D}, {0x03,0x79}, {0x03,0x76}, {0x03,0x73}, {0x03,0x6F}, {0x03,0x6C}, {0x03,0x69},
        {0x03,0x66}, {0x03,0x63}, {0x03,0x60}, {0x02,0x7D}, {0x02,0x79}, {0x02,0x76}, {0x02,0x73}, {0x02,0x6F},
        {0x02,0x6C}, {0x02,0x69}, {0x02,0x66}, {0x02,0x63}, {0x02,0x60}, {0x02,0x5E}, {0x02,0x5B}, {0x02,0x58},
        {0x02,0x56}, {0x01,0x7D}, {0x01,0x7A}, {0x01,0x76}, {0x01,0x73}, {0x01,0x70}, {0x01,0x6C}, {0x01,0x69},
        {0x01,0x66}, {0x01,0x63}, {0x01,0x61}, {0x01,0x5E}, {0x01,0x5B}, {0x01,0x59}, {0x01,0x56}, {0x01,0x54},
        {0x01,0x51}, {0x01,0x4F}, {0x01,0x4D}, {0x01,0x4B}, {0x01,0x48}, {0x01,0x46}, {0x01,0x44}, {0x01,0x42},
        {0x01,0x41}, {0x00,0x7E}, {0x00,0x7A}, {0x00,0x77}, {0x00,0x73}, {0x00,0x70}, {0x00,0x6D}, {0x00,0x6A},
        {0x00,0x67}, {0x00,0x64}, {0x00,0x61}, {0x00,0x5E}, {0x00,0x5B}, {0x00,0x59}, {0x00,0x56}, {0x00,0x54},
        {0x00,0x52}, {0x00,0x4F}, {0x00,0x4D}, {0x00,0x4B}, {0x00,0x49}, {0x00,0x47}, {0x00,0x45}, {0x00,0x43},
        {0x00,0x41},
    },
    {   /* stage 3 */
        {0x0F,0x7C}, {0x0F,0x78}, {0x0F,0x75}, {0x0F,0x72}, {0x0F,0x6F}, {0x0F,0x6B}, {0x0F,0x68}, {0x0F,0x65},
        {0x0F,0x62}, {0x0F,0x60}, {0x0F,0x5D}, {0x0E,0x60}, {0x0E,0x5E}, {0x0E,0x5B}, {0x0D,0x5F}, {0x0D,0x5C},
        {0x0C,0x60}, {0x0C,0x5E}, {0x0C,0x5B}, {0x0B,0x60}, {0x0B,0x5D}, {0x0B,0x5A}, {0x0A,0x60}, {0x0A,0x5D},
        {0x0A,0x5A}, {0x0A,0x58}, {0x09,0x5E}, {0x09,0x5B}, {0x09,0x59}, {0x08,0x60}, {0x08,0x5D}, {0x08,0x5A},
        {0x08,0x58}, {0x07,0x60}, {0x07,0x5D}, {0x07,0x5B}, {0x07,0x58}, {0x07,0x55}, {0x06,0x5F}, {0x06,0x5C},
        {0x06,0x5A}, {0x06,0x57}, {0x06,0x55}, {0x05,0x60}, {0x05,0x5D}, {0x05,0x5B}, {0x05,0x58}, {0x05,0x55},
        {0x05,0x53}, {0x05,0x51}, {0x04,0x5E}, {0x04,0x5B}, {0x04,0x59}, {0x04,0x56}, {0x04,0x54}, {0x04,0x51},
        {0x04,0x4F}, {0x03,0x60}, {0x03,0x5D}, {0x03,0x5B}, {0x03,0x58}, {0x03,0x56}, {0x03,0x53}, {0x03,0x51},
        {0x03,0x4F}, {0x03,0x4C}, {0x03,0x4A}, {0x02,0x60}, {0x02,0x5D}, {0x02,0x5B}, {0x02,0x58}, {0x02,0x56},
        {0x02,0x53}, {0x02,0x51}, {0x02,0x4F}, {0x02,0x4C}, {0x02,0x4A}, {0x02,0x48}, {0x02,0x46}, {0x02,0x44},
        {0x02,0x42}, {0x01,0x60}, {0x01,0x5E}, {0x01,0x5B}, {0x01,0x58}, {0x01,0x56}, {0x01,0x53}, {0x01,0x51},
        {0x01,0x4F}, {0x01,0x4D}, {0x01,0x4A}, {0x01,0x48}, {0x01,0x46}, {0x01,0x44}, {0x01,0x42}, {0x01,0x40},
        {0x01,0x3F}, {0x01,0x3D}, {0x01,0x3B}, {0x01,0x39}, {0x01,0x38}, {0x01,0x36}, {0x01,0x35}, {0x01,0x33},
        {0x01,0x32}, {0x01,0x30}, {0x00,0x5E}, {0x00,0x5B}, {0x00,0x59}, {0x00,0x56}, {0x00,0x54}, {0x00,0x51},
        {0x00,0x4F}, {0x00,0x4D}, {0x00,0x4B}, {0x00,0x48}, {0x00,0x46}, {0x00,0x44}, {0x00,0x42}, {0x00,0x41},
        {0x00,0x3F},
    },
};

/* Half-dB offsets applied to the target for the lower packet types. */
#define TXPWR_TYPE_OFFSET_MASK_12   0x3u    /* types 0, 1 */
#define TXPWR_TYPE_OFFSET_MASK_11   0x60u   /* types 5, 6 */
#define TXPWR_TYPE_OFFSET_MASK_13   0x180u  /* types 7, 8 */

/* ------------------------------------------------------------------------- */
/* Host configuration                                                        */
/* ------------------------------------------------------------------------- */

/* @provenance RVA 0x3874 tx_power_set_pa_gain_override */
btp_status_t txpwr_set_pa_gain_override(uint32_t enable, uint8_t gain)
{
    txpwr_mode.pa_gain_override = enable ? gain : 0;
    return BTP_OK;
}

/* @provenance RVA 0x3888 tx_power_reset_band_workspaces */
int txpwr_reset_band_overrides(void)
{
    for (unsigned band = 0; band < BTP_BAND_COUNT; ++band) {
        txpwr.band_override_value[band] = TXPWR_BAND_OVERRIDE_OFF;
        txpwr.band_override_enable[band] = 0;
    }
    return 0;
}

/* @provenance RVA 0x38B0 tx_power_reset_channel_limits */
int txpwr_reset_channel_limits(void)
{
    for (unsigned band = 0; band < BTP_BAND_COUNT; ++band) {
        for (unsigned type = 0; type < TXPWR_PKT_TYPES; ++type) {
            struct txpwr_channel_limit *lim = &txpwr.limits[band][type];

            lim->low_channel = 0xFF;
            lim->high_channel = 0xFF;
            lim->low_power = -1;
            lim->high_power = -1;
        }
    }
    return 0;
}

/* ROM 0x38DC reads the complete four-byte {low channel, low power,
 * high channel, high power} entry. The return value is packed, not a power. */
/* @provenance RVA 0x38DC tx_power_get_channel_limit */
uint32_t txpwr_get_channel_limit_word(unsigned band, unsigned pkt_type)
{
    uint32_t packed_limit;

    memcpy(&packed_limit, &txpwr.limits[band][pkt_type], sizeof packed_limit);
    return packed_limit;
}

/* @provenance RVA 0x38F0 tx_power_set_channel_limit */
btp_status_t txpwr_set_channel_limit(unsigned op, unsigned band, unsigned pkt_type,
                                     uint8_t channel, int8_t power)
{
    struct txpwr_channel_limit *lim;

    if (band > BTP_BAND_COUNT - 1 || pkt_type > TXPWR_PKT_TYPES - 1)
        return BTP_ERR_INVALID;

    lim = &txpwr.limits[band][pkt_type];
    irq_save();
    switch (op) {
    case TXPWR_LIMIT_LOW:
        lim->low_channel = channel;
        lim->low_power = power;
        break;
    case TXPWR_LIMIT_HIGH:
        lim->high_channel = channel;
        lim->high_power = power;
        break;
    default:
        lim->low_channel = 0xFF;
        lim->high_channel = 0xFF;
        lim->low_power = -1;
        lim->high_power = -1;
        break;
    }
    irq_enable();
    return BTP_OK;
}

/* @provenance RVA 0x3958 update_tx_power_tables */
btp_status_t txpwr_update_table(const void *table)
{
    if (txpwr_debug_flags & TXPWR_DEBUG_VERBOSE_TABLES)
        dbg_printf("\n update_tx_power_tables @ %08x size %d\n", txpwr.table, (int)sizeof txpwr.table);
    memcpy(txpwr.table, table, sizeof txpwr.table);
    tx_ovr.tables_valid = 1;
    return BTP_OK;
}

/* @provenance RVA 0x39E8 update_tx_power_tables_per_band */
btp_status_t txpwr_update_band_table(const uint32_t *table, unsigned band)
{
    if (txpwr_debug_flags & TXPWR_DEBUG_VERBOSE_TABLES) {
        dbg_printf("\n update_tx_power_tables per band %d @ %08x size %d\n",
                   band, txpwr.table, (int)sizeof txpwr.table);
        for (unsigned type = 0; type < TXPWR_PKT_TYPES; ++type)
            dbg_printf("band: %d packet type: %d:%08x:%08x\n",
                       band, type, &table[type], table[type]);
    }
    memcpy(txpwr.table[band], table, sizeof txpwr.table[band]);
    return BTP_OK;
}

/* @provenance RVA 0x3998 tx_power_set_global_offset */
btp_status_t txpwr_set_global_offset(uint32_t enable, int32_t offset)
{
    txpwr.global_offset = enable ? offset : 0;
    return BTP_OK;
}

/* @provenance RVA 0x39AC tx_power_set_band_workspace */
btp_status_t txpwr_set_band_override(unsigned band, uint32_t enable, uint32_t value)
{
    txpwr.band_override_enable[band] = enable;
    txpwr.band_override_value[band] = enable ? value : TXPWR_BAND_OVERRIDE_OFF;
    return BTP_OK;
}

/* @provenance RVA 0x39CC tx_power_set_iq_scale_override */
btp_status_t txpwr_set_iq_scale_override(uint32_t enable, uint32_t scale)
{
    txpwr_mode.iq_scale_override = enable;
    txpwr_iq_scale = enable ? scale : TXPWR_IQ_SCALE_DEFAULT;
    return BTP_OK;
}

/* @provenance RVA 0x3A5C tx_power_set_coex_floor */
int txpwr_set_coex_floor(int32_t floor)
{
    txpwr_coex_floor = floor;
    return floor;
}

/* @provenance RVA 0x4004 tx_power_configure_test_mode */
btp_status_t txpwr_set_test_mode(uint32_t mode, uint32_t stage, uint32_t post_stage,
                                 uint8_t delay, uint32_t seq_code, uint32_t gain_code,
                                 int override_value)
{
    irq_save();
    txpwr_mode.mode = mode;
    txpwr_mode.stage = stage;
    txpwr_mode.post_stage = post_stage;
    txpwr_mode.seq_code = seq_code;
    txpwr_mode.gain_code = gain_code;
    txpwr_mode.delay_ticks = delay;
    txpwr_mode.override_value = (uint32_t)override_value;
    irq_enable();
    return BTP_OK;
}

/* ------------------------------------------------------------------------- */
/* Calibration table                                                         */
/* ------------------------------------------------------------------------- */

/*
 * Expand the maximum-power calibration into txpwr.maxpwr.
 *
 * The calibration starts with one entry count per band, followed by the
 * entries themselves: 32-bit words whose top byte is a channel index and whose
 * low three bytes are the maximum power of the three PA stages at that
 * channel.  Channels between two entries are linearly interpolated, channels
 * above the last entry repeat it.
 */
static void txpwr_expand_maxpwr(const uint8_t *cal)
{
    unsigned entry = 2;                     /* first entry word (8 count bytes) */

    dbg_printf("phymaxpwr_p=%08x CAL %08x\n", txpwr_workspace_base(), cal);
    memset(txpwr_workspace_base(), 0, sizeof txpwr.maxpwr);

    for (unsigned band = 0; band < BTP_BAND_COUNT; ++band) {
        unsigned row = band * TXPWR_CHANNELS * 4;
        unsigned count = txpwr_calibration_base()[band];
        unsigned prev_channel = 0;

        for (unsigned k = 0; k < count; ++k, ++entry) {
            uint32_t word = *(const uint32_t *)&txpwr_calibration_base()[4 * entry];
            unsigned channel = word >> 24;

            for (unsigned stage = 0; stage < TXPWR_PA_STAGES; ++stage) {
                uint8_t *base = txpwr_workspace_base();
                uint32_t value = *(const uint32_t *)&txpwr_calibration_base()[4 * entry];

                base[row + 4 * channel + stage] = (uint8_t)(value >> (8 * stage));
            }

            if (channel != 0) {
                unsigned span = channel - prev_channel;

                for (unsigned c = prev_channel + 1; c < channel; ++c) {
                    for (unsigned stage = 0; stage < TXPWR_PA_STAGES; ++stage) {
                        uint8_t *base = txpwr_workspace_base();
                        int low = base[row + 4 * prev_channel + stage];
                        int high = base[row + 4 * channel + stage];

                        base[row + 4 * c + stage] =
                            (uint8_t)((high - low) * (int)(c - prev_channel) / (int)span + low);
                    }
                }
                prev_channel = channel;
            }

            if (k == count - 1) {
                for (unsigned c = channel + 1; c < TXPWR_CHANNELS; ++c) {
                    for (unsigned stage = 0; stage < TXPWR_PA_STAGES; ++stage) {
                        uint8_t *base = txpwr_workspace_base();

                        base[row + 4 * c + stage] = base[row + 4 * channel + stage];
                    }
                }
            }
        }
    }
}

/* @provenance RVA 0x3A68 tx_power_init_and_interpolate_tables */
uint32_t txpwr_init(void)
{
    txpwr_coex_floor = TXPWR_COEX_FLOOR_DEFAULT;
    txpwr.pa_tables[2] = txpwr_pa_codes[2];
    txpwr.pa_tables[1] = txpwr_pa_codes[1];
    txpwr.pa_tables[0] = txpwr_pa_codes[0];
    MMIO32(TXPWR_TABLE_MARKER) = TXPWR_TABLE_MARKER_VALUE;
    txpwr_reset_channel_limits();
    txpwr_reset_band_overrides();
    txpwr.global_offset = 0;
    txpwr.cal_offset = TXPWR_CAL_OFFSET_DEFAULT;

    if (nvv.tx_maxpwr_cal != NULL) {
        txpwr_expand_maxpwr(nvv.tx_maxpwr_cal);
        return 0;
    }
    dbg_printf("No Tx PWR TBL found!!\n");
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Per-packet power setting                                                  */
/* ------------------------------------------------------------------------- */

static inline volatile uint32_t *txpwr_pa_ctrl_for_band(unsigned band)
{
    return band == 0 ? &RADIO_PA_CTRL(RADIO_BANK_B) : &RADIO_PA_CTRL(RADIO_BANK_A);
}

/* Latch a {gain, code} pair and the stage into the sequencer. */
static void txpwr_program_code(const struct txpwr_pa_code *code, unsigned stage)
{
    REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_PWR_CODE, code->code);
    REG_WRITE_FIELD(SEQ_RF_CTRL, SEQ_RF_CTRL_PA_GAIN, code->gain);
    REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_PWR_LOAD);
    REG_WRITE_FIELD(SEQ_RF_CTRL, SEQ_RF_CTRL_PA_STAGE, stage);
}

/* @provenance RVA 0x3C20 tx_power_apply_calibrated_target */
void txpwr_apply_calibrated(int target, unsigned band, int channel, unsigned pkt_type)
{
    const struct txpwr_channel_limit *lim = &txpwr.limits[band][pkt_type];
    const uint8_t *maxpwr;
    bool coex_active = coex_is_active();
    int floored, limited, adjusted, doubled;
    unsigned stage, back_off;
    uint32_t stage_max;

    if (nvv.tx_maxpwr_cal == NULL) {
        txpwr_apply_stage(3, 0, band);
        return;
    }

    /* Coexistence floor, then the per-channel edge limits. */
    floored = target;
    if (coex_active && target <= txpwr_coex_floor)
        floored = txpwr_coex_floor;
    limited = (int)txpwr.cal_offset + floored;
    if (lim->low_channel != 0xFF && channel >= lim->low_channel && limited > lim->low_power)
        limited = lim->low_power;
    if (lim->high_channel != 0xFF && channel <= lim->high_channel && limited > lim->high_power)
        limited = lim->high_power;

    if (pkt_type <= 8) {
        if ((1u << pkt_type) & TXPWR_TYPE_OFFSET_MASK_12)
            limited += 12;
        else if ((1u << pkt_type) & TXPWR_TYPE_OFFSET_MASK_11)
            limited += 11;
        else if ((1u << pkt_type) & TXPWR_TYPE_OFFSET_MASK_13)
            limited += 13;
    }

    /* Pick the lowest PA stage whose maximum covers the target. */
    maxpwr = txpwr_workspace_base() + (band * TXPWR_CHANNELS + channel) * 4;
    adjusted = txpwr.global_offset + limited;
    doubled = 2 * adjusted;
    if ((int)maxpwr[0] <= doubled) {
        stage = doubled < (int)maxpwr[1] ? 2 : 3;
    } else {
        stage = 1;
        if (band == 0 && doubled + 10 > (int)maxpwr[0])
            stage = 2;
    }

    /* Back-off from the stage maximum selects the code pair. */
    stage_max = maxpwr[stage - 1];
    if ((int)stage_max <= doubled)
        back_off = 0;
    else if (adjusted <= (int)((stage_max >> 1) - TXPWR_CODE_INDEX_MAX))
        back_off = TXPWR_CODE_INDEX_MAX;
    else
        back_off = (unsigned)((int)(stage_max - (uint32_t)doubled) >> 1);

    txpwr_program_code(&txpwr.pa_tables[stage - 1][back_off], stage);
    REG_WRITE_FIELD(*txpwr_pa_ctrl_for_band(band), RADIO_PA_CTRL_GAIN, txpwr_pa_gain_code[stage]);
}

/* @provenance RVA 0x3DD0 tx_power_apply_direct_index */
uint32_t txpwr_apply_stage(unsigned stage, unsigned code_index, unsigned band)
{
    volatile uint32_t *pa_ctrl = txpwr_pa_ctrl_for_band(band);
    uint32_t result;

    if (code_index >= TXPWR_CODE_INDEX_MAX)
        code_index = TXPWR_CODE_INDEX_MAX;
    txpwr_program_code(&txpwr.pa_tables[stage - 1][code_index], stage & 3u);
    result = FIELD_REPLACE(*pa_ctrl, RADIO_PA_CTRL_GAIN, txpwr_pa_gain_code[stage]);
    *pa_ctrl = result;
    return result;
}

/* @provenance RVA 0x3E50 tx_power_apply_for_packet */
uint32_t txpwr_apply_for_packet(unsigned band, int channel, int target, unsigned pkt_type)
{
    volatile uint32_t *pa_ctrl = txpwr_pa_ctrl_for_band(band);
    uint32_t result;

    switch (txpwr_mode.mode) {
    case TXPWR_MODE_CALIBRATED:
        txpwr_apply_calibrated(target, band, channel, pkt_type);
        break;

    case TXPWR_MODE_TIMED_STAGE: {
        uint64_t trigger;

        /* Drop the stage, then switch to the requested one a fixed delay
         * after the TX trigger. */
        REG_WRITE_FIELD(SEQ_RF_CTRL, SEQ_RF_CTRL_PA_STAGE, 0);
        seq_timer_delay(5);
        trigger = seq_timer_tx_trigger();
        seq_timer_wait_until(trigger + txpwr_mode.delay_ticks);
        REG_WRITE_FIELD(SEQ_RF_CTRL, SEQ_RF_CTRL_PA_STAGE, txpwr_mode.stage);
        REG_WRITE_FIELD(*pa_ctrl, RADIO_PA_CTRL_GAIN,
                        txpwr_pa_gain_code[FIELD_GET(SEQ_RF_CTRL_PA_STAGE, SEQ_RF_CTRL)]);
        REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_PWR_CODE, txpwr_pa_seq_code[txpwr_mode.stage]);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_PWR_LOAD);
        break;
    }

    case TXPWR_MODE_FIXED_STAGE:
        REG_WRITE_FIELD(SEQ_RF_CTRL, SEQ_RF_CTRL_PA_STAGE, txpwr_mode.stage);
        REG_WRITE_FIELD(*pa_ctrl, RADIO_PA_CTRL_GAIN,
                        txpwr_pa_gain_code[FIELD_GET(SEQ_RF_CTRL_PA_STAGE, SEQ_RF_CTRL)]);
        REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_PWR_CODE, txpwr_pa_seq_code[txpwr_mode.stage]);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_PWR_LOAD);
        break;

    case TXPWR_MODE_FIXED_CODES:
        REG_WRITE_FIELD(SEQ_RF_CTRL, SEQ_RF_CTRL_PA_STAGE, txpwr_mode.stage);
        REG_WRITE_FIELD(*pa_ctrl, RADIO_PA_CTRL_GAIN, txpwr_pa_gain_code[txpwr_mode.stage]);
        REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_PWR_CODE, txpwr_pa_seq_code[txpwr_mode.stage]);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_PWR_LOAD);
        REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_PWR_CODE, txpwr_mode.seq_code);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_PWR_LOAD);
        REG_WRITE_FIELD(SEQ_RF_CTRL, SEQ_RF_CTRL_PA_GAIN, txpwr_mode.gain_code);
        break;

    case TXPWR_MODE_TARGET_OVERRIDE:
    case TXPWR_MODE_INDEX_OVERRIDE:
        /* Known C-model fidelity gap, not native mode semantics: ROM
         * 0x3E76 calls the calibrated path with override_value; ROM 0x3F64
         * calls the direct path with stage and override_value. The existing
         * halt behavior is retained here so this naming pass changes no code. */
        rtk_platform_halt();
        break;

    default:
        break;
    }

    /* A host-forced PA gain wins over everything on both banks. */
    result = txpwr_mode.pa_gain_override;
    if (txpwr_mode.pa_gain_override) {
        REG_WRITE_FIELD(RADIO_PA_CTRL(RADIO_BANK_B), RADIO_PA_CTRL_GAIN, txpwr_mode.pa_gain_override);
        REG_WRITE_FIELD(RADIO_PA_CTRL(RADIO_BANK_A), RADIO_PA_CTRL_GAIN, txpwr_mode.pa_gain_override);
        result = RADIO_PA_CTRL(RADIO_BANK_A);
    }
    return result;
}

/* @provenance RVA 0x3FCC tx_power_complete_packet */
int txpwr_packet_done(void)
{
    REG_WRITE_FIELD(SEQ_RF_CTRL, SEQ_RF_CTRL_PA_STAGE, 0);
    if (txpwr_mode.mode == TXPWR_MODE_TIMED_STAGE) {
        seq_timer_delay(5);
        REG_WRITE_FIELD(SEQ_RF_CTRL, SEQ_RF_CTRL_PA_STAGE, txpwr_mode.post_stage);
        return (int)txpwr_mode.post_stage;
    }
    return (int)txpwr_mode.mode;
}

/* @provenance RVA 0x4064 tx_power_setup_from_mac_command */
void txpwr_setup_from_command(void)
{
    uint32_t cmd0, cmd1;
    unsigned band, pwr_type;
    int target;

    REG_CLEAR_BITS(SEQ_CTRL, SEQ_CTRL_PWR_LOAD);
    cmd0 = SEQ_PKT_CMD0;
    cmd1 = SEQ_PKT_CMD1;

    if (cmd0 & SEQ_PKT_CMD0_ALT_FORMAT)
        pwr_type = txpwr_alt_fmt_to_pwr_type[FIELD_GET(SEQ_PKT_CMD0_ALT_RATE, cmd0)];
    else
        pwr_type = txpwr_pkt_type_to_pwr_type[FIELD_GET(SEQ_PKT_CMD1_PKT_TYPE, cmd1)];
    band = FIELD_GET(SEQ_PKT_CMD0_BAND, cmd0);

    if (tx_ovr.tables_valid) {
        const struct txpwr_entry *e = &txpwr.table[band][pwr_type];
        int step = (int)FIELD_GET(SEQ_PKT_CMD1_PWR_STEP, cmd1);

        if (cmd1 & SEQ_PKT_CMD1_PWR_ALT)
            target = e->alt_power - step * e->alt_step;
        else
            target = e->power - step * e->step;
    } else {
        target = TXPWR_DEFAULT_TARGET;
    }

    txpwr_apply_for_packet(band, (int)FIELD_GET(SEQ_PKT_CMD0_CHANNEL, cmd0), target, pwr_type);
}
