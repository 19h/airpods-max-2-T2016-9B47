/*
 * loopback.c -- TX->RX loopback measurement.
 *
 * With loopback engaged the radio routes the transmitter back into the
 * receiver and two correlators in the DPD block integrate I/Q over fixed
 * sample windows of every packet.  The correlator results are accumulated
 * into min/max/sum statistics that the host reads back through the mailbox.
 * The loopback DC-offset and IQ-mismatch corrections come from the NVV
 * calibration tables, indexed by band and by the TX power mode.
 */
#include "btp/btp.h"
#include "btp/loopback.h"
#include "btp/nvv.h"
#include "btp/trace.h"

struct loopback_state       lb       BTP_STATE(loopback);
struct loopback_saved_radio lb_saved BTP_STATE(lb_saved);

volatile uint32_t loopback_stats_address BTP_DATA(loopback_stats_cell) = 0x4D28996C;
volatile uint32_t loopback_control_address BTP_DATA(loopback_control_cell) = 0x4D2899A0;

static volatile struct loopback_stats *lb_stats_base(void)
{
#ifdef BTP_HOST
    return &lb.stats;
#else
    return BTP_PTR(loopback_stats_address);
#endif
}

static volatile uint32_t *lb_control_base(void)
{
#ifdef BTP_HOST
    return &lb.control;
#else
    return BTP_PTR(loopback_control_address);
#endif
}

/* Correlator and capture windows, in samples from the packet start. */
#define LB_CORR0_START              0x15E0
#define LB_CORR0_END                0x1DE0
#define LB_CORR1_START              0x1F40
#define LB_CORR1_END                0x2740
#define LB_CAPTURE0_START           0x1E00
#define LB_CAPTURE0_END             0x1E14
#define LB_CAPTURE1_START           0x2760
#define LB_CAPTURE1_END             0x2774
#define LB_CORR_WINDOW              4
#define LB_SEQ_MODE                 5
#define LB_SEQ_WINDOW0_START_VALUE  0xD0001540u
#define LB_SEQ_WINDOW0_END_POS      0x1E50
#define LB_SEQ_WINDOW1_START_VALUE  0xC0001EA0u
#define LB_SEQ_WINDOW1_END_POS      0x2800
#define LB_SAMPLE_COUNT_MAX         0xFF

/* Each loopback calibration table holds one u16 per band for the low-power
 * mode (band 0 first, then bands 1..7) followed by a second block of u16s;
 * the high-power tables are flat u16 arrays indexed by band. */
#define LB_CAL_LP_SECOND_BLOCK      0x0E

/* ------------------------------------------------------------------------- */
/* Calibration                                                               */
/* ------------------------------------------------------------------------- */

/* @provenance RVA 0x4288 loopback_apply_dcoc_cal */
int loopback_apply_dcoc_cal(unsigned index, bool high_power)
{
    const uint8_t *table = high_power ? nvv.lb_dcoc_cal_hp : nvv.lb_dcoc_cal_lp;
    volatile uint32_t *cfg = &RADIO_LB_CFG(RADIO_BANK_A);
    const uint8_t *entry;
    uint16_t dcoc;

    if (table == NULL)
        return 0;

    if (high_power) {
        entry = &table[2 * index];
    } else if (index != 0) {
        entry = &table[2 * index - 2];
    } else {
        /* band 0 lives on the other radio bank */
        cfg = &RADIO_LB_CFG(RADIO_BANK_B);
        entry = table;
    }

    dcoc = *(const uint16_t *)entry;
    REG_WRITE_FIELD(*cfg, RADIO_LB_CFG_DCOC_I, dcoc);
    REG_WRITE_FIELD(*cfg, RADIO_LB_CFG_DCOC_Q, dcoc >> 8);
    return 0;
}

