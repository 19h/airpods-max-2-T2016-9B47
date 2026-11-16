/*
 * rx.c -- receive path.
 *
 * The sequencer raises IRQ3 (rx_setup) just before a packet is received and
 * IRQ4 (rx_done) after it.  Setup programs the receiver for the packet's band,
 * bandwidth class and type from the calibration tables; done collects the
 * packet statistics reported through telemetry.
 */
#include "btp/btp.h"
#include "btp/rx.h"
#include "btp/nvv.h"
#include "btp/syscfg.h"
#include "btp/status.h"
#include "btp/clock.h"
#include "btp/trace.h"

struct rx_overrides rx_ovr BTP_STATE(rx_ovr);
struct rx_state     rx     BTP_STATE(rx);

/* Bandwidth class by SEQ_PKT_CMD0_ALT_RATE when the alternate format is in use. */
static const uint8_t rx_alt_rate_to_bw[8] = {
    RX_BW_1P5, RX_BW_1P5, RX_BW_2, RX_BW_1P5, RX_BW_1P5, RX_BW_1P5, RX_BW_1P5, RX_BW_1P5,
};

/* Bandwidth class by packet type for the standard format. */
static const uint8_t rx_pkt_type_to_bw[BTP_PKT_TYPE_COUNT] = {
    RX_BW_1P5, RX_BW_1P5, RX_BW_1P5, RX_BW_1P5, RX_BW_1P5, RX_BW_1P5, RX_BW_1P5, RX_BW_1P5,
    RX_BW_2P5, RX_BW_2P5, RX_BW_3,   RX_BW_2P5, RX_BW_2P5, RX_BW_2P5, RX_BW_1P5, RX_BW_1P5,
};

/* Added to the attenuator lane of the max-gain table, per band. */
static const uint8_t rx_maxgain_band_offset[BTP_BAND_COUNT] = { 0, 0, 1, 1, 1, 2, 2, 2 };

/* Packet types that need the alternate demodulator parameter set. */
#define RX_PKT_TYPES_ALT_DEMOD      (BIT(8) | BIT(9) | BIT(11))
#define RX_MODE1_ALT_PARAMS         0xFD5u
#define RX_MODE0_ALT_DEMOD          0x7u

/* RSSI offset used when no A0 RSSI calibration is present. */
#define RX_RSSI_OFFSET_DEFAULT      0xD9

/* AGC target when the alternate packet format is in use (0x64 >> 2). */
#define RX_AGC_TARGET_ALT_FORMAT    0x19

/* FRS scan packets: fixed demodulator settings. */
#define RX_FRS_SYNC_LEN             0x34
#define RX_FRS_CFG2_BYTE            0x32
#define RX_FRS_TIMEOUT              0x32

static const uint32_t rx_frs_pkt_params[RX_PKT_PARAM_COUNT] = {
    0x88, 0x80, 0x14, 0x10, 0x0C, 0x08, 0x00,
};

/* ---- host overrides ------------------------------------------------------ */

/* @provenance RVA 0x1BCC rx_set_rssi_cal_override */
btp_status_t rx_set_rssi_cal_override(uint32_t enable, int16_t value)
{
    rx_ovr.rssi_cal_enable = enable;
    rx_ovr.rssi_cal_value = (uint16_t)value;
    return BTP_OK;
}

/* @provenance RVA 0x1BDC rx_set_mxdc_i_override */
btp_status_t rx_set_mxdc_i_override(uint32_t enable, uint32_t value)
{
    rx_ovr.mxdc_i_enable = enable;
    rx_ovr.mxdc_i = value;
    return BTP_OK;
}

/* @provenance RVA 0x1BEC rx_set_mxdc_q_override */
btp_status_t rx_set_mxdc_q_override(uint32_t enable, uint32_t value)
{
    rx_ovr.mxdc_q_enable = enable;
    rx_ovr.mxdc_q = value;
    return BTP_OK;
}

/* @provenance RVA 0x1BFC rx_set_filter_bw_override */
btp_status_t rx_set_filter_bw_override(uint8_t enable, uint8_t bw)
{
    rx_ovr.filter_bw = bw;
    rx_ovr.filter_bw_enable = enable;
    return BTP_OK;
}

