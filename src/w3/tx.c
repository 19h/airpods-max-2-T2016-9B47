/*
 * tx.c -- transmit path.
 *
 * tx_setup_isr() runs when the sequencer has latched the next packet's
 * command words: it programs the PA bias, the A4P/KVCO pre-distortion, the
 * DF1 mantissa/exponent and the per-band delay and HF-coefficient sets from
 * the NVV calibration, then hands the power setting to txpwr.c and the DPD
 * experiments to dpd.c.  The RF test modes (CW tone, PRBS) drive the same
 * sequencer registers directly.
 */
#include "btp/btp.h"
#include "btp/tx.h"
#include "btp/txpwr.h"
#include "btp/dpd.h"
#include "btp/loopback.h"
#include "btp/nvv.h"
#include "btp/clock.h"
#include "btp/trace.h"
#include "btp/rtkit.h"

struct tx_overrides tx_ovr BTP_STATE(tx_ovr);
struct tx_state     tx     BTP_STATE(tx);

uint32_t tx_power_mode_override_value BTP_DATA(tx_power_mode_override_value) = 1;

/* SEQ_CFG tone code for each CW tone mode (modes 0..3; mode 1 is special). */
static uint8_t tx_tone_mode_code[4] BTP_DATA(tx_tone_mode_code) = { 0x0, 0x2, 0x9, 0xD };

/* Packet types that use the "alternate" dynamic calibration column. */
#define TX_ALT_TYPE_MASK_BAND0      0x3200u     /* types 9, 12, 13 */
#define TX_ALT_TYPE_MASK_OTHER      0x33C0u     /* types 6-9, 12, 13 */
#define TX_PKT_TYPE_MAX_DYNAMIC     13

/* A4P / KVCO calibration constants (bit patterns are those of the image). */
#define TX_A4P_SLOPE_SCALE          (-0.03125f)
#define TX_A4P_GAIN_BIAS            2048.0f
#define TX_A4P_RECIPROCAL           4194304.0f  /* 2^22 */
#define TX_KVCO_SCALE               0.0078125f  /* 1/128 */
#define TX_A4P_FIELD_MAX            0xFFFu

/* ------------------------------------------------------------------------- */
/* Host overrides                                                            */
/* ------------------------------------------------------------------------- */

/* @provenance RVA 0x2970 tx_set_slope_cal_bypass */
btp_status_t tx_set_slope_cal_bypass(uint32_t bypass)
{
    tx_ovr.slope_cal_bypass = bypass;
    return BTP_OK;
}

/* @provenance RVA 0x297C tx_set_df1_mantissa_override */
btp_status_t tx_set_df1_mantissa_override(uint32_t enable, int16_t mantissa)
{
    tx_ovr.df1_mantissa_override = enable;
    tx_ovr.df1_mantissa_value = (uint16_t)mantissa;
    return BTP_OK;
}

/* @provenance RVA 0x298C tx_set_hf_coeff_bypass */
btp_status_t tx_set_hf_coeff_bypass(uint8_t bypass)
{
    tx_ovr.hf_coeff_bypass = bypass;
    return BTP_OK;
}

/* @provenance RVA 0x2998 tx_override_noop_6 */
btp_status_t tx_set_reserved_override_6(void)
{
    return BTP_OK;
}

/* @provenance RVA 0x299C tx_override_noop_21 */
btp_status_t tx_set_reserved_override_21(void)
{
    return BTP_OK;
}

/* @provenance RVA 0x3658 tx_set_power_mode_override */
btp_status_t tx_set_power_mode_override(uint32_t mode)
{
    REG_SET_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_PWR_MODE_LATCH);
    REG_WRITE_MASKED(SEQ_RF_CTRL, SEQ_RF_CTRL_PWR_MODE,
                     (mode & 1u) ? SEQ_RF_CTRL_PWR_MODE : 0u);
    tx_power_mode_override_value = mode;
    tx_ovr.power_mode_override = 1;
    return BTP_OK;
}

/* ------------------------------------------------------------------------- */
/* RF test modes                                                             */
/* ------------------------------------------------------------------------- */

/* @provenance RVA 0x27AC rf_test_set_correlator_magic */
btp_status_t rf_test_set_correlator_magic(uint32_t enable)
{
    RX_CORR_MAGIC = enable ? RX_CORR_MAGIC_VALUE : 0u;
    return BTP_OK;
}

