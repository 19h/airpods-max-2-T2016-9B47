/*
 * nvv.h -- non-volatile calibration ("NVV") tags and the modem bring-up that
 * consumes them.
 *
 * Calibration arrives as tagged records inside "NV" containers: the RF
 * sequence container built into the image and the MDMI blob the host places at
 * NVV_MDMI_BASE.  The parser stores a pointer to each recognised payload and
 * records its presence in two bitmaps that are reported in the version block.
 */
#ifndef BTP_NVV_H
#define BTP_NVV_H

#include "btp.h"

enum nvv_tag {
    NVV_TAG_GLOBAL_PHY_INITS        = 0x080B,   /* {addr, value} register writes */
    NVV_TAG_RF_SEQ                  = 0x080E,
    NVV_TAG_GLOBAL_PHY_MSK_INITS    = 0x0821,   /* {addr, mask, value} */
    NVV_TAG_GLOBAL_RADIO_MSK_INITS  = 0x0836,   /* {addr, mask, value} */
    NVV_TAG_MOD_INFO                = 0x0857,   /* 4-character module id */
    NVV_TAG_A4P_CAL                 = 0x1006,
    NVV_TAG_A4P_CAL_SLOPES          = 0x115B,
    NVV_TAG_DYNAMIC_HF_COEFF        = 0x1168,
    NVV_TAG_DYNAMIC_MANT            = 0x1169,
    NVV_TAG_DYNAMIC_HF_DELAY_PTY    = 0x116A,
    NVV_TAG_DYNAMIC_HF_DELAY_PTE    = 0x116B,
    NVV_TAG_DYNAMIC_EXP             = 0x1170,
    NVV_TAG_DYNAMIC_IF_DELAY        = 0x1171,
    NVV_TAG_DYNAMIC_HF_DELAY        = 0x1172,
    NVV_TAG_DYNAMIC_AM_DELAY        = 0x1173,
    NVV_TAG_DPD_ACTUATION_TABLE     = 0x1176,
    NVV_TAG_KVCO_CAL                = 0x117A,
    NVV_TAG_TX_MAXPWR_CAL_V2        = 0x117C,
    NVV_TAG_DF1_MANT_CAL            = 0x117D,
    NVV_TAG_RX_MAXGAIN_CAL          = 0x117E,
    NVV_TAG_A0_RSSI_CAL             = 0x117F,
    NVV_TAG_DF1_CAL_V2              = 0x1180,
    NVV_TAG_MXDC_CAL                = 0x1182,
    NVV_TAG_KVCO_CAL_LP             = 0x118C,
    NVV_TAG_DF1_CAL_V3              = 0x118D,
    NVV_TAG_DF1_CAL_V4              = 0x1190,
    NVV_TAG_LB_DCOC_CAL_LP          = 0x1191,
    NVV_TAG_LB_DCOC_CAL_HP          = 0x1192,
    NVV_TAG_RXIQMM_AB_LNA1ST_BW1P5  = 0x1193,
    NVV_TAG_RXIQMM_AB_MIX1ST_BW1P5  = 0x1194,
    NVV_TAG_RXIQMM_AB_LNA1ST_BW2    = 0x1195,
    NVV_TAG_RXIQMM_AB_MIX1ST_BW2    = 0x1196,
    NVV_TAG_RXIQMM_AB_LNA1ST_BW3    = 0x1197,
    NVV_TAG_RXIQMM_AB_MIX1ST_BW3    = 0x1198,
    NVV_TAG_RXIQMM_AB_LNA1ST_BW2P5  = 0x1199,
    NVV_TAG_RXIQMM_AB_MIX1ST_BW2P5  = 0x119A,
    NVV_TAG_LBIQMM_AB_LP            = 0x119B,
    NVV_TAG_LBIQMM_AB_HP            = 0x119C,
    NVV_TAG_LBIQMM_DCOC_LP          = 0x119D,
    NVV_TAG_LBIQMM_DCOC_HP          = 0x119F,
    NVV_TAG_FRS_IQMM_AB_LNA1ST_BW3  = 0x11D8,
    NVV_TAG_FRS_IQMM_AB_MIX1ST_BW3  = 0x11D9,
};