/* @provenance RVA 0x2544 rx_set_iqmm_override */
btp_status_t rx_set_iqmm_override(uint8_t enable, unsigned param, int16_t value)
{
    rx.iqmm_override = enable;

    switch (param) {
    case 0: rx.iqmm_a[0] = (uint16_t)value; return BTP_OK;
    case 1: rx.iqmm_a[1] = (uint16_t)value; return BTP_OK;
    case 2: rx.iqmm_b[0] = (uint16_t)value; return BTP_OK;
    case 3: rx.iqmm_b[1] = (uint16_t)value; return BTP_OK;
    default: return BTP_ERR_INVALID;
    }
}

/* ---- per-packet programming ------------------------------------------------- */

/* Demodulator parameters: fixed values for the FRS scan packet, the defaults
 * captured at init for everything else. */
static void rx_apply_demod_params(unsigned pkt_type)
{
    if (pkt_type == BTP_PKT_TYPE_FRS_SCAN) {
        REG_WRITE_FIELD(RX_PKT_CFG0, RX_PKT_CFG0_SYNC_LEN, RX_FRS_SYNC_LEN);
        REG_WRITE_FIELD(RX_PKT_CFG2, RX_PKT_CFG2_BYTE0, RX_FRS_CFG2_BYTE);
        REG_WRITE_FIELD(RX_PKT_CFG2, RX_PKT_CFG2_BYTE1, RX_FRS_CFG2_BYTE);
        REG_WRITE_FIELD(RX_PKT_CFG1, RX_PKT_CFG1_TIMEOUT, RX_FRS_TIMEOUT);
        for (unsigned i = 0; i < RX_PKT_PARAM_COUNT; i++)
            RX_PKT_PARAM(i) = rx_frs_pkt_params[i];
    } else {
        REG_WRITE_FIELD(RX_PKT_CFG0, RX_PKT_CFG0_SYNC_LEN, rx.default_sync_len);
        REG_WRITE_FIELD(RX_PKT_CFG2, RX_PKT_CFG2_BYTE0, rx.default_cfg2_byte0);
        REG_WRITE_FIELD(RX_PKT_CFG2, RX_PKT_CFG2_BYTE1, rx.default_cfg2_byte1);
        REG_WRITE_FIELD(RX_PKT_CFG1, RX_PKT_CFG1_TIMEOUT, rx.default_timeout);
        for (unsigned i = 0; i < RX_PKT_PARAM_COUNT; i++)
            RX_PKT_PARAM(i) = rx.default_pkt_params[i];
    }
}

/* Program an IQ mismatch correction from an LNA-first / mixer-first pair of
 * calibration tables (u16 per band; the LNA table carries A and, 16 bytes on,
 * B). */
static void rx_write_iqmm(uint32_t scale, uint32_t phase, uint32_t a, uint32_t b)
{
    REG_WRITE_FIELD(FRS_CTRL, FRS_CTRL_MODE, 1);
    REG_WRITE_FIELD(RX_IQMM_SCALE, RX_IQMM_SCALE_VALUE, scale);
    REG_WRITE_FIELD(RX_IQMM_PHASE, RX_IQMM_PHASE_VALUE, phase);
    REG_WRITE_FIELD(RX_IQMM, RX_IQMM_A, a);
    REG_WRITE_FIELD(RX_IQMM, RX_IQMM_B, b);
}

static void rx_apply_iqmm_cal(unsigned bw, unsigned band)
{
    const uint8_t *lna, *mix;

    switch (bw) {
    case RX_BW_1P5: lna = nvv.rxiqmm_ab_lna1st_bw1p5; mix = nvv.rxiqmm_ab_mix1st_bw1p5; break;
    case RX_BW_2:   lna = nvv.rxiqmm_ab_lna1st_bw2;   mix = nvv.rxiqmm_ab_mix1st_bw2;   break;
    case RX_BW_2P5: lna = nvv.rxiqmm_ab_lna1st_bw2p5; mix = nvv.rxiqmm_ab_mix1st_bw2p5; break;
    case RX_BW_3:   lna = nvv.rxiqmm_ab_lna1st_bw3;   mix = nvv.rxiqmm_ab_mix1st_bw3;   break;
    default:        return;
    }

    if (lna != NULL && mix != NULL) {
        const uint16_t *lna16 = (const uint16_t *)lna;
        const uint16_t *mix16 = (const uint16_t *)mix;

        rx_write_iqmm(mix16[band], mix16[band + 8], lna16[band], lna16[band + 8]);
    }
}