/* @provenance RVA 0x27C4 rf_test_configure_cw_tone */
btp_status_t rf_test_cw_tone(unsigned enable, unsigned band, uint8_t channel, unsigned mode)
{
    if ((int)band <= 7 && (int)mode <= 3)
        return rf_test_cw_tone_apply(enable, (uint8_t)band, channel, mode);
    return BTP_ERR_INVALID;
}

/* @provenance RVA 0x27D0 rf_test_apply_cw_tone */
btp_status_t rf_test_cw_tone_apply(unsigned enable, uint8_t band, uint8_t channel, unsigned mode)
{
    uint32_t ctrl;

    /* Stop whatever the sequencer is doing and return the AGC to automatic. */
    REG_CLEAR_BITS(SEQ_CTRL, SEQ_CTRL_TONE_EN);
    REG_CLEAR_BITS(SEQ_CFG, SEQ_CFG_TONE_RUN);
    ctrl = SEQ_CTRL;
    SEQ_CTRL = ctrl & ~SEQ_CTRL_CHAN_LOAD;
    SEQ_CTRL = ctrl & ~(SEQ_CTRL_CHAN_LOAD | SEQ_CTRL_RF_LOAD);
    SEQ_CTRL = ctrl & ~(SEQ_CTRL_CHAN_LOAD | SEQ_CTRL_RF_LOAD | SEQ_CTRL_FORCE_TX);
    REG_WRITE_FIELD(AGC_CFG, AGC_CFG_GAIN_MODE, AGC_GAIN_MODE_AUTO);
    SEQ_CTRL = ctrl & ~(SEQ_CTRL_CHAN_LOAD | SEQ_CTRL_RF_LOAD | SEQ_CTRL_FORCE_TX
                        | SEQ_CTRL_TONE_MOD);
    SEQ_CTRL = ctrl & ~(SEQ_CTRL_CHAN_LOAD | SEQ_CTRL_RF_LOAD | SEQ_CTRL_FORCE_TX
                        | SEQ_CTRL_TONE_MOD | SEQ_CTRL_TONE_LOAD);
    REG_SET_BITS(RXDIG_CTRL, RXDIG_CTRL_TONE_MODE);

    if (enable) {
        REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_BAND, band);
        REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_CHANNEL, channel);
        if (mode == 1) {
            /* Modulated tone from the sequencer's own source. */
            REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_TONE_MOD);
            REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_TONE_LOAD);
            REG_SET_BITS(SEQ_CHAN, SEQ_CHAN_TONE_OFFSET);
            REG_WRITE_FIELD(SEQ_TONE_CFG, SEQ_TONE_CFG_SOURCE, 2);
            REG_CLEAR_BITS(RXDIG_CTRL, RXDIG_CTRL_TONE_MODE);
        } else {
            REG_WRITE_FIELD(SEQ_CFG, SEQ_CFG_TONE_CODE, tx_tone_mode_code[mode]);
        }
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_FORCE_TX);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_CHAN_LOAD);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_RF_LOAD);
        REG_WRITE_FIELD(AGC_CFG, AGC_CFG_GAIN_MODE, AGC_GAIN_MODE_FORCED);
        RX_GAIN_FORCE = (RX_GAIN_FORCE & ~RFIELD_MASK(RX_GAIN_FORCE_WORD))
                      + (enable == 1 ? 0xFEu : 0x40CDu);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_TONE_EN);
        REG_SET_BITS(SEQ_CFG, SEQ_CFG_TONE_RUN);
    }
    return BTP_OK;
}

/* Scale the TX DAC I/Q gains of one radio bank from the sequencer power code. */
static void tx_prbs_program_dac_gain(uint32_t bank, uint32_t gain)
{
    uint32_t reg = RADIO_TXDAC_GAIN(bank);

    RADIO_TXDAC_GAIN(bank) = reg | RADIO_TXDAC_GAIN_OVERRIDE;
    RADIO_TXDAC_GAIN(bank) = reg | RADIO_TXDAC_GAIN_OVERRIDE | RADIO_TXDAC_GAIN_LOAD;
    RADIO_TXDAC_GAIN(bank) = (reg & ~RFIELD_MASK(RADIO_TXDAC_GAIN_I))
                           | RADIO_TXDAC_GAIN_OVERRIDE | RADIO_TXDAC_GAIN_LOAD | gain;
    RADIO_TXDAC_GAIN(bank) = (reg & ~(RFIELD_MASK(RADIO_TXDAC_GAIN_I) | RFIELD_MASK(RADIO_TXDAC_GAIN_Q)))
                           | RADIO_TXDAC_GAIN_OVERRIDE | RADIO_TXDAC_GAIN_LOAD
                           | gain | (gain << RFIELD_SHIFT(RADIO_TXDAC_GAIN_Q));
}

