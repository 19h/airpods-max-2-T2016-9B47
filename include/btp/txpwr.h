/*
 * txpwr.h -- transmit power control.
 *
 * Power is set per packet from a target in half-dB steps.  The target comes
 * from the MAC's per-band power table, is clamped by per-channel edge limits
 * and the coexistence floor, and is then looked up in the interpolated
 * maximum-power calibration to pick a PA stage and a gain code.
 */
#ifndef BTP_TXPWR_H
#define BTP_TXPWR_H

#include "btp.h"

#define TXPWR_PKT_TYPES             9      /* power-table columns per band */
#define TXPWR_CHANNELS              128    /* interpolated table rows per band */
#define TXPWR_PA_STAGES             3
#define TXPWR_PA_CODES              121    /* entries per PA stage table */
#define TXPWR_CODE_INDEX_MAX        0x78
#define TXPWR_BAND_OVERRIDE_OFF     0x8000
#define TXPWR_IQ_SCALE_DEFAULT      0x1E00
#define TXPWR_COEX_FLOOR_DEFAULT    (-120)
#define TXPWR_CAL_OFFSET_DEFAULT    2
#define TXPWR_DEFAULT_TARGET        0x50

/* Operating modes (txpwr_mode.mode). */
enum txpwr_mode_id {
    TXPWR_MODE_CALIBRATED   = 0,
    TXPWR_MODE_TIMED_STAGE  = 1,
    TXPWR_MODE_FIXED_STAGE  = 2,
    TXPWR_MODE_FIXED_CODES  = 3,
    TXPWR_MODE_TARGET_OVERRIDE = 4, /* calibrated path, target from override_value */
    TXPWR_MODE_INDEX_OVERRIDE  = 5, /* direct path, code index from override_value */
    /* Previous C-model names; the reference does not halt in these modes. */
    TXPWR_MODE_HALT_4 = TXPWR_MODE_TARGET_OVERRIDE,
    TXPWR_MODE_HALT_5 = TXPWR_MODE_INDEX_OVERRIDE,
};

/* Per-channel edge limits: cap the target above/below a channel index. */
struct txpwr_channel_limit {
    uint8_t  low_channel;       /* 0xFF = unused */
    int8_t   low_power;
    uint8_t  high_channel;      /* 0xFF = unused */
    int8_t   high_power;
};

enum txpwr_limit_op {
    TXPWR_LIMIT_CLEAR   = 0,
    TXPWR_LIMIT_HIGH    = 1,
    TXPWR_LIMIT_LOW     = 2,
};

/* MAC power table entry (one per band and packet type). */
struct txpwr_entry {
    int8_t   power;             /* base target, half dB */
    uint8_t  step;              /* per power-step reduction */
    int8_t   alt_power;         /* used when SEQ_PKT_CMD1_PWR_ALT is set */
    uint8_t  alt_step;
};

/* PA stage code tables: gain code (SEQ_RF_CTRL_PA_GAIN) + sequencer code. */
struct txpwr_pa_code {
    uint8_t gain;
    uint8_t code;
};

/* Test-mode parameters (MAC_SUB_TXPWR_TEST_MODE), early .bss block. */
struct txpwr_mode {
    uint32_t mode;              /* enum txpwr_mode_id */
    uint32_t stage;
    uint32_t post_stage;        /* stage selected after the packet (TIMED_STAGE) */
    uint32_t seq_code;
    uint32_t gain_code;
    uint8_t  delay_ticks;
    uint8_t  reserved_15[3];
    /* ROM 0x3E76/0x3F64: target (mode 4) or code index (mode 5).
     * The C consumer still has the documented halt-path fidelity gap. */
    union {
        uint32_t override_value;
        uint32_t reserved_18;   /* previous source spelling */
    };
    uint32_t iq_scale_override;
    uint8_t  pa_gain_override;
    uint8_t  reserved_21[3];
};
extern struct txpwr_mode txpwr_mode;

struct txpwr_state {
    struct txpwr_channel_limit limits[BTP_BAND_COUNT][TXPWR_PKT_TYPES];  /* +0x000 */
    int32_t  global_offset;                                               /* +0x120 */
    uint32_t band_override_enable[BTP_BAND_COUNT];                        /* +0x124 */
    uint32_t band_override_value[BTP_BAND_COUNT];                         /* +0x144 */
    uint8_t  reserved_164[0x9C];
    uint8_t  maxpwr[BTP_BAND_COUNT][TXPWR_CHANNELS][4];                  /* +0x200 */
    const struct txpwr_pa_code *pa_tables[TXPWR_PA_STAGES];               /* +0x1200 */
    uint8_t  reserved_120c[0xF4];
    struct txpwr_entry table[BTP_BAND_COUNT][TXPWR_PKT_TYPES];           /* +0x1300 */
    uint32_t cal_offset;                                                  /* +0x1420 */
};
extern struct txpwr_state txpwr;
extern volatile uint32_t txpwr_workspace_address;

/* .data configuration cells */
extern uint32_t  txpwr_iq_scale;
extern int32_t   txpwr_coex_floor;
extern uint8_t   txpwr_debug_flags;
#define TXPWR_DEBUG_VERBOSE_TABLES  BIT(1)

btp_status_t txpwr_set_pa_gain_override(uint32_t enable, uint8_t gain);
int          txpwr_reset_band_overrides(void);
int          txpwr_reset_channel_limits(void);
uint32_t     txpwr_get_channel_limit_word(unsigned band, unsigned pkt_type);
btp_status_t txpwr_set_channel_limit(unsigned op, unsigned band, unsigned pkt_type,
                                     uint8_t channel, int8_t power);
btp_status_t txpwr_update_table(const void *table);
btp_status_t txpwr_update_band_table(const uint32_t *table, unsigned band);
btp_status_t txpwr_set_global_offset(uint32_t enable, int32_t offset);
btp_status_t txpwr_set_band_override(unsigned band, uint32_t enable, uint32_t value);
btp_status_t txpwr_set_iq_scale_override(uint32_t enable, uint32_t scale);
int          txpwr_set_coex_floor(int32_t floor);
uint32_t     txpwr_init(void);
void         txpwr_apply_calibrated(int target, unsigned band, int channel, unsigned pkt_type);
uint32_t     txpwr_apply_stage(unsigned stage, unsigned code_index, unsigned band);
uint32_t     txpwr_apply_for_packet(unsigned band, int channel, int target, unsigned pkt_type);
int          txpwr_packet_done(void);
btp_status_t txpwr_set_test_mode(uint32_t mode, uint32_t stage, uint32_t post_stage,
                                 uint8_t delay, uint32_t seq_code, uint32_t gain_code,
                                 int override_value);
void         txpwr_setup_from_command(void);

/* Previous source spellings; descriptive names above are canonical. */
#define txpwr_get_channel_limit txpwr_get_channel_limit_word

#endif /* BTP_TXPWR_H */