/* RSSI calibration: A0 offset by band and bandwidth, the baselines captured
 * at init, then the SysCfg per-channel-range trim on top. */
static void rx_apply_rssi_cal(unsigned band, unsigned bw, unsigned channel)
{
    uint32_t offset = RX_RSSI_OFFSET_DEFAULT;

    if (nvv.a0_rssi_cal != NULL)
        offset = nvv.a0_rssi_cal[3 * band + bw];

    REG_WRITE_FIELD(RX_RSSI_OFFSET, RX_RSSI_OFFSET_VALUE, offset);
    REG_WRITE_FIELD(RX_RSSI_CAL0, RX_RSSI_CAL0_BASE, diag.rssi_baseline[0]);
    REG_WRITE_FIELD(RX_RSSI_CAL1, RX_RSSI_CAL1_MIX, diag.rssi_baseline[1]);
    REG_WRITE_FIELD(RX_RSSI_CAL1, RX_RSSI_CAL1_LNA, diag.rssi_baseline[2]);
    REG_WRITE_FIELD(RX_RSSI_CAL2, RX_RSSI_CAL2_ATTEN, diag.rssi_baseline[3]);

    if (syscfg.rssi_cal != NULL) {
        /* three trims per band: low, middle and high channel range */
        unsigned range = channel > 0x4B ? 2 : (channel > 0xE ? 1 : 0);
        uint8_t trim = (uint8_t)syscfg.rssi_cal[3 * band + range];

        REG_WRITE_FIELD(RX_RSSI_OFFSET, RX_RSSI_OFFSET_VALUE,
                        trim + FIELD_GET(RX_RSSI_OFFSET_VALUE, RX_RSSI_OFFSET));
        REG_WRITE_FIELD(RX_RSSI_CAL0, RX_RSSI_CAL0_BASE,
                        trim + FIELD_GET(RX_RSSI_CAL0_BASE, RX_RSSI_CAL0));
        REG_WRITE_FIELD(RX_RSSI_CAL1, RX_RSSI_CAL1_MIX,
                        trim + FIELD_GET(RX_RSSI_CAL1_MIX, RX_RSSI_CAL1));
        REG_WRITE_FIELD(RX_RSSI_CAL1, RX_RSSI_CAL1_LNA,
                        trim + FIELD_GET(RX_RSSI_CAL1_LNA, RX_RSSI_CAL1));
        REG_WRITE_FIELD(RX_RSSI_CAL2, RX_RSSI_CAL2_ATTEN,
                        trim + FIELD_GET(RX_RSSI_CAL2_ATTEN, RX_RSSI_CAL2));
    }
}

/* Max-gain limits.  Each band owns two lanes (LNA-first and mixer-first rows
 * of rx.maxgain) spread across the three limit registers. */
