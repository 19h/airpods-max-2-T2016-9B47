/*
 * nvv.c -- non-volatile calibration ("NVV") parsing and modem bring-up.
 *
 * Calibration data is delivered in "NV" containers. Decoding examines a
 * little-endian 16-bit prefix, including for a one-byte header:
 *
 * prefix test       header bytes   tag              payload length
 * bit 15 clear      1              low 8 bits       1
 * bits 15:14 = 10   2              low 11 bits      bits 13:11
 * bits 15:14 = 11   4              low 14 bits      next little-endian u16
 *
 * The parser stores recognized payload pointers and presence bits. Register
 * initialization records are applied immediately and can also write RAM.
 */
#include "btp/btp.h"
#include "btp/nvv.h"
#include "btp/trace.h"
#include "btp/status.h"
#include "btp/dpd.h"

struct nvv_presence nvv_presence BTP_STATE(nvv_presence);
struct nvv_tables   nvv          BTP_STATE(nvv_tables);

/* ------------------------------------------------------------------------- */
/* Built-in containers and fallbacks (.data)                                 */
/* ------------------------------------------------------------------------- */

/* RF sequence container compiled into the image: header + one NVV_TAG_RF_SEQ
 * record of 0x228 bytes. */
uint8_t nvv_rfseq_blob[0x234] BTP_DATA(nvv_rfseq_blob) = {
    0x2C, 0x02, 0x56, 0x4E, 0x00, 0x00, 0xCF, 0xEC, 0x0E, 0xC8, 0x28, 0x02,
    0x05, 0x00, 0x00, 0xFE, 0x05, 0x00, 0x00, 0xFE, 0x0E, 0x00, 0x00, 0xFE,
    0x36, 0x00, 0x00, 0xFE, 0x3A, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0xFB,
    0x02, 0x00, 0x20, 0xF6, 0x00, 0x00, 0x00, 0xFB, 0x20, 0xA1, 0x07, 0xFF,
    0xC1, 0x52, 0x45, 0xF3, 0x00, 0x00, 0x00, 0xF1, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xF2, 0x01, 0x00, 0x00, 0x00, 0xC1, 0x52, 0x45, 0xF0,
    0x00, 0x00, 0x00, 0xFB, 0x4E, 0x40, 0x45, 0xF3, 0x00, 0x00, 0x00, 0xF1,
    0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF2, 0x00, 0x01, 0x00, 0x00,
    0x4E, 0x40, 0x45, 0xF0, 0x4F, 0x40, 0x45, 0xF3, 0x00, 0x00, 0x00, 0xF1,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF2, 0xFE, 0x00, 0x00, 0x00,
    0x4F, 0x40, 0x45, 0xF0, 0x47, 0x00, 0x40, 0xF3, 0x00, 0x00, 0x00, 0xF1,
    0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0xF2, 0x00, 0x00, 0x00, 0x10,
    0x47, 0x00, 0x40, 0xF0, 0x47, 0x00, 0x40, 0xF3, 0x00, 0x00, 0x00, 0xF1,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0xF2, 0x00, 0x00, 0x00, 0x20,
    0x47, 0x00, 0x40, 0xF0, 0x9B, 0x00, 0x40, 0xF3, 0x00, 0x00, 0x00, 0xF1,
    0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0xF2, 0x00, 0x00, 0x00, 0x10,
    0x9B, 0x00, 0x40, 0xF0, 0x9B, 0x00, 0x40, 0xF3, 0x00, 0x00, 0x00, 0xF1,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0xF2, 0x00, 0x00, 0x00, 0x20,
    0x9B, 0x00, 0x40, 0xF0, 0x44, 0x00, 0x00, 0xE0, 0xA0, 0x05, 0x00, 0xFF,
    0x4E, 0x40, 0x45, 0xF8, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFB,
    0x47, 0x00, 0x40, 0xF8, 0x00, 0x00, 0x00, 0x20, 0x9B, 0x00, 0x40, 0xF8,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0xFB, 0x1E, 0x00, 0x40, 0xF6,
    0x2B, 0x00, 0x20, 0xF6, 0x02, 0x00, 0x80, 0xF6, 0x00, 0x00, 0x00, 0xFB,
    0x01, 0x4C, 0x45, 0xF3, 0x00, 0x00, 0x00, 0xF1, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xF2, 0x02, 0x00, 0x00, 0x00, 0x01, 0x4C, 0x45, 0xF0,
    0x90, 0x01, 0x00, 0xFF, 0x6F, 0x00, 0x40, 0xF3, 0x00, 0x00, 0x00, 0xF1,
    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF2, 0x10, 0x00, 0x00, 0x00,
    0x6F, 0x00, 0x40, 0xF0, 0x6C, 0x00, 0x40, 0xF3, 0x00, 0x00, 0x00, 0xF1,
    0x00, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF2, 0x00, 0x06, 0x00, 0x00,
    0x6C, 0x00, 0x40, 0xF0, 0x6C, 0x00, 0x40, 0xF3, 0x00, 0x00, 0x00, 0xF1,
    0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF2, 0x00, 0x40, 0x00, 0x00,
    0x6C, 0x00, 0x40, 0xF0, 0x00, 0x00, 0x00, 0xFB, 0x00, 0x68, 0xC0, 0xF3,
    0x00, 0x00, 0x00, 0xF1, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF2,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x68, 0xC0, 0xF0, 0xC0, 0x12, 0x00, 0xFF,
    0x00, 0x50, 0x45, 0xF3, 0x00, 0x00, 0x00, 0xF1, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xF2, 0x01, 0x00, 0x00, 0x00, 0x00, 0x50, 0x45, 0xF0,
    0x00, 0x00, 0x00, 0xFB, 0x01, 0x68, 0x45, 0xF3, 0x00, 0x00, 0x00, 0xF1,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF2, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x68, 0x45, 0xF0, 0x20, 0x03, 0x00, 0xFF, 0x01, 0x50, 0x45, 0xF3,
    0x00, 0x00, 0x00, 0xF1, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xF2,
    0x00, 0x00, 0x00, 0x80, 0x01, 0x50, 0x45, 0xF0, 0x00, 0x00, 0x00, 0xFB,
    0x08, 0x00, 0x00, 0xFE, 0x0A, 0x00, 0x00, 0xFE, 0x0A, 0x00, 0x00, 0xFE,
    0x0A, 0x00, 0x00, 0xFE, 0x0A, 0x00, 0x00, 0xFE, 0x0A, 0x00, 0x00, 0xFE,
    0x0A, 0x00, 0x00, 0xFE, 0x0A, 0x00, 0x00, 0xFE, 0x9B, 0x00, 0x40, 0xF8,
    0x00, 0x00, 0x00, 0x20, 0xB3, 0xFF, 0x00, 0xFE, 0xB2, 0xFF, 0x00, 0xFE,
    0xB1, 0xFF, 0x00, 0xFE, 0xB0, 0xFF, 0x00, 0xFE, 0xAF, 0xFF, 0x00, 0xFE,
    0xAE, 0xFF, 0x00, 0xFE, 0xAD, 0xFF, 0x00, 0xFE, 0xAC, 0xFF, 0x00, 0xFE,
};