/* nvv_presence.p0 */
enum nvv_presence0 {
    NVV_P0_RF_SEQ               = BIT(0),
    NVV_P0_GLOBAL_PHY_INITS     = BIT(1),
    NVV_P0_GLOBAL_PHY_MSK_INITS = BIT(2),
    NVV_P0_GLOBAL_RADIO_MSK_INITS = BIT(3),
    NVV_P0_DYNAMIC_HF_COEFF     = BIT(4),
    NVV_P0_DYNAMIC_HF_DELAY_PTY = BIT(5),
    NVV_P0_DYNAMIC_HF_DELAY_PTE = BIT(6),
    NVV_P0_DYNAMIC_MANT         = BIT(7),
    NVV_P0_DYNAMIC_EXP          = BIT(8),
    NVV_P0_DYNAMIC_IF_DELAY     = BIT(9),
    NVV_P0_DYNAMIC_HF_DELAY     = BIT(10),
    NVV_P0_DYNAMIC_AM_DELAY     = BIT(11),
    NVV_P0_DPD_ACTUATION_TABLE  = BIT(12),
    NVV_P0_TX_MAXPWR_CAL_V2     = BIT(13),
    NVV_P0_A4P_CAL              = BIT(14),
    NVV_P0_A4P_CAL_SLOPES       = BIT(15),
    NVV_P0_KVCO_CAL             = BIT(16),
    NVV_P0_DF1_MANT_CAL         = BIT(17),
    NVV_P0_MOD_INFO             = BIT(18),
    NVV_P0_MXDC_CAL             = BIT(19),
    NVV_P0_A0_RSSI_CAL          = BIT(20),
    NVV_P0_SYSCFG_TXPWR         = BIT(21),  /* SysCfg TLV 1 */
    NVV_P0_SYSCFG_RSSI_CAL      = BIT(22),  /* SysCfg TLV 2 */
    NVV_P0_KVCO_CAL_LP          = BIT(23),
    NVV_P0_DF1_CAL_V2           = BIT(24),
    NVV_P0_DF1_CAL_V3           = BIT(25),
    NVV_P0_DF1_CAL_V4           = BIT(26),
    NVV_P0_LB_DCOC_CAL_LP       = BIT(27),
    NVV_P0_LB_DCOC_CAL_HP       = BIT(28),
    NVV_P0_LBIQMM_DCOC_LP       = BIT(29),
    NVV_P0_LBIQMM_DCOC_HP       = BIT(30),
    NVV_P0_LBIQMM_AB_LP         = BIT(31),
};

/* nvv_presence.p1 */
enum nvv_presence1 {
    NVV_P1_LBIQMM_AB_HP             = BIT(0),
    NVV_P1_RXIQMM_AB_LNA1ST_BW1P5   = BIT(1),
    NVV_P1_RXIQMM_AB_LNA1ST_BW2     = BIT(2),
    NVV_P1_RXIQMM_AB_LNA1ST_BW2P5   = BIT(3),
    NVV_P1_RXIQMM_AB_LNA1ST_BW3     = BIT(4),
    NVV_P1_RXIQMM_AB_MIX1ST_BW1P5   = BIT(5),
    NVV_P1_RXIQMM_AB_MIX1ST_BW2     = BIT(6),
    NVV_P1_RXIQMM_AB_MIX1ST_BW2P5   = BIT(7),
    NVV_P1_RXIQMM_AB_MIX1ST_BW3     = BIT(8),
    NVV_P1_RX_MAXGAIN_CAL           = BIT(9),
    NVV_P1_FRS_IQMM_AB_LNA1ST_BW3   = BIT(10),
    NVV_P1_FRS_IQMM_AB_MIX1ST_BW3   = BIT(11),
};

struct nvv_presence {
    uint32_t p0;
    uint32_t p1;
};
extern struct nvv_presence nvv_presence;

/*
 * Pointers to the recognised calibration payloads.  Order and spacing match
 * the recovered image; `hf_coeff` is re-pointed at a module-specific delay
 * table after parsing (see modem_init()).
 */