/* @provenance RVA 0x29A0 rf_test_configure_prbs_tx */
btp_status_t rf_test_prbs(uint32_t mode, uint8_t band, uint8_t channel)
{
    uint32_t ctrl, unlock, dac;

    /* Tear down any running transmission and reset the PRBS engine. */
    REG_CLEAR_BITS(SEQ_CTRL, SEQ_CTRL_TX_START);
    REG_CLEAR_BITS(SEQ_CFG, SEQ_CFG_PRBS_RUN);
    unlock = SEQ_REG_UNLOCK;
    SEQ_REG_UNLOCK = unlock & ~(SEQ_REG_UNLOCK_RADIO | SEQ_REG_UNLOCK_RADIO_SEQ);
    SEQ_REG_UNLOCK = unlock & ~(SEQ_REG_UNLOCK_RADIO | SEQ_REG_UNLOCK_RADIO_SEQ
                                | SEQ_REG_UNLOCK_MODEM);
    REG_CLEAR_BITS(TXDIG_CTRL, TXDIG_CTRL_REG_UNLOCK);
    ctrl = SEQ_CTRL;
    SEQ_CTRL = ctrl & ~SEQ_CTRL_CHAN_LOAD;
    SEQ_CTRL = ctrl & ~(SEQ_CTRL_CHAN_LOAD | SEQ_CTRL_RF_LOAD);
    dac = RADIO_TXDAC_GAIN(RADIO_BANK_B);
    RADIO_TXDAC_GAIN(RADIO_BANK_B) = dac & ~RADIO_TXDAC_GAIN_OVERRIDE;
    RADIO_TXDAC_GAIN(RADIO_BANK_B) = dac & ~(RADIO_TXDAC_GAIN_OVERRIDE | RADIO_TXDAC_GAIN_LOAD);
    dac = RADIO_TXDAC_GAIN(RADIO_BANK_A);
    RADIO_TXDAC_GAIN(RADIO_BANK_A) = dac & ~RADIO_TXDAC_GAIN_OVERRIDE;
    RADIO_TXDAC_GAIN(RADIO_BANK_A) = dac & ~(RADIO_TXDAC_GAIN_OVERRIDE | RADIO_TXDAC_GAIN_LOAD);
    REG_CLEAR_BITS(PRBS_CFG, PRBS_CFG_RESET);
    systick_delay(10);
    REG_SET_BITS(PRBS_CFG, PRBS_CFG_RESET);
    systick_delay(10);
    REG_CLEAR_BITS(PRBS_CFG, PRBS_CFG_RUN);
    systick_delay(50);
    REG_CLEAR_BITS(PRBS_CTRL, PRBS_CTRL_EN);
    systick_delay(50);
    REG_SET_BITS(SEQ_REG_UNLOCK, SEQ_REG_UNLOCK_PRBS_RESET);
    systick_delay(50);
    REG_CLEAR_BITS(SEQ_REG_UNLOCK, SEQ_REG_UNLOCK_PRBS_RESET);

    if (mode == 1) {
        uint32_t iq_scale, pwr_code, gain;

        /* Continuous transmission at the calibrated power. */
        REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_BAND, band);
        REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_CHANNEL, channel);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_CHAN_LOAD);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_RF_LOAD);
        txpwr_setup_from_command();
        REG_SET_BITS(SEQ_CFG, SEQ_CFG_PRBS_RUN);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_TX_START);
        SEQ_REG_UNLOCK = (SEQ_REG_UNLOCK & ~(SEQ_REG_UNLOCK_RADIO | SEQ_REG_UNLOCK_RADIO_SEQ))
                       | SEQ_REG_UNLOCK_RADIO;
        REG_SET_BITS(SEQ_REG_UNLOCK, SEQ_REG_UNLOCK_MODEM);

        /* DAC gain = (pwr_code * 4095 / 128) * iq_scale / 8192, 12 bits */
        iq_scale = TXDIG_IQ_SCALE;
        pwr_code = FIELD_GET(SEQ_CHAN_PWR_CODE, SEQ_CHAN);
        gain = ((((0xFFFu * pwr_code) >> 7) * FIELD_GET(TXDIG_IQ_SCALE_VALUE, iq_scale)) >> 13)
             & TX_A4P_FIELD_MAX;
        tx_prbs_program_dac_gain(RADIO_BANK_B, gain);
        tx_prbs_program_dac_gain(RADIO_BANK_A, gain);
    } else if (mode & 0x40u) {
        /* PRBS pattern: bits [3:0] pattern, bits [10:7] rate. */
        REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_BAND, band);
        REG_WRITE_FIELD(SEQ_CHAN, SEQ_CHAN_CHANNEL, channel);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_CHAN_LOAD);
        REG_SET_BITS(SEQ_CTRL, SEQ_CTRL_RF_LOAD);
        REG_WRITE_FIELD(PRBS_CTRL, PRBS_CTRL_PATTERN, mode & 0xFu);
        REG_CLEAR_BITS(PRBS_CTRL, RFIELD_MASK(PRBS_CTRL_RATE));
        REG_WRITE_FIELD(PRBS_CTRL, PRBS_CTRL_RATE, (mode >> 7) & 0xFu);
        PRBS_SEED1 = 0xFF83DF17u;
        PRBS_SEED0 = (PRBS_SEED0 & 0xFFFFu) | 0xFFFF0000u;
        PRBS_LENGTH = 0x7E;
        REG_CLEAR_BITS(PRBS_CFG, PRBS_CFG_RUN);
        REG_CLEAR_BITS(PRBS_CFG, PRBS_CFG_RESET);
        systick_delay(10);
        REG_SET_BITS(PRBS_CTRL, PRBS_CTRL_EN);
        systick_delay(50);
        REG_SET_BITS(PRBS_CFG, PRBS_CFG_RUN);
    }
    return BTP_OK;
}