/* Sequence the RF engine runs until an rf_seq record has been loaded. */
uint32_t nvv_rfseq_fallback[5] BTP_DATA(nvv_rfseq_fallback) = {
    0xFB000000, 0xFB000000, 0xFB000000, 0xFB000000, 0xFB000000,
};

/* Radio revision 2 does not carry dynamic_exp / dynamic_mant records. */
uint8_t nvv_dynamic_exp_rev2[8] BTP_DATA(nvv_dynamic_exp_rev2) = { 4, 3, 3, 3, 5, 4, 4, 3 };

uint32_t nvv_dynamic_mant_rev2[8] BTP_DATA(nvv_dynamic_mant_rev2) = {
    0x0000F3CF, 0x00008925, 0x00008C71, 0x0000999A,
    0x0000F3CF, 0x00009862, 0x0000E38E, 0x00008E39,
};

/* ------------------------------------------------------------------------- */
/* Register-initialisation records                                           */
/* ------------------------------------------------------------------------- */

struct nvv_reg_write {              /* NVV_TAG_GLOBAL_PHY_INITS */
    uint32_t address;
    uint32_t value;
};

struct nvv_reg_masked_write {       /* NVV_TAG_GLOBAL_*_MSK_INITS */
    uint32_t address;
    uint32_t mask;
    uint32_t value;
};

