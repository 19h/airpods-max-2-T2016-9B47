/*
 * regs_radio.h -- RF transceiver register block (0x4D000000).
 *
 * The radio carries two register banks for most of the front-end and
 * synthesiser controls.  Bank A sits at the base addresses, bank B is a copy
 * displaced by RADIO_FE_BANK_STRIDE (front-end group) or RADIO_SYNTH_BANK_STRIDE
 * (synthesiser group).  Which bank a given code path addresses depends on the
 * band being programmed; the predicates are written out at each call site
 * because they are not uniform.
 *
 * Field names describe the use the firmware makes of them.  Where a field's
 * purpose cannot be inferred from the firmware it is named after its position.
 */
#ifndef BTP_REGS_RADIO_H
#define BTP_REGS_RADIO_H

#include "regs.h"

#define RADIO_BASE                  0x4D000000u
#define RADIO_REG(offset)           MMIO32(RADIO_BASE + (offset))

enum radio_bank { RADIO_BANK_A = 0, RADIO_BANK_B = 1 };

#define RADIO_FE_BANK_STRIDE        0x24Cu   /* 0x0AC..0x0C0 <-> 0x2F8..0x30C */
#define RADIO_SYNTH_BANK_STRIDE     0x150u   /* 0x100..0x1C8 <-> 0x250..0x318 */

#define RADIO_FE_REG(bank, off)     RADIO_REG((off) + (bank) * RADIO_FE_BANK_STRIDE)
#define RADIO_SYNTH_REG(bank, off)  RADIO_REG((off) + (bank) * RADIO_SYNTH_BANK_STRIDE)

/* ---- identification ---------------------------------------------------- */
#define RADIO_ID                    RADIO_REG(0x000)
#define RADIO_ID_MAJOR              RFIELD(12, 8)   /* 'A' (0x41) or 'B' (0x42) */
#define RADIO_ID_MINOR              RFIELD(8, 4)

/* ---- front-end group (bank stride 0x24C) ------------------------------- */
#define RADIO_PA_CTRL(bank)         RADIO_FE_REG(bank, 0x0AC)
#define RADIO_PA_CTRL_GAIN          RFIELD(13, 4)

#define RADIO_TXDAC_GAIN(bank)      RADIO_FE_REG(bank, 0x0B0)
#define RADIO_TXDAC_GAIN_I          RFIELD(0, 12)
#define RADIO_TXDAC_GAIN_Q          RFIELD(12, 12)
#define RADIO_TXDAC_GAIN_OVERRIDE   BIT(24)
#define RADIO_TXDAC_GAIN_LOAD       BIT(25)

#define RADIO_LB_PATH(bank)         RADIO_FE_REG(bank, 0x0B8)
#define RADIO_LB_PATH_EN            BIT(22)

#define RADIO_LB_CFG(bank)          RADIO_FE_REG(bank, 0x0BC)
#define RADIO_LB_CFG_DCOC_I         RFIELD(0, 7)
#define RADIO_LB_CFG_DCOC_Q         RFIELD(7, 7)
#define RADIO_LB_CFG_ATTEN          RFIELD(14, 5)
#define RADIO_LB_CFG_MODE           RFIELD(23, 2)
#define RADIO_LB_CFG_GAIN           RFIELD(25, 4)
#define RADIO_LB_CFG_MODE_GAIN_MASK (RFIELD_MASK(RADIO_LB_CFG_MODE) | RFIELD_MASK(RADIO_LB_CFG_GAIN))

#define RADIO_LB_POWER(bank)        RADIO_FE_REG(bank, 0x0C0)
#define RADIO_LB_POWER_BIAS_EN      BIT(0)
#define RADIO_LB_POWER_EN           BIT(1)

/* ---- synthesiser group (bank stride 0x150) ----------------------------- */
#define RADIO_SYNTH_STATUS(bank)    RADIO_SYNTH_REG(bank, 0x100)
#define RADIO_SYNTH_STATUS_LOCK_FAIL_MASK   (BIT(17) | BIT(18))

#define RADIO_SYNTH_CTRL0(bank)     RADIO_SYNTH_REG(bank, 0x104)
#define RADIO_SYNTH_CTRL0_PD        BIT(28)
#define RADIO_SYNTH_CTRL1(bank)     RADIO_SYNTH_REG(bank, 0x11C)
#define RADIO_SYNTH_CTRL1_PD        BIT(29)

#define RADIO_RXFE_CTRL(bank)       RADIO_SYNTH_REG(bank, 0x15C)
#define RADIO_RXFE_CTRL_LNA_BYPASS  BIT(23)

#define RADIO_SYNTH_FREQ(bank)      RADIO_SYNTH_REG(bank, 0x16C)
#define RADIO_SYNTH_FREQ_MHZ        RFIELD(17, 14)
#define RADIO_SYNTH_FREQ_FROM_SEQ   BIT(31)

/* Bank-A-only registers. */
#define RADIO_PA_BIAS               RADIO_REG(0x138)
#define RADIO_PA_BIAS_STAGE1        RFIELD(5, 4)
#define RADIO_PA_BIAS_STAGE2        RFIELD(9, 4)
#define RADIO_PA_BIAS_MODE          RFIELD(13, 2)

#define RADIO_RX_FILTER             RADIO_REG(0x1C8)
#define RADIO_RX_FILTER_BW          RFIELD(9, 3)

#define RADIO_RESET                 RADIO_REG(0x1F4)
#define RADIO_RESET_TX_DIGITAL      BIT(28)
#define RADIO_RESET_RELEASE         BIT(29)

/* Bank-B-only registers. */
#define RADIO_RX_MXDC               RADIO_REG(0x24C)
#define RADIO_RX_MXDC_I             RFIELD(12, 6)
#define RADIO_RX_MXDC_Q             RFIELD(18, 6)

#define RADIO_LB_TONE               RADIO_REG(0x274)
#define RADIO_LB_TONE_LEVEL         RFIELD(8, 7)
#define RADIO_LB_TONE_EN            BIT(15)

#define RADIO_SYNTH_FREQ_OVERRIDE   RADIO_REG(0x2B4)
#define RADIO_SYNTH_FREQ_OVERRIDE_EN BIT(27)

#define RADIO_FRS_GAIN0             RADIO_REG(0x3B4)
#define RADIO_FRS_GAIN1             RADIO_REG(0x3C8)
#define RADIO_FRS_GAIN_CODE         RFIELD(24, 8)

#endif /* BTP_REGS_RADIO_H */