struct nvv_tables {
    const uint8_t  *a4p_cal;                /* 0x1006: u16[128] */
    const uint8_t  *a4p_cal_slopes;         /* 0x115B: u8[band*2 + alt] */
    const uint8_t  *df1_cal_v2;             /* 0x1180 */
    const uint8_t  *df1_cal_v3;             /* 0x118D */
    const uint8_t  *df1_cal_v4;             /* 0x1190 */
    const uint8_t  *df1_mant_cal;           /* 0x117D */
    const uint8_t  *dynamic_am_delay;       /* 0x1173 */
    const uint8_t  *dynamic_exp;            /* 0x1170 */
    const uint8_t  *hf_coeff;               /* 0x1168, then module selection */
    const uint8_t  *dynamic_hf_delay_pte;   /* 0x116B */
    const uint8_t  *dynamic_hf_delay_pty;   /* 0x116A */
    const uint8_t  *dynamic_hf_delay;       /* 0x1172 */
    const uint8_t  *dynamic_if_delay;       /* 0x1171 */
    const uint8_t  *dynamic_mant;           /* 0x1169: u32[] */
    const uint8_t  *frs_iqmm_ab_lna1st_bw3; /* 0x11D8 */
    const uint8_t  *frs_iqmm_ab_mix1st_bw3; /* 0x11D9 */
    const uint8_t  *kvco_cal_lp;            /* 0x118C */
    const uint8_t  *kvco_cal;               /* 0x117A */
    const uint8_t  *lb_dcoc_cal_hp;         /* 0x1192 */
    const uint8_t  *lb_dcoc_cal_lp;         /* 0x1191 */
    const uint8_t  *lbiqmm_ab_hp;           /* 0x119C */
    const uint8_t  *lbiqmm_ab_lp;           /* 0x119B */
    const uint8_t  *lbiqmm_dcoc_hp;         /* 0x119F */
    const uint8_t  *lbiqmm_dcoc_lp;         /* 0x119D */
    const char     *mod_info;               /* 0x0857 */
    const uint8_t  *mxdc_cal;               /* 0x1182 */
    const uint8_t  *rf_seq;                 /* 0x080E */
    const uint8_t  *a0_rssi_cal;            /* 0x117F: u8[band][3] */
    const uint8_t  *rx_maxgain_cal;         /* 0x117E: u32[16] */
    const uint8_t  *rxiqmm_ab_lna1st_bw1p5; /* 0x1193 */
    const uint8_t  *rxiqmm_ab_lna1st_bw2;   /* 0x1195 */
    const uint8_t  *rxiqmm_ab_lna1st_bw2p5; /* 0x1199 */
    const uint8_t  *rxiqmm_ab_lna1st_bw3;   /* 0x1197 */
    const uint8_t  *rxiqmm_ab_mix1st_bw1p5; /* 0x1194 */
    const uint8_t  *rxiqmm_ab_mix1st_bw2;   /* 0x1196 */
    const uint8_t  *rxiqmm_ab_mix1st_bw2p5; /* 0x119A */
    const uint8_t  *rxiqmm_ab_mix1st_bw3;   /* 0x1198 */
    const uint8_t  *tx_maxpwr_cal;          /* 0x117C, also from SysCfg TLV 1 */
};
extern struct nvv_tables nvv;

/* Module identities carried by NVV_TAG_MOD_INFO. */
#define NVV_MODULE_DAUG             "DAUG"
#define NVV_MODULE_PLTY             "PLTY"
#define NVV_MODULE_PLTE             "PLTE"
#define NVV_MODULE_YNGR             "YNGR"
#define NVV_MODULE_ELDR             "ELDR"

/* Built-in RF sequence container and the radio-rev-2 fallbacks. */
extern uint8_t  nvv_rfseq_blob[0x234];
extern uint32_t nvv_rfseq_fallback[5];
extern uint8_t  nvv_dynamic_exp_rev2[8];
extern uint32_t nvv_dynamic_mant_rev2[8];

uint32_t    nvv_parse_blob(const uint8_t *blob, uint32_t size);
bool        nvv_validate_address(uint32_t address);
int         nvv_log_bad_address(uint32_t range, uint32_t address);
int         nvv_module_prefix4_matches(const char *module_id, const char *expected_prefix);

void        modem_init(void);
int         modem_restore_registers(void);
uint8_t     modem_set_register_unlock(uint8_t unlock);

/* PHY bring-up (main.c). */
void        btp_main(void);
int         btp_early_hw_init(void);
uint32_t    phy_hw_init(bool warm_boot);
int         phy_reset_tx_and_radio(void);
uint32_t    phy_set_synth_band(bool high_band, bool tx, unsigned band);

/* Previous source spellings; descriptive names above are canonical. */
#define nvv_module_id_equals nvv_module_prefix4_matches

#endif /* BTP_NVV_H */