/* Locate the two u16 parameters of an IQMM table for `index`. */
static bool lb_iqmm_entries(const uint8_t *table, unsigned index, bool high_power,
                            const uint16_t **first, const uint16_t **second)
{
    if (table == NULL)
        return false;

    if (high_power) {
        *first = (const uint16_t *)&table[2 * index];
        *second = (const uint16_t *)&table[2 * index + 0x10];
    } else if (index != 0) {
        *first = (const uint16_t *)&table[2 * index - 2];
        *second = (const uint16_t *)&table[2 * index + 0xC];
    } else {
        *first = (const uint16_t *)table;
        *second = (const uint16_t *)&table[LB_CAL_LP_SECOND_BLOCK];
    }
    return true;
}

/* @provenance RVA 0x42F8 loopback_apply_iqmm_dcoc_cal */
uint32_t loopback_apply_iqmm_dcoc_cal(unsigned index, bool high_power)
{
    const uint16_t *i_cal, *q_cal;

    if (!lb_iqmm_entries(high_power ? nvv.lbiqmm_dcoc_hp : nvv.lbiqmm_dcoc_lp,
                         index, high_power, &i_cal, &q_cal))
        return index;

    uint16_t high = *(const volatile uint16_t *)q_cal;
    uint16_t low = *(const volatile uint16_t *)i_cal;
    uint32_t result;

    REG_WRITE_FIELD(LB_IQMM_DCOC, LB_IQMM_DCOC_I, low);
    result = FIELD_REPLACE(LB_IQMM_DCOC, LB_IQMM_DCOC_Q, high);
    LB_IQMM_DCOC = result;
    return result;
}

/* @provenance RVA 0x4350 loopback_apply_iqmm_ab_cal */
uint32_t loopback_apply_iqmm_ab_cal(unsigned index, bool high_power)
{
    const uint16_t *a_cal, *b_cal;

    if (!lb_iqmm_entries(high_power ? nvv.lbiqmm_ab_hp : nvv.lbiqmm_ab_lp,
                         index, high_power, &a_cal, &b_cal))
        return index;

    uint16_t high = *(const volatile uint16_t *)b_cal;
    uint16_t low = *(const volatile uint16_t *)a_cal;
    uint32_t result;

    REG_WRITE_FIELD(LB_IQMM_AB, LB_IQMM_AB_A, low);
    result = FIELD_REPLACE(LB_IQMM_AB, LB_IQMM_AB_B, high);
    LB_IQMM_AB = result;
    return result;
}

/* ------------------------------------------------------------------------- */
/* Measurement windows                                                       */
/* ------------------------------------------------------------------------- */

/* @provenance RVA 0x449C loopback_configure_corr_window */
void loopback_configure_corr_window(unsigned corr, uint32_t start, uint32_t end)
{
    CORR_START(corr) = (CORR_START(corr) & ~RFIELD_MASK(CORR_POS)) + start;
    CORR_END(corr) = (CORR_END(corr) & ~RFIELD_MASK(CORR_POS)) + end;
    REG_WRITE_FIELD(CORR_CTRL(corr), CORR_CTRL_WINDOW, LB_CORR_WINDOW);
    REG_SET_BITS(CORR_TRIGGER, CORR_TRIGGER_CORR(corr));
}