/* ------------------------------------------------------------------------- */
/* Per-packet setup                                                          */
/* ------------------------------------------------------------------------- */

/* Program the PA bias for a non-zero band from the saved reset values, or the
 * fixed high-rate setting when the packet class asks for it. */
static void tx_program_pa_bias(uint32_t cmd1)
{
    uint32_t bias = RADIO_PA_BIAS & ~RFIELD_MASK(RADIO_PA_BIAS_STAGE1);

    if (FIELD_GET(SEQ_PKT_CMD1_RATE, cmd1) == 6) {
        RADIO_PA_BIAS = bias | FIELD_PREP(RADIO_PA_BIAS_STAGE1, 8);
        REG_WRITE_FIELD(RADIO_PA_BIAS, RADIO_PA_BIAS_STAGE2, 7);
        RADIO_PA_BIAS = RADIO_PA_BIAS & ~RFIELD_MASK(RADIO_PA_BIAS_MODE);
    } else {
        RADIO_PA_BIAS = bias | FIELD_PREP(RADIO_PA_BIAS_STAGE1, tx.saved_pa_bias_stage1);
        REG_WRITE_FIELD(RADIO_PA_BIAS, RADIO_PA_BIAS_STAGE2, tx.saved_pa_bias_stage2);
        RADIO_PA_BIAS = (RADIO_PA_BIAS & ~RFIELD_MASK(RADIO_PA_BIAS_MODE))
                      | FIELD_PREP(RADIO_PA_BIAS_MODE, tx.saved_pa_bias_mode);
    }
}

/*
 * A4P pre-distortion and KVCO compensation.  Produces the gain and slope words
 * of TXDIG_A4P from the channel, the per-band slope calibration and (if
 * present) the KVCO calibration.
 */
