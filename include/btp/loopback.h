/*
 * loopback.h -- TX->RX loopback measurement using the two correlators.
 */
#ifndef BTP_LOOPBACK_H
#define BTP_LOOPBACK_H

#include "btp.h"

#pragma pack(push, 1)
struct loopback_stats {
    uint8_t  sample_count;
    uint8_t  reserved[3];
    int32_t  sum[2][2];         /* [correlator][I,Q] */
    int32_t  max[2][2];
    int32_t  min[2][2];
};
#pragma pack(pop)

/* loopback_state.control (request word 0) */
#define LB_CTRL_MODE                RFIELD(0, 4)
#define LB_MODE_OFF                 0
#define LB_MODE_ATTEN_ONLY          2
#define LB_MODE_FULL                3
#define LB_CTRL_ATTEN               RFIELD(8, 5)
#define LB_CTRL_PATH_MODE           RFIELD(16, 2)
#define LB_CTRL_GAIN                RFIELD(24, 4)
#define LB_CTRL_SKIP_IQMM_AB        BIT(5)
#define LB_CTRL_SKIP_IQMM_DCOC      BIT(6)
#define LB_CTRL_SKIP_DCOC           BIT(7)

/* loopback_state.tone (request word 1): enable in the low half, MHz above. */
#define LB_TONE_ENABLE_MASK         0x0000FFFFu
#define LB_TONE_MHZ_SHIFT           16
#define LB_TONE_MHZ_MIN             2350
#define LB_TONE_MHZ_MAX             2530

struct loopback_state {
    struct loopback_stats stats;
    uint32_t control;
    uint32_t tone;
    uint8_t  active;
    uint8_t  reserved[3];
};
extern struct loopback_state lb;
extern volatile uint32_t loopback_stats_address;
extern volatile uint32_t loopback_control_address;

/* Radio fields saved while loopback is engaged. */
struct loopback_saved_radio {
    uint32_t mode_b;
    uint32_t gain_b;
    uint32_t mode_a;
    uint32_t gain_a;
    uint32_t atten_b;
    uint32_t atten_a;
};
extern struct loopback_saved_radio lb_saved;

int      loopback_apply_dcoc_cal(unsigned index, bool high_power);
uint32_t loopback_apply_iqmm_dcoc_cal(unsigned index, bool high_power);
uint32_t loopback_apply_iqmm_ab_cal(unsigned index, bool high_power);
void     loopback_setup_tx_measurement(unsigned band, bool high_power);
void     loopback_configure_corr_window(unsigned corr, uint32_t start, uint32_t end);
void     loopback_corr0_done_isr(void);
void     loopback_corr1_done_isr(void);
int      loopback_restore_radio(void);
btp_status_t loopback_configure(const uint32_t *request);
btp_status_t loopback_read_stats(void *dst, uint32_t *copied, uint32_t capacity);
uint8_t  loopback_is_active(void);
int      loopback_init(void);

#endif /* BTP_LOOPBACK_H */
