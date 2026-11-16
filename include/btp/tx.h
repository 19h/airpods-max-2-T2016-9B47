/*
 * tx.h -- transmit path: per-packet setup, RF test modes and the host
 * overrides of the TX calibration.
 */
#ifndef BTP_TX_H
#define BTP_TX_H

#include "btp.h"

struct tx_overrides {
    uint32_t power_mode;                /* latched from SEQ_RF_CTRL at init */
    uint8_t  hf_coeff_bypass;
    uint8_t  reserved_05[3];
    uint8_t  power_mode_override;
    uint8_t  reserved_09;
    uint16_t df1_mantissa_value;
    uint32_t slope_cal_bypass;
    uint32_t df1_mantissa_override;
    uint8_t  tables_valid;              /* set once the MAC uploaded a TX power table */
    uint8_t  reserved_15[3];
};
extern struct tx_overrides tx_ovr;

struct tx_state {
    uint32_t packet_count;
    uint8_t  saved_pa_bias_stage1;
    uint8_t  saved_pa_bias_stage2;
    uint8_t  saved_pa_bias_mode;
    uint8_t  reserved_07;
};
extern struct tx_state tx;

/* .data: power-mode value forced by MAC_SUB_TXPWR_MODE */
extern uint32_t tx_power_mode_override_value;

btp_status_t tx_set_slope_cal_bypass(uint32_t bypass);
btp_status_t tx_set_df1_mantissa_override(uint32_t enable, int16_t mantissa);
btp_status_t tx_set_hf_coeff_bypass(uint8_t bypass);
btp_status_t tx_set_reserved_override_6(void);
btp_status_t tx_set_reserved_override_21(void);
btp_status_t tx_set_power_mode_override(uint32_t mode);
void         tx_init(bool warm_boot);
uint32_t     tx_setup_isr(void);
void         tx_done_isr(void);

/* RF test modes. */
btp_status_t rf_test_set_correlator_magic(uint32_t enable);
btp_status_t rf_test_cw_tone(unsigned enable, unsigned band, uint8_t channel, unsigned mode);
btp_status_t rf_test_cw_tone_apply(unsigned enable, uint8_t band, uint8_t channel, unsigned mode);
btp_status_t rf_test_prbs(uint32_t mode, uint8_t band, uint8_t channel);

#endif /* BTP_TX_H */