static void tx_program_a4p(uint32_t channel, unsigned band, unsigned alt_type,
                           uint32_t power_mode)
{
    const uint8_t *slopes = nvv.a4p_cal_slopes;
    uint32_t gain, slope;
    unsigned kvco_column = 0;
    bool use_kvco = true;

    if (band != 0 || alt_type != 0) {
        float scaled = (float)channel * (float)slopes[2 * band + alt_type] * TX_A4P_SLOPE_SCALE
                     + TX_A4P_GAIN_BIAS;

        gain = (uint32_t)roundf_half_up(scaled);
        slope = (uint32_t)roundf_half_up(TX_A4P_RECIPROCAL / (float)gain);
        switch (band) {
        case 0:  kvco_column = alt_type;              break;
        case 1:  use_kvco = false;                    break;
        case 2:  kvco_column = alt_type + 2;          break;
        case 3:  kvco_column = 0;                     break;
        default: kvco_column = 2 * band + alt_type - 4; break;
        }
    } else {
        gain = *(const uint16_t *)&nvv.a4p_cal[2 * channel];
        slope = (uint32_t)roundf_half_up(TX_A4P_RECIPROCAL / (float)gain);
    }

    if (use_kvco) {
        float corrected = 0.0f;
        bool apply = false;

        if (nvv.kvco_cal_lp != NULL) {
            if (power_mode == 1) {
                corrected = (float)slope / ((float)nvv.kvco_cal_lp[kvco_column] * TX_KVCO_SCALE);
                apply = true;
            }
        } else if (power_mode == 0 && nvv.kvco_cal != NULL) {
            corrected = (float)slope * (float)nvv.kvco_cal[kvco_column] * TX_KVCO_SCALE;
            apply = true;
        }
        if (apply) {
            slope = (uint32_t)roundf_half_up(corrected);
            gain = (uint32_t)roundf_half_up(TX_A4P_RECIPROCAL / (float)slope);
        }
    }

    if (gain >= TX_A4P_FIELD_MAX)
        gain = TX_A4P_FIELD_MAX;
    REG_WRITE_FIELD(TXDIG_A4P, TXDIG_A4P_GAIN, gain);
    REG_WRITE_FIELD(TXDIG_A4P, TXDIG_A4P_SLOPE, slope < TX_A4P_FIELD_MAX ? slope : TX_A4P_FIELD_MAX);
}

/*
 * DF1 mantissa: the dynamic table value unless one of the calibration tables
 * (newest first) supplies a per-band/per-mode override for this packet.
 */
static uint16_t tx_select_df1_mantissa(uint32_t cmd0, uint32_t cmd1, unsigned band,
                                       uint32_t power_mode, unsigned alt_type,
                                       unsigned dynamic_index, uint16_t *modulation)
{
    uint16_t mantissa = (uint16_t)*(const uint32_t *)&nvv.dynamic_mant[4 * dynamic_index];
    bool alt_format = (cmd0 & SEQ_PKT_CMD0_ALT_FORMAT) != 0;
    unsigned pkt_type = FIELD_GET(SEQ_PKT_CMD1_PKT_TYPE, cmd1);
    bool mid_type = (pkt_type - 6u) <= 2u;              /* packet types 6..8 */
    uint16_t cal;

    if (tx_ovr.df1_mantissa_override) {
        if (band == 0)
            mantissa = tx_ovr.df1_mantissa_value;
        else
            *modulation = tx_ovr.df1_mantissa_value;
        return mantissa;
    }

    if (nvv.df1_cal_v4 != NULL) {
        unsigned alt_col = alt_format && FIELD_GET(SEQ_PKT_CMD0_ALT_RATE, cmd0) == 2;

        cal = *(const uint16_t *)&nvv.df1_cal_v4[4 * band + 2 * alt_col];
        if (band != 0) {
            if (alt_format)
                *modulation = cal;
            return mantissa;
        }
        if (!alt_format && !mid_type)
            return mantissa;
        if (cal == 0 || alt_type)
            return mantissa;
        return cal;
    }

    if (nvv.df1_cal_v3 != NULL) {
        cal = *(const uint16_t *)&nvv.df1_cal_v3[2 * (uint8_t)(power_mode + 2 * band)];
        if (band != 0) {
            if (alt_format)
                *modulation = cal;
            return mantissa;
        }
        if (band == 0 && (alt_format || mid_type)) {
            if (cal == 0 || alt_type)
                return mantissa;
            return cal;
        }
        return mantissa;
    }

    if (nvv.df1_cal_v2 != NULL) {
        if (band == 0) {
            bool eligible = alt_format
                         || FIELD_GET(SEQ_PKT_CMD1_RATE, cmd1) == 3
                         || FIELD_GET(SEQ_PKT_CMD1_PKT_TYPE, cmd1) == 8;

            if (eligible && power_mode != 0 && alt_type == 0)
                return *(const uint16_t *)nvv.df1_cal_v2;
        } else if (alt_format) {
            unsigned index = band > 3 ? band - 1 : band;
            *modulation = *(const uint16_t *)&nvv.df1_cal_v2[2 * index];
        }
        return mantissa;
    }

    if (nvv.df1_mant_cal != NULL && alt_format) {
        unsigned index = band;

        if (index > 3)
            index -= 1;
        if (power_mode == 0 || alt_type)
            return mantissa;
        return *(const uint16_t *)&nvv.df1_mant_cal[2 * index];
    }

    return mantissa;
}