static void rx_apply_maxgain(unsigned band, unsigned bw)
{
    uint8_t lna = rx.maxgain[2 * band][bw];
    uint8_t mix = rx.maxgain[2 * band + 1][bw];

    switch (band) {
    case 0:
        REG_WRITE_FIELD(RX_MAXGAIN0, RX_MAXGAIN_LANE(3), lna);
        REG_WRITE_FIELD(RX_MAXGAIN0, RX_MAXGAIN_LANE(0), mix);
        break;
    case 1:
        REG_WRITE_FIELD(RX_MAXGAIN0, RX_MAXGAIN_LANE(4), lna);
        REG_WRITE_FIELD(RX_MAXGAIN0, RX_MAXGAIN_LANE(1), mix);
        break;
    case 2:
        REG_WRITE_FIELD(RX_MAXGAIN0, RX_MAXGAIN_LANE(5), lna);
        REG_WRITE_FIELD(RX_MAXGAIN0, RX_MAXGAIN_LANE(2), mix);
        break;
    case 3:
        REG_WRITE_FIELD(RX_MAXGAIN1, RX_MAXGAIN_LANE(0), lna);
        REG_WRITE_FIELD(RX_MAXGAIN1, RX_MAXGAIN_LANE(5), mix);
        break;
    case 4:
        REG_WRITE_FIELD(RX_MAXGAIN1, RX_MAXGAIN_LANE(1), lna);
        REG_WRITE_FIELD(RX_MAXGAIN2, RX_MAXGAIN_LANE(2), mix);
        break;
    case 5:
        REG_WRITE_FIELD(RX_MAXGAIN1, RX_MAXGAIN_LANE(2), lna);
        REG_WRITE_FIELD(RX_MAXGAIN2, RX_MAXGAIN_LANE(3), mix);
        break;
    case 6:
        REG_WRITE_FIELD(RX_MAXGAIN1, RX_MAXGAIN_LANE(3), lna);
        REG_WRITE_FIELD(RX_MAXGAIN2, RX_MAXGAIN_LANE(4), mix);
        break;
    default:
        REG_WRITE_FIELD(RX_MAXGAIN1, RX_MAXGAIN_LANE(4), lna);
        REG_WRITE_FIELD(RX_MAXGAIN2, RX_MAXGAIN_LANE(5), mix);
        break;
    }
}

/* Mixer DC offset for bands 1..7 (band 0 keeps the register as is). */
static void rx_apply_mxdc_cal(unsigned band)
{
    unsigned index;

    if (band == 0 || nvv.mxdc_cal == NULL)
        return;

    index = band > 3 ? 2 * band - 4 : 2 * band - 2;
    REG_WRITE_FIELD(RADIO_RX_MXDC, RADIO_RX_MXDC_Q, nvv.mxdc_cal[index]);
    REG_WRITE_FIELD(RADIO_RX_MXDC, RADIO_RX_MXDC_I, nvv.mxdc_cal[index + 1]);
}