/* @provenance RVA 0x43A8 loopback_setup_tx_measurement */
void loopback_setup_tx_measurement(unsigned band, bool high_power)
{
    uint8_t control = *(volatile uint8_t *)lb_control_base();

    if (!(control & LB_CTRL_SKIP_DCOC)) {
        loopback_apply_dcoc_cal(band, high_power);
        control = *(volatile uint8_t *)lb_control_base();
    }
    if (!(control & LB_CTRL_SKIP_IQMM_DCOC)) {
        loopback_apply_iqmm_dcoc_cal(band, high_power);
        control = *(volatile uint8_t *)lb_control_base();
    }
    if (!(control & LB_CTRL_SKIP_IQMM_AB))
        loopback_apply_iqmm_ab_cal(band, high_power);

    (void)LB_SEQ_WINDOW0_START;
    LB_SEQ_WINDOW0_START = LB_SEQ_WINDOW0_START_VALUE;
    REG_WRITE_FIELD(LB_SEQ_WINDOW0_END, LB_SEQ_WINDOW_POS, LB_SEQ_WINDOW0_END_POS);
    (void)LB_SEQ_WINDOW1_START;
    LB_SEQ_WINDOW1_START = LB_SEQ_WINDOW1_START_VALUE;
    REG_WRITE_FIELD(LB_SEQ_WINDOW1_END, LB_SEQ_WINDOW_POS, LB_SEQ_WINDOW1_END_POS);
    REG_WRITE_FIELD(LB_SEQ_CTRL, LB_SEQ_CTRL_MODE, LB_SEQ_MODE);

    loopback_configure_corr_window(0, LB_CORR0_START, LB_CORR0_END);
    loopback_configure_corr_window(1, LB_CORR1_START, LB_CORR1_END);

    REG_WRITE_FIELD(CORR_CAPTURE_START(0), CORR_CAPTURE_POS, LB_CAPTURE0_START);
    REG_WRITE_FIELD(CORR_CAPTURE_END(0), CORR_CAPTURE_POS, LB_CAPTURE0_END);
    REG_SET_BITS(CORR_CAPTURE_START(0), CORR_CAPTURE_EN);
    REG_WRITE_FIELD(CORR_CAPTURE_START(1), CORR_CAPTURE_POS, LB_CAPTURE1_START);
    REG_WRITE_FIELD(CORR_CAPTURE_END(1), CORR_CAPTURE_POS, LB_CAPTURE1_END);
    REG_SET_BITS(CORR_CAPTURE_START(1), CORR_CAPTURE_EN);
}

/* ------------------------------------------------------------------------- */
/* Correlator interrupts                                                     */
/* ------------------------------------------------------------------------- */

static inline int32_t lb_sign_extend_result(uint32_t raw)
{
    int32_t value = (int32_t)(raw & ((1u << CORR_RESULT_BITS) - 1u));

    if (raw & (1u << (CORR_RESULT_BITS - 1)))
        value -= 1 << CORR_RESULT_BITS;
    return value;
}

/* ARM ADD wraps at 32 bits. Keep both C operands and the signed result
 * conversion defined even after NVV has supplied extreme accumulator values. */
static int32_t lb_add_wrapped(int32_t sum, int32_t value)
{
    uint32_t bits = (uint32_t)sum + (uint32_t)value;

    if (bits <= INT32_MAX)
        return (int32_t)bits;
    return -1 - (int32_t)(UINT32_MAX - bits);
}

static void lb_accumulate(unsigned corr, int32_t i, int32_t q)
{
    volatile struct loopback_stats *s;
    int32_t old_i, old_q, max_i;
    uint8_t count;

    if (*(volatile uint8_t *)&lb.active != 1)
        return;
    s = lb_stats_base();
    count = s->sample_count;
    if (count == LB_SAMPLE_COUNT_MAX)
        return;

    if (corr == 0) {
        /* 0x4544..0x454A: Q sum and maximum precede the count store. */
        old_q = s->sum[0][1];
        max_i = s->max[0][0];
        s->sample_count = (uint8_t)(count + 1);
        old_i = s->sum[0][0];
    } else {
        /* Correlator 1 does not increment the shared sample count. */
        old_i = s->sum[1][0];
        old_q = s->sum[1][1];
        max_i = s->max[1][0];
    }
    s->sum[corr][0] = lb_add_wrapped(old_i, i);
    s->sum[corr][1] = lb_add_wrapped(old_q, q);
    if (max_i < i)
        s->max[corr][0] = i;
    if (s->max[corr][1] < q)
        s->max[corr][1] = q;
    if (s->min[corr][0] > i)
        s->min[corr][0] = i;
    if (s->min[corr][1] > q)
        s->min[corr][1] = q;
}

/* @provenance RVA 0x44EC loopback_corr0_done_isr */
void loopback_corr0_done_isr(void)
{
    uint32_t raw_i = CORR_RESULT_I(0);
    uint32_t raw_q = CORR_RESULT_Q(0);

    REG_CLEAR_BITS(CORR_TRIGGER, CORR_TRIGGER_CORR(0));
    REG_CLEAR_BITS(CORR_CAPTURE_START(0), CORR_CAPTURE_EN);
    REG_SET_BITS(DPD_IRQ, DPD_IRQ_CORR0_ACK);
    lb_accumulate(0, lb_sign_extend_result(raw_i), lb_sign_extend_result(raw_q));
}