/* Which of the five HF coefficient sets of a band applies to this packet. */
static unsigned tx_select_hf_coeff_set(unsigned band, uint32_t cmd1, bool alt_format)
{
    unsigned pkt_type = FIELD_GET(SEQ_PKT_CMD1_PKT_TYPE, cmd1);

    if (alt_format)
        return 0;

    if (band == 0) {
        if (pkt_type <= TX_PKT_TYPE_MAX_DYNAMIC) {
            if ((1u << pkt_type) & TX_ALT_TYPE_MASK_BAND0)
                return 1;
            if ((1u << pkt_type) & 0xC0u)
                return (SEQ_RF_CTRL & RFIELD_MASK(SEQ_RF_CTRL_PA_STAGE))
                       == FIELD_PREP(SEQ_RF_CTRL_PA_STAGE, 1) ? 0 : 3;
            if (pkt_type == 8)
                return 2;
        }
        return 3;
    }

    if ((pkt_type & 0xEu) == 0xC)
        return 0;
    if (pkt_type == 9) {
        if ((SEQ_RF_CTRL & RFIELD_MASK(SEQ_RF_CTRL_PA_STAGE))
            == FIELD_PREP(SEQ_RF_CTRL_PA_STAGE, 2))
            return 4;
        /* The fallback tests a fresh read at 0x34B8. */
        return (SEQ_RF_CTRL & RFIELD_MASK(SEQ_RF_CTRL_PA_STAGE))
               == FIELD_PREP(SEQ_RF_CTRL_PA_STAGE, 2) ? 3 : 1;
    }
    return pkt_type == 8 ? 2 : 3;
}

static void tx_load_hf_coeffs(unsigned band, unsigned set)
{
    const uint8_t *coeffs = nvv.hf_coeff + 3 * (5 * band + set) * 8;

    for (unsigned i = 0; i < TXDIG_HF_COEFF_COUNT; ++i)
        TXDIG_HF_COEFF(i) = *(const uint32_t *)&coeffs[4 * i];
}