/* @provenance RVA 0x1C10 rx_setup_isr */
uint32_t rx_setup_isr(void)
{
    struct trace_event ev;
    uint32_t cmd0, cmd1, format_cmd, rate_cmd, channel_cmd;
    unsigned band, pkt_type, bw, channel;
    bool alt_format, frs_packet;
    uint32_t latency;

    trace_begin(&ev, TRACE_TYPE_ISR, TRACE_ISR_SUBTYPE, TRACE_ISR_RX_SETUP, systick_now());
    trace_log(TRACE_SITE_RX_SETUP_ENTER, &ev);

    cmd0 = SEQ_PKT_CMD0;
    cmd1 = SEQ_PKT_CMD1;
    format_cmd = SEQ_PKT_CMD0;
    rate_cmd = SEQ_PKT_CMD0;
    channel_cmd = SEQ_PKT_CMD0;
    pkt_type = FIELD_GET(SEQ_PKT_CMD1_PKT_TYPE, cmd1);
    band = FIELD_GET(SEQ_PKT_CMD0_BAND, cmd0);
    channel = FIELD_GET(SEQ_PKT_CMD0_CHANNEL, channel_cmd);
    alt_format = (format_cmd & SEQ_PKT_CMD0_ALT_FORMAT) != 0;
    frs_packet = pkt_type == BTP_PKT_TYPE_FRS_SCAN || pkt_type == BTP_PKT_TYPE_FRS_WIDE;
    rx.packet_count++;

    /* The mode registers are rewritten per packet; rx_done_isr() restores them. */
    rx.saved_mode0 = RX_MODE0;
    rx.saved_mode1 = RX_MODE1;

    if (alt_format)
        bw = rx_alt_rate_to_bw[FIELD_GET(SEQ_PKT_CMD0_ALT_RATE, rate_cmd)];
    else
        bw = rx_pkt_type_to_bw[pkt_type];

    if (pkt_type <= 0xB && (BIT(pkt_type) & RX_PKT_TYPES_ALT_DEMOD)) {
        REG_WRITE_FIELD(RX_MODE1, RX_MODE1_PARAMS, RX_MODE1_ALT_PARAMS);
        REG_WRITE_FIELD(RX_MODE0, RX_MODE0_DEMOD, RX_MODE0_ALT_DEMOD);
    }

    if (bw == RX_BW_2P5 && !frs_packet)
        REG_SET_BITS(RX_DEMOD_CTRL, RX_DEMOD_CTRL_WIDEBAND);
    else
        REG_CLEAR_BITS(RX_DEMOD_CTRL, RX_DEMOD_CTRL_WIDEBAND);

    /* Front end: LNA bypass and analogue filter bandwidth. */
    if (frs_packet) {
        enum radio_bank bank = band <= 1 ? RADIO_BANK_B : RADIO_BANK_A;

        REG_CLEAR_BITS(RADIO_RXFE_CTRL(bank), RADIO_RXFE_CTRL_LNA_BYPASS);
        REG_WRITE_FIELD(RADIO_RX_FILTER, RADIO_RX_FILTER_BW, 3);
    } else {
        REG_SET_BITS(RADIO_RXFE_CTRL(RADIO_BANK_B), RADIO_RXFE_CTRL_LNA_BYPASS);
        REG_SET_BITS(RADIO_RXFE_CTRL(RADIO_BANK_A), RADIO_RXFE_CTRL_LNA_BYPASS);
        REG_WRITE_FIELD(RADIO_RX_FILTER, RADIO_RX_FILTER_BW, 6);
        if (band == 1)
            REG_SET_BITS(RADIO_RX_FILTER, FIELD_PREP(RADIO_RX_FILTER_BW, 7));
    }
    if (rx_ovr.filter_bw_enable)
        REG_WRITE_FIELD(RADIO_RX_FILTER, RADIO_RX_FILTER_BW, rx_ovr.filter_bw);

    rx_apply_demod_params(pkt_type);

    REG_WRITE_FIELD(RX_AGC_TARGET, RX_AGC_TARGET_LEVEL,
                    alt_format ? RX_AGC_TARGET_ALT_FORMAT : rx.default_agc_target);

    rx_apply_iqmm_cal(bw, band);
    if (rx.iqmm_override)
        rx_write_iqmm(rx.iqmm_a[0], rx.iqmm_a[1], rx.iqmm_b[0], rx.iqmm_b[1]);

    rx_apply_rssi_cal(band, bw, channel);

    if (nvv.rx_maxgain_cal != NULL) {
        rx_apply_maxgain(band, bw);
        if (alt_format && pkt_type >= 4)
            REG_WRITE_FIELD(RX_IQMM_AUX, RX_IQMM_AUX_GAIN, rx.maxgain[2 * band][3]);
    }

    rx_apply_mxdc_cal(band);
    if (rx_ovr.mxdc_i_enable)
        REG_WRITE_FIELD(RADIO_RX_MXDC, RADIO_RX_MXDC_I, rx_ovr.mxdc_i);
    if (rx_ovr.mxdc_q_enable)
        REG_WRITE_FIELD(RADIO_RX_MXDC, RADIO_RX_MXDC_Q, rx_ovr.mxdc_q);
    if (rx_ovr.rssi_cal_enable)
        REG_WRITE_FIELD(RX_RSSI_OFFSET, RX_RSSI_OFFSET_VALUE, rx_ovr.rssi_cal_value);

    phy_set_synth_band(band != 0, false, band);

    /* Tell the sequencer which receive mode to run and that setup is done. */
    {
        uint32_t mode = 0;

        if (!(SEQ_PKT_CMD0 & SEQ_PKT_CMD0_ALT_FORMAT)) {
            unsigned type = FIELD_GET(SEQ_PKT_CMD1_PKT_TYPE, SEQ_PKT_CMD1);

            if (type == BTP_PKT_TYPE_FRS_WIDE)
                mode = 2;
            if (type == BTP_PKT_TYPE_FRS_SCAN)
                mode = 1;
        }
        REG_WRITE_FIELD(SEQ_IRQ, SEQ_IRQ_RX_MODE, mode);
        REG_SET_BITS(SEQ_IRQ, SEQ_IRQ_RX_SETUP_DONE);
    }

    ev.start_ts = 0;
    ev.end_ts = systick_now();
    trace_log(TRACE_SITE_RX_SETUP_EXIT, &ev);

    latency = (uint32_t)seq_timer_now() - (uint32_t)seq_timer_rx_trigger();
    if (phy_status->telemetry.max_rx_isr_latency < latency)
        phy_status->telemetry.max_rx_isr_latency = latency;
    return latency;
}