/* @provenance RVA 0x4580 loopback_corr1_done_isr */
void loopback_corr1_done_isr(void)
{
    uint32_t raw_i = CORR_RESULT_I(1);
    uint32_t raw_q = CORR_RESULT_Q(1);

    REG_CLEAR_BITS(CORR_TRIGGER, CORR_TRIGGER_CORR(1));
    REG_CLEAR_BITS(CORR_CAPTURE_START(1), CORR_CAPTURE_EN);
    REG_SET_BITS(DPD_IRQ, DPD_IRQ_CORR1_ACK);
    lb_accumulate(1, lb_sign_extend_result(raw_i), lb_sign_extend_result(raw_q));
}

/* ------------------------------------------------------------------------- */
/* Engage / disengage                                                        */
/* ------------------------------------------------------------------------- */

/* Write mode and gain of one bank's loopback path from saved values. */
static void lb_restore_mode_gain(uint32_t bank, uint32_t mode, uint32_t gain)
{
    REG_WRITE_FIELD(RADIO_LB_CFG(bank), RADIO_LB_CFG_MODE, mode);
    REG_WRITE_FIELD(RADIO_LB_CFG(bank), RADIO_LB_CFG_GAIN, gain);
}

/* @provenance RVA 0x4610 loopback_restore_radio_state */
int loopback_restore_radio(void)
{
    REG_CLEAR_BITS(RADIO_LB_PATH(RADIO_BANK_B), RADIO_LB_PATH_EN);
    REG_CLEAR_BITS(RADIO_LB_PATH(RADIO_BANK_A), RADIO_LB_PATH_EN);
    REG_CLEAR_BITS(RADIO_LB_POWER(RADIO_BANK_B), RADIO_LB_POWER_EN);
    REG_CLEAR_BITS(RADIO_LB_POWER(RADIO_BANK_B), RADIO_LB_POWER_BIAS_EN);
    REG_CLEAR_BITS(RADIO_LB_POWER(RADIO_BANK_A), RADIO_LB_POWER_EN);
    REG_CLEAR_BITS(RADIO_LB_POWER(RADIO_BANK_A), RADIO_LB_POWER_BIAS_EN);

    lb_restore_mode_gain(RADIO_BANK_B, lb_saved.mode_b, lb_saved.gain_b);
    lb_restore_mode_gain(RADIO_BANK_A, lb_saved.mode_a, lb_saved.gain_a);
    REG_WRITE_FIELD(RADIO_LB_CFG(RADIO_BANK_B), RADIO_LB_CFG_ATTEN, lb_saved.atten_b);
    REG_WRITE_FIELD(RADIO_LB_CFG(RADIO_BANK_A), RADIO_LB_CFG_ATTEN, lb_saved.atten_a);
    return 0;
}

static void lb_reset_stats(void)
{
    memset((void *)lb_stats_base(), 0, sizeof lb.stats);
    volatile struct loopback_stats *stats = lb_stats_base();

    /* Original stores at 0x4802..0x4814: descending minima, then maxima. */
    stats->min[1][1] = INT32_MAX;
    stats->min[1][0] = INT32_MAX;
    stats->min[0][1] = INT32_MAX;
    stats->min[0][0] = INT32_MAX;
    stats->max[1][1] = INT32_MIN;
    stats->max[1][0] = INT32_MIN;
    stats->max[0][1] = INT32_MIN;
    stats->max[0][0] = INT32_MIN;
}

/* Program the loopback path's attenuation, mode and gain from the request. */
static uint8_t lb_control_byte(unsigned byte)
{
    return ((volatile uint8_t *)lb_control_base())[byte];
}