/* These bit-mask exclusions also allow RAM, including initialized pointer cells. */
#define NVV_FORBIDDEN_MASK0        0x42400000u
#define NVV_FORBIDDEN_MASK1        0x44440000u

/* @provenance RVA 0x4EC6 nvv_log_bad_address */
int nvv_log_bad_address(uint32_t range, uint32_t address)
{
    dbg_printf("## BAD ADDR %08x!!\n", address);
    return 0;
}

/* @provenance RVA 0x4EA8 nvv_validate_mmio_address */
bool nvv_validate_address(uint32_t address)
{
    if ((address & NVV_FORBIDDEN_MASK0) == NVV_FORBIDDEN_MASK0)
        return nvv_log_bad_address(NVV_FORBIDDEN_MASK0, address);
    if ((address & NVV_FORBIDDEN_MASK1) == NVV_FORBIDDEN_MASK1)
        return nvv_log_bad_address(NVV_FORBIDDEN_MASK1, address);
    return true;
}

/* @provenance RVA 0x5868 modem_set_register_write_unlock */
uint8_t modem_set_register_unlock(uint8_t unlock)
{
    uint32_t bit = unlock & 1u;

    REG_WRITE_MASKED(SEQ_REG_UNLOCK,
                     SEQ_REG_UNLOCK_RADIO | SEQ_REG_UNLOCK_RADIO_SEQ, bit << 4);
    REG_WRITE_MASKED(SEQ_REG_UNLOCK, SEQ_REG_UNLOCK_MODEM, bit << 6);
    REG_WRITE_MASKED(TXDIG_CTRL, TXDIG_CTRL_REG_UNLOCK, bit << 5);
    REG_WRITE_MASKED(TXDIG_CTRL, TXDIG_CTRL_REG_UNLOCK_SEQ, bit << 16);
    return unlock;
}

/* Returns the number of bytes consumed from the record (rounded to entries). */
static uint32_t nvv_apply_reg_writes(const uint8_t *payload, uint32_t length)
{
    uint32_t offset;

    modem_set_register_unlock(true);
    for (offset = 0; offset < length; offset += sizeof(struct nvv_reg_write)) {
        const struct nvv_reg_write *w = (const void *)(payload + offset);

        if (nvv_validate_address(w->address))
            MMIO32(w->address) = w->value;
    }
    modem_set_register_unlock(false);
    return offset;
}

static uint32_t nvv_apply_reg_masked_writes(const uint8_t *payload, uint32_t length)
{
    uint32_t offset;

    modem_set_register_unlock(true);
    for (offset = 0; offset < length; offset += sizeof(struct nvv_reg_masked_write)) {
        const struct nvv_reg_masked_write *w = (const void *)(payload + offset);

        if (nvv_validate_address(w->address))
            MMIO32(w->address) = (MMIO32(w->address) & ~w->mask) | (w->value & w->mask);
    }
    modem_set_register_unlock(false);
    return offset;
}

/* ------------------------------------------------------------------------- */
/* Tag catalogue                                                             */
/* ------------------------------------------------------------------------- */

struct nvv_tag_desc {
    uint16_t    tag;
    uint8_t     presence_word;      /* 0 or 1 */
    uint32_t    presence_bit;
    uint16_t    slot;               /* offsetof(struct nvv_tables, ...) */
    const char *name;
};

#define NVV_SLOT(field)     ((uint16_t)offsetof(struct nvv_tables, field))