/* @provenance RVA 0x2470 rx_done_isr */
void rx_done_isr(void)
{
    struct trace_event ev;
    uint32_t status;
    int32_t cfo, rssi;
    unsigned bin;

    trace_begin(&ev, TRACE_TYPE_ISR, TRACE_ISR_SUBTYPE, TRACE_ISR_RX_DONE, systick_now());
    trace_log(TRACE_SITE_RX_DONE, &ev);

    REG_SET_BITS(SEQ_IRQ, SEQ_IRQ_RX_DONE_ACK);

    status = RX_PKT_STATUS;

    /* 10-bit two's-complement carrier frequency offset */
    cfo = (int32_t)FIELD_GET(RX_PKT_STATUS_CFO, status);
    if (cfo > 0x200)
        cfo -= 0x400;
    rx.cfo_sum += cfo;
    if (cfo > rx.cfo_max)
        rx.cfo_max = cfo;
    if (cfo < rx.cfo_min)
        rx.cfo_min = cfo;

    /* The RSSI byte is taken to be a negative dBm value. */
    rssi = (int32_t)(status | ~RFIELD_MASK(RX_PKT_STATUS_RSSI));
    if (rssi >= -80)
        bin = rssi >= -70 ? (rssi < -50 ? 1 : 0) : 2;
    else
        bin = 3;
    rx.rssi_histogram[bin]++;

    RX_MODE0 = rx.saved_mode0;
    RX_MODE1 = rx.saved_mode1;
}

/* ---- gain tables -------------------------------------------------------- */

/*
 * Convert the max-gain calibration (16 rows of three packed 10-bit values)
 * into the per-lane limits programmed by rx_apply_maxgain().  Even rows are
 * referenced to the LNA-first gain, odd rows to the mixer-first gain; row 1 is
 * biased differently from the rest.
 */
/* @provenance RVA 0x2570 rx_update_maxgain_tables */
void rx_update_maxgain_tables(void)
{
    const uint32_t *cal = (const uint32_t *)nvv.rx_maxgain_cal;
    uint32_t cfg;

    if (cal == NULL)
        return;

    cfg = RX_MAXGAIN_CFG;
    RX_MAXGAIN_CFG = cfg & ~0x00007C00u;
    RX_MAXGAIN_CFG = cfg & ~0x000FFC00u;
    RX_MAXGAIN_CFG = (cfg & ~0x000FFFE0u) | RX_MAXGAIN_CFG_VALUE;

    for (unsigned row = 0; row < RX_MAXGAIN_ROWS; row++) {
        uint8_t *lane = rx.maxgain[row];
        uint32_t word = cal[row];
        uint32_t g0 = word & 0x3FF;
        uint32_t g1 = (word >> 10) & 0x3FF;
        uint32_t g2 = (word >> 20) & 0x3FF;

        lane[1] = (uint8_t)((g1 + 6) / 12);
        lane[0] = (uint8_t)((g0 + 6) / 12);
        lane[3] = (uint8_t)((g0 + 6) / 12);
        lane[2] = (uint8_t)((g2 + 6) / 12);

        if (row == 1) {
            lane[1] = (uint8_t)((g1 - 12) / 12);
            lane[0] = (uint8_t)((g0 - 12) / 12);
            lane[2] = (uint8_t)((g2 - 12) / 12);
        } else if ((row & 1) == 0) {
            uint8_t floor;

            for (unsigned i = 0; i < 3; i++) {
                uint8_t ref = rx.maxgain_ref_lna;

                if (lane[i] > ref)
                    ref += 0x20;
                lane[i] = (uint8_t)(ref - lane[i]);
            }
            floor = (uint8_t)(lane[3] - rx.maxgain_ref_lna - 0x66);
            if (floor <= 0x98)
                floor = 0x98;
            lane[3] = (uint8_t)(floor + rx_maxgain_band_offset[row >> 1]);
            continue;
        }

        for (unsigned i = 0; i < 3; i++) {
            uint8_t ref = rx.maxgain_ref_mix;

            if (lane[i] > ref)
                ref += 0x20;
            lane[i] = (uint8_t)(ref - lane[i]);
        }
    }
}