static void lb_program_path(uint8_t control, volatile uint32_t *initial_base)
{
    uint32_t value;
    uint8_t field;

    switch (FIELD_GET(LB_CTRL_MODE, control)) {
    case LB_MODE_FULL:
    case LB_MODE_ATTEN_ONLY:
        /* Each field is read after its radio register. The first byte uses
         * the pointer captured with the mode; later fields reload the cell. */
        value = RADIO_LB_CFG(RADIO_BANK_B);
        field = ((volatile uint8_t *)initial_base)[1];
        RADIO_LB_CFG(RADIO_BANK_B) = FIELD_REPLACE(value, RADIO_LB_CFG_ATTEN, field);
        value = RADIO_LB_CFG(RADIO_BANK_A);
        field = lb_control_byte(1);
        RADIO_LB_CFG(RADIO_BANK_A) = FIELD_REPLACE(value, RADIO_LB_CFG_ATTEN, field);
        if (FIELD_GET(LB_CTRL_MODE, control) != LB_MODE_FULL)
            break;
        value = RADIO_LB_CFG(RADIO_BANK_B);
        field = lb_control_byte(2);
        RADIO_LB_CFG(RADIO_BANK_B) = FIELD_REPLACE(value, RADIO_LB_CFG_MODE, field);
        value = RADIO_LB_CFG(RADIO_BANK_B);
        field = lb_control_byte(3);
        RADIO_LB_CFG(RADIO_BANK_B) = FIELD_REPLACE(value, RADIO_LB_CFG_GAIN, field);
        value = RADIO_LB_CFG(RADIO_BANK_A);
        field = lb_control_byte(2);
        RADIO_LB_CFG(RADIO_BANK_A) = FIELD_REPLACE(value, RADIO_LB_CFG_MODE, field);
        value = RADIO_LB_CFG(RADIO_BANK_A);
        field = lb_control_byte(3);
        RADIO_LB_CFG(RADIO_BANK_A) = FIELD_REPLACE(value, RADIO_LB_CFG_GAIN, field);
        break;
    default:
        break;
    }
}

/* Save the radio's loopback configuration and force mode 3 / gain 0. */
static void lb_save_and_arm_radio(void)
{
    lb_saved.mode_b = FIELD_GET(RADIO_LB_CFG_MODE, RADIO_LB_CFG(RADIO_BANK_B));
    lb_saved.gain_b = FIELD_GET(RADIO_LB_CFG_GAIN, RADIO_LB_CFG(RADIO_BANK_B));
    lb_saved.mode_a = FIELD_GET(RADIO_LB_CFG_MODE, RADIO_LB_CFG(RADIO_BANK_A));
    lb_saved.gain_a = FIELD_GET(RADIO_LB_CFG_GAIN, RADIO_LB_CFG(RADIO_BANK_A));
    lb_saved.atten_b = FIELD_GET(RADIO_LB_CFG_ATTEN, RADIO_LB_CFG(RADIO_BANK_B));
    lb_saved.atten_a = FIELD_GET(RADIO_LB_CFG_ATTEN, RADIO_LB_CFG(RADIO_BANK_A));

    REG_SET_BITS(RADIO_LB_CFG(RADIO_BANK_B), RFIELD_MASK(RADIO_LB_CFG_MODE));
    REG_CLEAR_BITS(RADIO_LB_CFG(RADIO_BANK_B), RFIELD_MASK(RADIO_LB_CFG_GAIN));
    REG_SET_BITS(RADIO_LB_CFG(RADIO_BANK_A), RFIELD_MASK(RADIO_LB_CFG_MODE));
    REG_CLEAR_BITS(RADIO_LB_CFG(RADIO_BANK_A), RFIELD_MASK(RADIO_LB_CFG_GAIN));
}

