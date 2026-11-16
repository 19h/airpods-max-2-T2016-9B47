/*
 * mac.h -- MAC <-> PHY command interface.
 */
#ifndef BTP_MAC_H
#define BTP_MAC_H

#include "btp.h"

/* mac_request.command */
enum mac_command {
    MAC_CMD_GET_VERSION         = 0,
    MAC_CMD_SUBCOMMAND          = 1,
    MAC_CMD_TXPWR_TABLE         = 2,
    MAC_CMD_PING                = 3,
    MAC_CMD_WARM_BOOT_ACK       = 4,
    MAC_CMD_TXPWR_BAND_TABLE    = 5,
};

/* mac_request.u.sub.id */
enum mac_subcommand {
    MAC_SUB_SYSCFG              = 0x0A,
    MAC_SUB_LOOPBACK_MODE       = 0x0F,
    MAC_SUB_LOOPBACK_STATS      = 0x10,
    MAC_SUB_TXPWR_MODE          = 0x12,
    MAC_SUB_TXPWR_TEST_MODE     = 0x13,
    MAC_SUB_SLOPE_CAL_BYPASS    = 0x14,
    MAC_SUB_OVERRIDE            = 0x15,
    MAC_SUB_FRS_DUMP            = 0x16,
    MAC_SUB_DPD                 = 0x17,
    MAC_SUB_TELEMETRY           = 0x18,
    MAC_SUB_VERSION             = 0x19,
    MAC_SUB_CHANNEL_LIMIT       = 0x1A,
    MAC_SUB_CW_TONE             = 0x1B,
    MAC_SUB_DIAG_BUFFER         = 0x1C,
    MAC_SUB_DEBUG               = 0x1D,
    MAC_SUB_PRBS                = 0x20,
    MAC_SUB_CORR_MAGIC          = 0x21,
};

/* mac_request.u.sub.p.ovr.index for MAC_SUB_OVERRIDE */
enum mac_override {
    MAC_OVR_DF1_MANTISSA        = 0x01,
    MAC_OVR_RSSI_CAL            = 0x02,
    MAC_OVR_HF_COEFF_BYPASS     = 0x03,
    MAC_OVR_MXDC_I              = 0x04,
    MAC_OVR_MXDC_Q              = 0x05,
    MAC_OVR_RESERVED_6          = 0x06,
    MAC_OVR_IQ_SCALE            = 0x07,
    MAC_OVR_TXPWR_OFFSET        = 0x08,
    MAC_OVR_TXPWR_BAND_FIRST    = 0x09,     /* ..0x10: one per band */
    MAC_OVR_TXPWR_BAND_LAST     = 0x10,
    MAC_OVR_IQMM_FIRST          = 0x11,     /* ..0x14: one per IQMM parameter */
    MAC_OVR_IQMM_LAST           = 0x14,
    MAC_OVR_RESERVED_21         = 0x15,
    MAC_OVR_PA_GAIN             = 0x16,
    MAC_OVR_RX_FILTER_BW        = 0x17,
    MAC_OVR_MAXGAIN_REF_LNA     = 0x18,
    MAC_OVR_MAXGAIN_REF_MIX     = 0x19,
    MAC_OVR_COEX_POWER_FLOOR    = 0x1A,
};

/* MAC_SUB_DEBUG operations */
enum mac_debug_op {
    MAC_DEBUG_ASSERT            = 0,
    MAC_DEBUG_HANG              = 2,
};

struct mac_state {
    uint8_t warm_boot_ack_pending;
    uint8_t reserved[3];
};
extern struct mac_state mac;

void mac_isr(void);
void mac_send_warm_boot_ack(void);
void mac_send_crash_notification(const struct crash_descriptor *desc);

#endif /* BTP_MAC_H */