/* @provenance RVA 0x269C rx_init */
void rx_init(void)
{
    struct trace_event ev;
    uint32_t now = systick_now();

    rx.packet_count = 0;
    for (unsigned i = 0; i < ARRAY_SIZE(rx.rssi_histogram); i++)
        rx.rssi_histogram[i] = 0;
    rx.cfo_sum = 0;
    rx.cfo_max = RX_CFO_MAX_INIT;
    rx.iqmm_override = 0;
    rx.iqmm_b[0] = rx.iqmm_b[1] = 0;
    rx.iqmm_a[0] = rx.iqmm_a[1] = 0;
    rx.cfo_min = RX_CFO_MIN_INIT;

    trace_begin(&ev, TRACE_TYPE_INIT, TRACE_INIT_RX, 0, now);
    trace_log(TRACE_SITE_RX_INIT_ENTER, &ev);

    rx.maxgain_ref_lna = RX_MAXGAIN_REF_LNA_DEFAULT;
    rx_ovr.coex_backoff_active = 0;
    rx.maxgain_ref_mix = RX_MAXGAIN_REF_MIX_DEFAULT;
    rx_update_maxgain_tables();

    /* Capture the reset values the per-packet setup restores. */
    rx.default_sync_len = FIELD_GET(RX_PKT_CFG0_SYNC_LEN, RX_PKT_CFG0);
    {
        uint32_t cfg2 = RX_PKT_CFG2;

        rx.default_cfg2_byte0 = FIELD_GET(RX_PKT_CFG2_BYTE0, cfg2);
        rx.default_cfg2_byte1 = FIELD_GET(RX_PKT_CFG2_BYTE1, cfg2);
    }
    rx.default_timeout = FIELD_GET(RX_PKT_CFG1_TIMEOUT, RX_PKT_CFG1);
    for (unsigned i = 0; i < RX_PKT_PARAM_COUNT; i++)
        rx.default_pkt_params[i] = RX_PKT_PARAM(i);
    rx.default_agc_target = FIELD_GET(RX_AGC_TARGET_LEVEL, RX_AGC_TARGET);

    ev.start_ts = 0;
    ev.end_ts = systick_now();
    trace_log(TRACE_SITE_RX_INIT_EXIT, &ev);
}

/* ---- coexistence gain back-off ------------------------------------------- */

#define COEX_BACKOFF_MIX            0x1D
#define COEX_BACKOFF_LNA            0x25
#define COEX_BACKOFF_LIMIT          0x1B

/* @provenance RVA 0x28DC coex_enable_rx_gain_backoff */
void coex_enable_rx_gain_backoff(void)
{
    rx_ovr.coex_backoff_active = 1;
    rx.coex_saved_mix = FIELD_GET(COEX_GAIN_MIX, COEX_GAIN);
    rx.coex_saved_lna = FIELD_GET(COEX_GAIN_LNA, COEX_GAIN);
    rx.coex_saved_limit = FIELD_GET(COEX_GAIN2_LIMIT, COEX_GAIN2);

    REG_WRITE_FIELD(COEX_GAIN, COEX_GAIN_MIX, COEX_BACKOFF_MIX);
    REG_WRITE_FIELD(COEX_GAIN, COEX_GAIN_LNA, COEX_BACKOFF_LNA);
    REG_WRITE_FIELD(COEX_GAIN2, COEX_GAIN2_LIMIT, COEX_BACKOFF_LIMIT);
}

/* @provenance RVA 0x292C coex_disable_rx_gain_backoff */
void coex_disable_rx_gain_backoff(void)
{
    rx_ovr.coex_backoff_active = 0;
    REG_WRITE_FIELD(COEX_GAIN, COEX_GAIN_MIX, rx.coex_saved_mix);
    REG_WRITE_FIELD(COEX_GAIN, COEX_GAIN_LNA, rx.coex_saved_lna);
    REG_WRITE_FIELD(COEX_GAIN2, COEX_GAIN2_LIMIT, rx.coex_saved_limit);
}

/* @provenance RVA 0x2964 coex_is_active */
uint32_t coex_is_active(void)
{
    return rx_ovr.coex_backoff_active;
}