/* @provenance RVA 0x2BB8 tx_setup_isr */
uint32_t tx_setup_isr(void)
{
    struct trace_event ev;
    uint32_t cmd0, cmd1, power_mode, rf_ctrl, delay, latency;
    uint32_t format_cmd, rate_cmd, channel_cmd;
    unsigned band, pkt_type, alt_type, dynamic_index, delay_pair, hf_set;
    bool alt_format, high_band;
    uint16_t modulation;

    trace_begin(&ev, TRACE_TYPE_ISR, TRACE_ISR_SUBTYPE, TRACE_ISR_TX_SETUP, systick_now());
    trace_log(TRACE_SITE_TX_SETUP_ENTER, &ev);

    ++tx.packet_count;
    cmd0 = SEQ_PKT_CMD0;
    cmd1 = SEQ_PKT_CMD1;
    format_cmd = SEQ_PKT_CMD0;
    rate_cmd = SEQ_PKT_CMD0;
    channel_cmd = SEQ_PKT_CMD0;
    band = FIELD_GET(SEQ_PKT_CMD0_BAND, cmd0);
    pkt_type = FIELD_GET(SEQ_PKT_CMD1_PKT_TYPE, cmd1);
    alt_format = (format_cmd & SEQ_PKT_CMD0_ALT_FORMAT) != 0;

    if (band != 0)
        tx_program_pa_bias(cmd1);

    /* Power mode: forced by the host, else from init (band 1 always high). */
    if (tx_ovr.power_mode_override == 1)
        power_mode = tx_power_mode_override_value;
    else if (band == 1)
        power_mode = 1;
    else
        power_mode = tx_ovr.power_mode;
    REG_SET_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_PWR_MODE_LATCH);
    SEQ_RF_CTRL = (SEQ_RF_CTRL & ~SEQ_RF_CTRL_PWR_MODE)
                | ((power_mode & 1u) ? SEQ_RF_CTRL_PWR_MODE : 0u);

    /* Dynamic calibration column: power mode, +2 for the alternate packet
     * types, +4 for bands other than 0. */
    alt_type = 0;
    if (band == 0) {
        dynamic_index = power_mode;
        if (!alt_format && pkt_type <= TX_PKT_TYPE_MAX_DYNAMIC
            && ((1u << pkt_type) & TX_ALT_TYPE_MASK_BAND0)) {
            dynamic_index = power_mode + 2;
            alt_type = 1;
        }
        delay = TXDIG_DELAY;
        if (power_mode)
            delay |= RFIELD_MASK(TXDIG_DELAY_PWR_MODE);
        else
            delay = (delay & ~RFIELD_MASK(TXDIG_DELAY_PWR_MODE)) | FIELD_PREP(TXDIG_DELAY_PWR_MODE, 2);
        high_band = false;
    } else {
        dynamic_index = power_mode + 4;
        if (!alt_format && pkt_type <= TX_PKT_TYPE_MAX_DYNAMIC
            && ((1u << pkt_type) & TX_ALT_TYPE_MASK_OTHER)) {
            dynamic_index = power_mode + 6;
            alt_type = 1;
        }
        delay = TXDIG_DELAY & ~RFIELD_MASK(TXDIG_DELAY_PWR_MODE);
        if (power_mode)
            delay += FIELD_PREP(TXDIG_DELAY_PWR_MODE, 1);
        high_band = true;
    }
    TXDIG_DELAY = delay;
    TXDIG_BAND = alt_type | FIELD_PREP(TXDIG_BAND_INDEX, band);

    txpwr_setup_from_command();
    if (loopback_is_active())
        loopback_setup_tx_measurement(band, power_mode != 0);

    if (nvv.a4p_cal != NULL && nvv.a4p_cal_slopes != NULL && !tx_ovr.slope_cal_bypass)
        tx_program_a4p(FIELD_GET(SEQ_PKT_CMD0_CHANNEL, channel_cmd), band, alt_type, power_mode);

    phy_set_synth_band(high_band, true, band);

    rf_ctrl = SEQ_RF_CTRL;
    if (alt_type) {
        SEQ_RF_CTRL = rf_ctrl | SEQ_RF_CTRL_ALT_RATE;
        REG_SET_BITS(SEQ_TX_CFG, SEQ_TX_CFG_ALT_RATE);
        REG_SET_BITS(TXDIG_CTRL, TXDIG_CTRL_PACKET_EN);
        REG_SET_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_ALT_PATH);
        SEQ_RF_CTRL = SEQ_RF_CTRL | SEQ_RF_CTRL_TX_ALT_RATE;
    } else {
        SEQ_RF_CTRL = rf_ctrl & ~SEQ_RF_CTRL_ALT_RATE;
        REG_CLEAR_BITS(SEQ_TX_CFG, SEQ_TX_CFG_ALT_RATE);
        REG_CLEAR_BITS(TXDIG_CTRL, TXDIG_CTRL_PACKET_EN);
        REG_CLEAR_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_ALT_PATH);
        SEQ_RF_CTRL = SEQ_RF_CTRL & ~SEQ_RF_CTRL_TX_ALT_RATE;
    }
    REG_SET_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_TX_ACTIVE);

    /* DF1 mantissa and exponent. */
    modulation = band == 0 || alt_format ? 0x8000 : 0x999A;
    if (txpwr.band_override_enable[band])
        modulation = (uint16_t)txpwr.band_override_value[band];
    TXDIG_DF1 = (TXDIG_DF1 & ~RFIELD_MASK(TXDIG_DF1_MANTISSA))
              | tx_select_df1_mantissa((rate_cmd & ~SEQ_PKT_CMD0_ALT_FORMAT)
                                      | (format_cmd & SEQ_PKT_CMD0_ALT_FORMAT),
                                      cmd1, band, power_mode, alt_type, dynamic_index,
                                      &modulation);
    REG_WRITE_FIELD(TXDIG_DF1, TXDIG_DF1_EXPONENT, nvv.dynamic_exp[dynamic_index]);
    TXDIG_MOD_CTRL = (TXDIG_MOD_CTRL & ~TXDIG_MOD_CTRL_OVERRIDE_MASK)
                   | ((uint32_t)modulation << 16);

    if (txpwr_mode.iq_scale_override && pkt_type != 0)
        REG_WRITE_FIELD(TXDIG_IQ_SCALE, TXDIG_IQ_SCALE_VALUE, txpwr_iq_scale);

    /* Per-band delay lines: two entries per band (paths 0 and 1). */
    delay_pair = 2 * band;
    REG_WRITE_FIELD(TXDIG_AM_DELAY, TXDIG_AM_DELAY_PATH0, nvv.dynamic_am_delay[delay_pair]);
    REG_WRITE_FIELD(TXDIG_AM_DELAY, TXDIG_AM_DELAY_PATH1, nvv.dynamic_am_delay[delay_pair | 1]);
    REG_WRITE_FIELD(TXDIG_DELAY, TXDIG_DELAY_IF_PATH0, nvv.dynamic_if_delay[delay_pair]);
    REG_WRITE_FIELD(TXDIG_DELAY, TXDIG_DELAY_IF_PATH1, nvv.dynamic_if_delay[delay_pair | 1]);
    REG_WRITE_FIELD(TXDIG_DELAY, TXDIG_DELAY_HF_PATH0, nvv.dynamic_hf_delay[delay_pair]);
    REG_WRITE_FIELD(TXDIG_DELAY, TXDIG_DELAY_HF_PATH1, nvv.dynamic_hf_delay[delay_pair | 1]);

    if (!tx_ovr.hf_coeff_bypass) {
        hf_set = tx_select_hf_coeff_set(band, cmd1, alt_format);
        tx_load_hf_coeffs(band, hf_set);
    }

    ev.start_ts = 0;
    ev.arg1 = 0;
    ev.end_ts = systick_now();
    trace_log(TRACE_SITE_TX_SETUP_EXIT, &ev);
    REG_SET_BITS(SEQ_IRQ, SEQ_IRQ_TX_SETUP_DONE);

    dpd_on_tx_setup();

    latency = (uint32_t)(seq_timer_now() - seq_timer_tx_trigger());
    if (phy_status->telemetry.max_tx_isr_latency < latency)
        phy_status->telemetry.max_tx_isr_latency = latency;
    return latency;
}

