/*
 * rx.h -- receive path: per-packet setup, packet statistics, gain tables and
 * the coexistence gain back-off.
 */
#ifndef BTP_RX_H
#define BTP_RX_H

#include "btp.h"

/* Receiver bandwidth classes selected per packet type. */
enum rx_bandwidth {
    RX_BW_1P5       = 0,
    RX_BW_2         = 1,
    RX_BW_2P5       = 2,
    RX_BW_3         = 3,
    RX_BW_COUNT     = 4,
};

/* Host-controlled overrides (early .bss block). */
struct rx_overrides {
    uint32_t coex_backoff_active;
    uint8_t  filter_bw_enable;
    uint8_t  filter_bw;
    uint8_t  reserved_06[2];
    uint16_t rssi_cal_value;
    uint32_t rssi_cal_enable;
    uint32_t mxdc_i_enable;
    uint32_t mxdc_i;
    uint32_t mxdc_q_enable;
    uint32_t mxdc_q;
};
extern struct rx_overrides rx_ovr;

#define RX_MAXGAIN_ROWS             16
#define RX_MAXGAIN_LANES            4
#define RX_MAXGAIN_REF_LNA_DEFAULT  0x47
#define RX_MAXGAIN_REF_MIX_DEFAULT  0x3E

#define RX_CFO_MAX_INIT             (-0x200)
#define RX_CFO_MIN_INIT             0x200

struct rx_state {
    uint16_t iqmm_a[2];                 /* IQMM override parameters 0,1 */
    uint16_t iqmm_b[2];                 /*                           2,3 */
    uint8_t  iqmm_override;
    uint8_t  reserved_09[3];
    int32_t  cfo_sum;
    uint8_t  maxgain_ref_lna;
    uint8_t  maxgain_ref_mix;
    uint8_t  reserved_12[2];
    int32_t  cfo_max;
    int32_t  cfo_min;
    uint32_t packet_count;
    uint32_t rssi_histogram[4];         /* >= -50, [-70,-50), [-80,-70), < -80 dBm */
    /* register defaults captured at init and re-applied per packet */
    uint32_t default_agc_target;
    uint32_t default_sync_len;
    uint32_t default_cfg2_byte0;
    uint32_t default_cfg2_byte1;
    uint32_t default_timeout;
    uint32_t default_pkt_params[RX_PKT_PARAM_COUNT];
    uint32_t reserved_60;
    /* coexistence back-off: gains saved while the back-off is active */
    uint32_t coex_saved_mix;
    uint32_t coex_saved_lna;
    uint32_t coex_saved_limit;
    uint32_t reserved_70[2];
    uint8_t  maxgain[RX_MAXGAIN_ROWS][RX_MAXGAIN_LANES];
    uint32_t saved_mode0;
    uint32_t saved_mode1;
};
extern struct rx_state rx;

btp_status_t rx_set_rssi_cal_override(uint32_t enable, int16_t value);
btp_status_t rx_set_mxdc_i_override(uint32_t enable, uint32_t value);
btp_status_t rx_set_mxdc_q_override(uint32_t enable, uint32_t value);
btp_status_t rx_set_filter_bw_override(uint8_t enable, uint8_t bw);
btp_status_t rx_set_iqmm_override(uint8_t enable, unsigned param, int16_t value);
void         rx_update_maxgain_tables(void);
void         rx_init(void);
uint32_t     rx_setup_isr(void);
void         rx_done_isr(void);

void         coex_enable_rx_gain_backoff(void);
void         coex_disable_rx_gain_backoff(void);
uint32_t     coex_is_active(void);

#endif /* BTP_RX_H */