/* @provenance RVA 0x46A4 loopback_configure_mode */
btp_status_t loopback_configure(const uint32_t *request)
{
    uint32_t tone_reg;

    uint32_t control = ((const volatile uint32_t *)request)[0];
    uint32_t tone = ((const volatile uint32_t *)request)[1];
    volatile uint32_t *destination = lb_control_base();

    destination[1] = tone;
    destination[0] = control;
    lb_save_and_arm_radio();

    volatile uint32_t *path_control = lb_control_base();

    control = *(volatile uint8_t *)path_control;
    if (control == LB_MODE_OFF) {
        lb.active = 0;
        loopback_restore_radio();
        return BTP_OK;
    }

    lb_program_path((uint8_t)control, path_control);
    lb_reset_stats();

    REG_SET_BITS(RADIO_LB_PATH(RADIO_BANK_B), RADIO_LB_PATH_EN);
    REG_SET_BITS(RADIO_LB_PATH(RADIO_BANK_A), RADIO_LB_PATH_EN);
    REG_SET_BITS(RADIO_LB_POWER(RADIO_BANK_B), RADIO_LB_POWER_EN);
    REG_SET_BITS(RADIO_LB_POWER(RADIO_BANK_B), RADIO_LB_POWER_BIAS_EN);
    REG_SET_BITS(RADIO_LB_POWER(RADIO_BANK_A), RADIO_LB_POWER_EN);
    REG_SET_BITS(RADIO_LB_POWER(RADIO_BANK_A), RADIO_LB_POWER_BIAS_EN);
    lb.active = 1;

    /* Optional CW tone on bank B at an explicit frequency. */
    uint16_t tone_enable = *(volatile uint16_t *)(lb_control_base() + 1);

    tone_reg = RADIO_LB_TONE;
    if (tone_enable) {
        RADIO_LB_TONE = tone_reg & ~RFIELD_MASK(RADIO_LB_TONE_LEVEL);
        REG_SET_BITS(RADIO_LB_TONE, RADIO_LB_TONE_EN);
        uint32_t mhz = ((volatile uint16_t *)(lb_control_base() + 1))[1];

        if (mhz < LB_TONE_MHZ_MIN || mhz > LB_TONE_MHZ_MAX)
            return BTP_ERR_INVALID;
        REG_WRITE_FIELD(RADIO_SYNTH_FREQ(RADIO_BANK_B), RADIO_SYNTH_FREQ_MHZ, mhz);
        REG_SET_BITS(RADIO_SYNTH_FREQ_OVERRIDE, RADIO_SYNTH_FREQ_OVERRIDE_EN);
    } else {
        RADIO_LB_TONE = tone_reg & ~RADIO_LB_TONE_EN;
        REG_CLEAR_BITS(RADIO_SYNTH_FREQ_OVERRIDE, RADIO_SYNTH_FREQ_OVERRIDE_EN);
    }
    return BTP_OK;
}

/* ------------------------------------------------------------------------- */
/* Host interface                                                            */
/* ------------------------------------------------------------------------- */

/* @provenance RVA 0x48DC loopback_read_stats */
btp_status_t loopback_read_stats(void *dst, uint32_t *copied, uint32_t capacity)
{
    volatile struct loopback_stats *s;

    if (dst == NULL || copied == NULL || capacity < sizeof *s) {
        trace_log_error(TRACE_ERR_CLASS_COMMAND, 0, TRACE_ERR_LOOPBACK_STATS_ARGS);
        return BTP_ERR_INVALID;
    }
    if (*(volatile uint8_t *)&lb.active == 0) {
        trace_log_error(TRACE_ERR_CLASS_COMMAND, 0, TRACE_ERR_LOOPBACK_INACTIVE);
        return BTP_ERR_NOT_ACTIVE;
    }

    s = lb_stats_base();
    uint8_t count = s->sample_count;

    /* All four sums are sampled before any mean is stored (0x4910..0x4930). */
    if (count != 0) {
        int32_t i0 = s->sum[0][0];
        int32_t q0 = s->sum[0][1];
        int32_t i1 = s->sum[1][0];
        int32_t q1 = s->sum[1][1];

        s->sum[0][0] = i0 / count;
        s->sum[0][1] = q0 / count;
        s->sum[1][0] = i1 / count;
        s->sum[1][1] = q1 / count;
    }
    memcpy(dst, (const void *)s, sizeof *s);
    *copied = sizeof *s;
    return BTP_OK;
}

/* @provenance RVA 0x4958 loopback_is_active */
uint8_t loopback_is_active(void)
{
    return lb.active;
}

/* @provenance RVA 0x4964 loopback_init_noop */
int loopback_init(void)
{
    return 0;
}