static const struct nvv_tag_desc nvv_tag_catalogue[] = {
    { NVV_TAG_RF_SEQ,                 0, NVV_P0_RF_SEQ,                NVV_SLOT(rf_seq),                 "rf_seq" },
    { NVV_TAG_MOD_INFO,               0, NVV_P0_MOD_INFO,              NVV_SLOT(mod_info),               "mod_info" },
    { NVV_TAG_A4P_CAL,                0, NVV_P0_A4P_CAL,               NVV_SLOT(a4p_cal),                "a4p_cal" },
    { NVV_TAG_A4P_CAL_SLOPES,         0, NVV_P0_A4P_CAL_SLOPES,        NVV_SLOT(a4p_cal_slopes),         "a4p_cal_slopes" },
    { NVV_TAG_DYNAMIC_HF_COEFF,       0, NVV_P0_DYNAMIC_HF_COEFF,      NVV_SLOT(hf_coeff),               "dynamic_hf_coeff" },
    { NVV_TAG_DYNAMIC_MANT,           0, NVV_P0_DYNAMIC_MANT,          NVV_SLOT(dynamic_mant),           "dynamic_mant" },
    { NVV_TAG_DYNAMIC_HF_DELAY_PTY,   0, NVV_P0_DYNAMIC_HF_DELAY_PTY,  NVV_SLOT(dynamic_hf_delay_pty),   "dynamic_hf_delay_pty" },
    { NVV_TAG_DYNAMIC_HF_DELAY_PTE,   0, NVV_P0_DYNAMIC_HF_DELAY_PTE,  NVV_SLOT(dynamic_hf_delay_pte),   "dynamic_hf_delay_pte" },
    { NVV_TAG_DYNAMIC_EXP,            0, NVV_P0_DYNAMIC_EXP,           NVV_SLOT(dynamic_exp),            "dynamic_exp" },
    { NVV_TAG_DYNAMIC_IF_DELAY,       0, NVV_P0_DYNAMIC_IF_DELAY,      NVV_SLOT(dynamic_if_delay),       "dynamic_if_delay" },
    { NVV_TAG_DYNAMIC_HF_DELAY,       0, NVV_P0_DYNAMIC_HF_DELAY,      NVV_SLOT(dynamic_hf_delay),       "dynamic_hf_delay" },
    { NVV_TAG_DYNAMIC_AM_DELAY,       0, NVV_P0_DYNAMIC_AM_DELAY,      NVV_SLOT(dynamic_am_delay),       "dynamic_am_delay" },
    { NVV_TAG_KVCO_CAL,               0, NVV_P0_KVCO_CAL,              NVV_SLOT(kvco_cal),               "kvco_cal" },
    { NVV_TAG_TX_MAXPWR_CAL_V2,       0, NVV_P0_TX_MAXPWR_CAL_V2,      NVV_SLOT(tx_maxpwr_cal),          "tx_maxpwr_cal_v2" },
    { NVV_TAG_DF1_MANT_CAL,           0, NVV_P0_DF1_MANT_CAL,          NVV_SLOT(df1_mant_cal),           "df1_mant_cal" },
    { NVV_TAG_RX_MAXGAIN_CAL,         1, NVV_P1_RX_MAXGAIN_CAL,        NVV_SLOT(rx_maxgain_cal),         "rx_maxgain_cal" },
    { NVV_TAG_A0_RSSI_CAL,            0, NVV_P0_A0_RSSI_CAL,           NVV_SLOT(a0_rssi_cal),            NULL },
    { NVV_TAG_DF1_CAL_V2,             0, NVV_P0_DF1_CAL_V2,            NVV_SLOT(df1_cal_v2),             "df1_cal_v2" },
    { NVV_TAG_MXDC_CAL,               0, NVV_P0_MXDC_CAL,              NVV_SLOT(mxdc_cal),               NULL },
    { NVV_TAG_KVCO_CAL_LP,            0, NVV_P0_KVCO_CAL_LP,           NVV_SLOT(kvco_cal_lp),            "kvco_cal_lp" },
    { NVV_TAG_DF1_CAL_V3,             0, NVV_P0_DF1_CAL_V3,            NVV_SLOT(df1_cal_v3),             "df1_cal_v3" },
    { NVV_TAG_DF1_CAL_V4,             0, NVV_P0_DF1_CAL_V4,            NVV_SLOT(df1_cal_v4),             "df1_cal_v4" },
    { NVV_TAG_LB_DCOC_CAL_LP,         0, NVV_P0_LB_DCOC_CAL_LP,        NVV_SLOT(lb_dcoc_cal_lp),         "lb_dcoc_cal_lp" },
    { NVV_TAG_LB_DCOC_CAL_HP,         0, NVV_P0_LB_DCOC_CAL_HP,        NVV_SLOT(lb_dcoc_cal_hp),         "lb_dcoc_cal_hp" },
    { NVV_TAG_RXIQMM_AB_LNA1ST_BW1P5, 1, NVV_P1_RXIQMM_AB_LNA1ST_BW1P5, NVV_SLOT(rxiqmm_ab_lna1st_bw1p5), "rxiqmm_ab_lna1st_bw1p5" },
    { NVV_TAG_RXIQMM_AB_MIX1ST_BW1P5, 1, NVV_P1_RXIQMM_AB_MIX1ST_BW1P5, NVV_SLOT(rxiqmm_ab_mix1st_bw1p5), "rxiqmm_ab_mix1st_bw1p5" },
    { NVV_TAG_RXIQMM_AB_LNA1ST_BW2,   1, NVV_P1_RXIQMM_AB_LNA1ST_BW2,  NVV_SLOT(rxiqmm_ab_lna1st_bw2),   "rxiqmm_ab_lna1st_bw2" },
    { NVV_TAG_RXIQMM_AB_MIX1ST_BW2,   1, NVV_P1_RXIQMM_AB_MIX1ST_BW2,  NVV_SLOT(rxiqmm_ab_mix1st_bw2),   "rxiqmm_ab_mix1st_bw2" },
    { NVV_TAG_RXIQMM_AB_LNA1ST_BW3,   1, NVV_P1_RXIQMM_AB_LNA1ST_BW3,  NVV_SLOT(rxiqmm_ab_lna1st_bw3),   "rxiqmm_ab_lna1st_bw3" },
    { NVV_TAG_RXIQMM_AB_MIX1ST_BW3,   1, NVV_P1_RXIQMM_AB_MIX1ST_BW3,  NVV_SLOT(rxiqmm_ab_mix1st_bw3),   "rxiqmm_ab_mix1st_bw3" },
    { NVV_TAG_RXIQMM_AB_LNA1ST_BW2P5, 1, NVV_P1_RXIQMM_AB_LNA1ST_BW2P5, NVV_SLOT(rxiqmm_ab_lna1st_bw2p5), "rxiqmm_ab_lna1st_bw2p5" },
    { NVV_TAG_RXIQMM_AB_MIX1ST_BW2P5, 1, NVV_P1_RXIQMM_AB_MIX1ST_BW2P5, NVV_SLOT(rxiqmm_ab_mix1st_bw2p5), "rxiqmm_ab_mix1st_bw2p5" },
    { NVV_TAG_LBIQMM_AB_LP,           0, NVV_P0_LBIQMM_AB_LP,          NVV_SLOT(lbiqmm_ab_lp),           "lbiqmm_ab_lp" },
    { NVV_TAG_LBIQMM_AB_HP,           1, NVV_P1_LBIQMM_AB_HP,          NVV_SLOT(lbiqmm_ab_hp),           "lbiqmm_ab_hp" },
    { NVV_TAG_LBIQMM_DCOC_LP,         0, NVV_P0_LBIQMM_DCOC_LP,        NVV_SLOT(lbiqmm_dcoc_lp),         "lbiqmm_dcoc_lp" },
    { NVV_TAG_LBIQMM_DCOC_HP,         0, NVV_P0_LBIQMM_DCOC_HP,        NVV_SLOT(lbiqmm_dcoc_hp),         "lbiqmm_dcoc_hp" },
    { NVV_TAG_FRS_IQMM_AB_LNA1ST_BW3, 1, NVV_P1_FRS_IQMM_AB_LNA1ST_BW3, NVV_SLOT(frs_iqmm_ab_lna1st_bw3), "frs_iqmm_ab_lna1st_bw3" },
    { NVV_TAG_FRS_IQMM_AB_MIX1ST_BW3, 1, NVV_P1_FRS_IQMM_AB_MIX1ST_BW3, NVV_SLOT(frs_iqmm_ab_mix1st_bw3), "frs_iqmm_ab_mix1st_bw3" },
};