/* @provenance RVA 0x3558 tx_done_isr */
void tx_done_isr(void)
{
    struct trace_event ev;

    trace_begin(&ev, TRACE_TYPE_ISR, TRACE_ISR_SUBTYPE, TRACE_ISR_TX_DONE, systick_now());
    trace_log(TRACE_SITE_TX_DONE, &ev);
    txpwr_packet_done();
    REG_SET_BITS(SEQ_IRQ, SEQ_IRQ_TX_DONE_ACK);
}

/* @provenance RVA 0x377C tx_init */
void tx_init(bool warm_boot)
{
    struct trace_event ev;

    tx.packet_count = 0;
    trace_begin(&ev, TRACE_TYPE_INIT, TRACE_INIT_TX, 0, systick_now());
    trace_log(TRACE_SITE_TX_INIT_ENTER, &ev);

    tx_ovr.power_mode = (SEQ_RF_CTRL & SEQ_RF_CTRL_PWR_MODE) != 0;
    tx.saved_pa_bias_stage1 = (uint8_t)FIELD_GET(RADIO_PA_BIAS_STAGE1, RADIO_PA_BIAS);
    tx.saved_pa_bias_stage2 = (uint8_t)FIELD_GET(RADIO_PA_BIAS_STAGE2, RADIO_PA_BIAS);
    tx.saved_pa_bias_mode = (uint8_t)FIELD_GET(RADIO_PA_BIAS_MODE, RADIO_PA_BIAS);
    dbg_printf("tx_pwr_mode=%d\n", tx_ovr.power_mode);

    if (!warm_boot)
        txpwr_init();
    loopback_init();

    ev.start_ts = 0;
    ev.end_ts = systick_now();
    trace_log(TRACE_SITE_TX_INIT_EXIT, &ev);
}
