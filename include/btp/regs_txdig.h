/*
 * regs_txdig.h -- transmit digital front-end (0x4D140000).
 */
#ifndef BTP_REGS_TXDIG_H
#define BTP_REGS_TXDIG_H

#include "regs.h"

#define TXDIG_BASE                  0x4D140000u
#define TXDIG_REG(offset)           MMIO32(TXDIG_BASE + (offset))

#define TXDIG_CTRL                  TXDIG_REG(0x000)
#define TXDIG_CTRL_PACKET_EN        BIT(0)
#define TXDIG_CTRL_REG_UNLOCK       BIT(5)
#define TXDIG_CTRL_REG_UNLOCK_SEQ   BIT(16)

#define TXDIG_MOD_CTRL              TXDIG_REG(0x008)
#define TXDIG_MOD_CTRL_OVERRIDE_MASK 0xFFFF0000u

#define TXDIG_AM_DELAY              TXDIG_REG(0x014)
#define TXDIG_AM_DELAY_PATH0        RFIELD(0, 3)
#define TXDIG_AM_DELAY_PATH1        RFIELD(3, 3)

#define TXDIG_DELAY                 TXDIG_REG(0x018)
#define TXDIG_DELAY_HF_PATH1        RFIELD(3, 3)
#define TXDIG_DELAY_HF_PATH0        RFIELD(6, 3)
#define TXDIG_DELAY_IF_PATH0        RFIELD(16, 4)
#define TXDIG_DELAY_IF_PATH1        RFIELD(20, 4)
#define TXDIG_DELAY_PWR_MODE        RFIELD(24, 2)

#define TXDIG_DF1                   TXDIG_REG(0x024)
#define TXDIG_DF1_MANTISSA          RFIELD(0, 16)
#define TXDIG_DF1_EXPONENT          RFIELD(16, 3)

#define TXDIG_IQ_SCALE              TXDIG_REG(0x02C)
#define TXDIG_IQ_SCALE_VALUE        RFIELD(16, 14)

#define TXDIG_A4P                   TXDIG_REG(0x20C)
#define TXDIG_A4P_GAIN              RFIELD(0, 12)
#define TXDIG_A4P_SLOPE             RFIELD(16, 12)

#define TXDIG_BAND                  TXDIG_REG(0x480)
#define TXDIG_BAND_ALT_RATE         BIT(0)
#define TXDIG_BAND_INDEX            RFIELD(1, 3)

#define TXDIG_HF_COEFF_COUNT        6
#define TXDIG_HF_COEFF(n)           TXDIG_REG(0x900 + 4 * (n))

#endif /* BTP_REGS_TXDIG_H */