static const struct nvv_tag_desc *nvv_lookup_tag(unsigned tag)
{
    for (size_t i = 0; i < ARRAY_SIZE(nvv_tag_catalogue); ++i)
        if (nvv_tag_catalogue[i].tag == tag)
            return &nvv_tag_catalogue[i];
    return NULL;
}

static void nvv_set_presence(const struct nvv_tag_desc *desc)
{
    if (desc->presence_word == 0)
        nvv_presence.p0 |= desc->presence_bit;
    else
        nvv_presence.p1 |= desc->presence_bit;
}

static void nvv_store_payload(const struct nvv_tag_desc *desc, const uint8_t *payload)
{
    *(const uint8_t **)((uint8_t *)&nvv + desc->slot) = payload;
}

/* Debug dumps of the payload head for the records the bring-up team watched. */
static void nvv_dump_payload(unsigned tag, const uint8_t *p)
{
    const uint16_t *h = (const uint16_t *)p;
    const uint32_t *w = (const uint32_t *)p;

    switch (tag) {
    case NVV_TAG_TX_MAXPWR_CAL_V2:
        dbg_printf("TX PWR TBL: %02x %02x %02x %02x\n", w[0], w[1], w[2], w[3]);
        break;
    case NVV_TAG_RX_MAXGAIN_CAL:
        dbg_printf("rx_maxgain_cal_p: %08x %08x..\n", w[0], w[1]);
        break;
    case NVV_TAG_A0_RSSI_CAL:
        dbg_printf("NVV_TAG_A0_RSSI_CAL: %02x %02x %02x %02x  %02x %02x %02x  \n",
                   p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
        break;
    case NVV_TAG_MXDC_CAL:
        dbg_printf("NVV_TAG_MXDC_CAL: %02x %02x %02x %02x  %02x %02x   \n",
                   p[0], p[1], p[2], p[3], p[4], p[5]);
        break;
    case NVV_TAG_LB_DCOC_CAL_LP:
        dbg_printf("lb_dcoc_cal_lp: %04x %04x %04x %04x\n", h[0], h[1], h[2], h[3]);
        break;
    case NVV_TAG_LB_DCOC_CAL_HP:
        dbg_printf("lb_dcoc_cal_hp: %04x %04x %04x %04x\n", h[0], h[1], h[2], h[3]);
        break;
    case NVV_TAG_LBIQMM_AB_LP:
        dbg_printf("lbiqmm_ab_lp_p: %04x %04x %04x %04x\n", h[0], h[1], h[2], h[3]);
        break;
    case NVV_TAG_LBIQMM_AB_HP:
        dbg_printf("lbiqmm_ab_hp_p: %04x %04x %04x %04x\n", h[0], h[1], h[2], h[3]);
        break;
    case NVV_TAG_LBIQMM_DCOC_LP:
        dbg_printf("lbiqmm_dcoc_lp_p: %04x %04x %04x %04x\n", h[0], h[1], h[2], h[3]);
        break;
    case NVV_TAG_LBIQMM_DCOC_HP:
        dbg_printf("lbiqmm_dcoc_hp_p: %04x %04x %04x %04x\n", h[0], h[1], h[2], h[3]);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* Container parser                                                          */
/* ------------------------------------------------------------------------- */

struct nvv_record {
    unsigned    tag;
    uint32_t    length;         /* payload bytes */
    uint32_t    header_size;    /* 1, 2 or 4 */
    const uint8_t *payload;
};

static void nvv_decode_record(const uint8_t *p, struct nvv_record *rec)
{
    int16_t word = (int16_t)(p[0] | (p[1] << 8));

    if (word >= 0) {
        rec->tag = p[0];
        rec->length = 1;
        rec->header_size = 1;
    } else if (((uint16_t)word & 0xC000) == 0x8000) {
        rec->tag = (uint16_t)word & 0x7FF;
        rec->length = ((uint16_t)word >> 11) & 7;
        rec->header_size = 2;
    } else {
        rec->tag = (uint16_t)word & 0x3FFF;
        rec->length = (uint16_t)(p[2] | (p[3] << 8));
        rec->header_size = 4;
    }
    rec->payload = p + rec->header_size;
}

/*
 * Handle one record.  Returns the payload bytes to skip, which for the
 * register-initialisation records is rounded up to whole entries.
 */
static uint32_t nvv_handle_record(const struct nvv_record *rec, const char **prev_name)
{
    const uint8_t *payload = rec->payload;
    const struct nvv_tag_desc *desc;
    const char *name = "??";
    uint32_t skip = rec->length;
    /* Per-record value column of the log line; never assigned in this build. */
    uint32_t tag_value = 0;

    switch (rec->tag) {
    case NVV_TAG_GLOBAL_PHY_INITS:
        name = "global_phy_inits";
        nvv_presence.p0 |= NVV_P0_GLOBAL_PHY_INITS;
        dbg_printf("NVV %s (%x) %d bytes = %d\n", name, rec->tag, rec->length, 1);
        skip = nvv_apply_reg_writes(payload, rec->length);
        break;

    case NVV_TAG_GLOBAL_PHY_MSK_INITS:
    case NVV_TAG_GLOBAL_RADIO_MSK_INITS:
        if (rec->tag == NVV_TAG_GLOBAL_PHY_MSK_INITS) {
            name = "global_phy_msk_inits";
            nvv_presence.p0 |= NVV_P0_GLOBAL_PHY_MSK_INITS;
        } else {
            name = "global_radio_msk_inits";
            nvv_presence.p0 |= NVV_P0_GLOBAL_RADIO_MSK_INITS;
        }
        dbg_printf("NVV %s (%x) %d bytes = %d\n", name, rec->tag, rec->length, 2);
        skip = nvv_apply_reg_masked_writes(payload, rec->length);
        break;

    case NVV_TAG_DPD_ACTUATION_TABLE:
        dpd_set_tables(payload, rec->length);
        nvv_presence.p0 |= NVV_P0_DPD_ACTUATION_TABLE;
        name = "dpd_actuation_table";
        dbg_printf("NVV %s (%x) %d bytes = %d\n", name, rec->tag, rec->length, tag_value);
        break;

    default:
        desc = nvv_lookup_tag(rec->tag);
        if (desc != NULL) {
            if (rec->tag == NVV_TAG_RF_SEQ)
                SEQ_RFSEQ_PTR = (uint32_t)(uintptr_t)payload;
            nvv_store_payload(desc, payload);
            nvv_set_presence(desc);
            nvv_dump_payload(rec->tag, payload);
            /* Two records have no name of their own and echo the previous one. */
            name = desc->name != NULL ? desc->name : *prev_name;
        }
        dbg_printf("NVV %s (%x) %d bytes = %d\n", name, rec->tag, rec->length, tag_value);
        break;
    }

    *prev_name = name;
    return skip;
}

/* @provenance RVA 0x50B0 nvv_parse_blob */
uint32_t nvv_parse_blob(const uint8_t *blob, uint32_t size)
{
    uint32_t offset = 0;
    /* Raw R0 result, not a payload pointer: startup leaves the format-string
     * address in R0 for an empty input; completed containers return offset+8. */
    uint32_t result = 0x835Du;
    const char *prev_name = NULL;       /* undefined before the first record */

    while (offset < size) {
        const struct nvv_blob_header *hdr = (const void *)(blob + offset);
        const uint8_t *records = blob + offset + sizeof *hdr;
        const uint8_t *p = records;
        uint32_t consumed = 0;

        if (hdr->magic != NVV_BLOB_MAGIC)
            return (uint32_t)dbg_printf("Done @ %04x!\n", offset);
        if (offset + hdr->payload_length + sizeof *hdr > size)
            dbg_printf("Blob too big! (%x + %x > %x)\n");
        dbg_printf("Blob @ %x..\n", offset);

        while (consumed < hdr->payload_length) {
            struct nvv_record rec;
            uint32_t skip;

            nvv_decode_record(p, &rec);
            skip = nvv_handle_record(&rec, &prev_name);
            p = rec.payload + skip;
            consumed += rec.header_size + rec.length;
        }

        result = offset + sizeof *hdr;
        offset += sizeof *hdr + hdr->payload_length;
    }
    return result;
}

/* ------------------------------------------------------------------------- */
/* Modem bring-up                                                            */
/* ------------------------------------------------------------------------- */

/* ROM 0x58D4 compares at most four characters with strncmp. This C
 * adapter returns a predicate; the raw helper returns strncmp's signed result.
 * A longer module ID can therefore match without full-string equality. */
/* @provenance RVA 0x58D4 nvv_module_id_compare4 */
int nvv_module_prefix4_matches(const char *module_id, const char *expected_prefix)
{
    return strncmp(module_id, expected_prefix, 4) == 0;
}

/* Pick the HF delay table for the module this radio is built into. */
static void modem_select_module_tables(void)
{
    const char *id = nvv.mod_info;

    if (id == NULL) {
        dbg_printf("missing mod info, defaulting to YNGR\n");
        nvv.hf_coeff = nvv.dynamic_hf_delay_pty;
    } else if (nvv_module_prefix4_matches(id, NVV_MODULE_DAUG)) {
        dbg_printf("found DAUG module\n");
    } else if (nvv_module_prefix4_matches(id, NVV_MODULE_PLTY)) {
        dbg_printf("found PLTY module\n");
        nvv.hf_coeff = nvv.dynamic_hf_delay_pty;
    } else if (nvv_module_prefix4_matches(id, NVV_MODULE_PLTE)) {
        dbg_printf("found PLTE module\n");
        nvv.hf_coeff = nvv.dynamic_hf_delay_pte;
    } else if (nvv_module_prefix4_matches(id, NVV_MODULE_YNGR)) {
        dbg_printf("found YNGR module\n");
        nvv.hf_coeff = nvv.dynamic_hf_delay_pty;
    } else if (nvv_module_prefix4_matches(id, NVV_MODULE_ELDR)) {
        dbg_printf("found ELDR module\n");
        nvv.hf_coeff = nvv.dynamic_hf_delay_pte;
    } else {
        dbg_printf("Error! Unexpected module id!\n");
        trace_log_error(TRACE_ERR_CLASS_COMMAND, *(const uint32_t *)id,
                        TRACE_ERR_NVV_UNKNOWN_MODULE);
        nvv.hf_coeff = nvv.dynamic_hf_delay_pty;
    }
}

/* @provenance RVA 0x4EDC modem_init_load_nvv_and_module */
void modem_init(void)
{
    dbg_printf("\nModem inits:\n");
    SEQ_RFSEQ_PTR = (uint32_t)(uintptr_t)nvv_rfseq_fallback;

    /* Tables that are refreshed by every load start out absent. */
    nvv.tx_maxpwr_cal = NULL;
    nvv.rxiqmm_ab_mix1st_bw3 = NULL;
    nvv.rxiqmm_ab_mix1st_bw2p5 = NULL;
    nvv.rxiqmm_ab_mix1st_bw2 = NULL;
    nvv.rxiqmm_ab_mix1st_bw1p5 = NULL;
    nvv.rxiqmm_ab_lna1st_bw3 = NULL;
    nvv.rxiqmm_ab_lna1st_bw2p5 = NULL;
    nvv.rxiqmm_ab_lna1st_bw2 = NULL;
    nvv.rxiqmm_ab_lna1st_bw1p5 = NULL;
    nvv.lbiqmm_ab_hp = NULL;
    nvv.lbiqmm_ab_lp = NULL;
    nvv.lbiqmm_dcoc_hp = NULL;
    nvv.lbiqmm_dcoc_lp = NULL;
    nvv.lb_dcoc_cal_hp = NULL;
    nvv.lb_dcoc_cal_lp = NULL;
    nvv.mxdc_cal = NULL;
    nvv.a0_rssi_cal = NULL;

    dbg_printf("loading rfseq\n");
    nvv_parse_blob(nvv_rfseq_blob, sizeof nvv_rfseq_blob);
    dbg_printf("loading mdmi\n");
    nvv_parse_blob(NVV_MDMI_BLOB, NVV_MDMI_SIZE);

    if (radio_revision_get() == RADIO_REV_A0) {
        nvv.dynamic_exp = nvv_dynamic_exp_rev2;
        nvv.dynamic_mant = (const uint8_t *)nvv_dynamic_mant_rev2;
    }

    modem_select_module_tables();

    /* Remember the RSSI calibration the register inits left behind. */
    diag.rssi_baseline[0] = FIELD_GET(RX_RSSI_CAL0_BASE, RX_RSSI_CAL0);
    diag.rssi_baseline[1] = FIELD_GET(RX_RSSI_CAL1_MIX, RX_RSSI_CAL1);
    diag.rssi_baseline[2] = FIELD_GET(RX_RSSI_CAL1_LNA, RX_RSSI_CAL1);
    diag.rssi_baseline[3] = FIELD_GET(RX_RSSI_CAL2_ATTEN, RX_RSSI_CAL2);
}

/* @provenance RVA 0x5824 modem_warm_boot_restore_regs */
int modem_restore_registers(void)
{
    SEQ_RFSEQ_PTR = (uint32_t)(uintptr_t)nvv.rf_seq;
    modem_set_register_unlock(true);
    volatile struct phy_status_region *status = phy_status;

    for (uint32_t i = 0; i < status->warm_restore_count; ++i) {
        /* The reference reads the value before the destination address. A
         * restore entry may modify the count or a subsequent table entry. */
        uint32_t value = status->warm_restore[i].value;
        uint32_t address = status->warm_restore[i].address;

        MMIO32(address) = value;
    }
    return modem_set_register_unlock(false);
}
