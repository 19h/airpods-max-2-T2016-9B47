/*
 * regs_rxdig.h -- receive digital front-end, AGC and the fast-RSSI-scan
 * correlator (0x4D150000).
 */
#ifndef BTP_REGS_RXDIG_H
#define BTP_REGS_RXDIG_H

#include "regs.h"

#define RXDIG_BASE                  0x4D150000u
#define RXDIG_REG(offset)           MMIO32(RXDIG_BASE + (offset))

#define RXDIG_CTRL                  RXDIG_REG(0x008)
#define RXDIG_CTRL_TONE_MODE        BIT(1)

#define RX_FRS_CFG0                 RXDIG_REG(0x018)
#define RX_FRS_CFG0_KEEP_MASK       0xFC00C00Fu
#define RX_FRS_CFG0_VALUE           0x001400F0u
#define RX_FRS_CFG1                 RXDIG_REG(0x01C)
#define RX_FRS_CFG1_KEEP_MASK       0xFFC00C00u
#define RX_FRS_CFG1_VALUE           0x0003C019u

#define AGC_CTRL                    RXDIG_REG(0x02C)
#define AGC_CTRL_START              BIT(0)
#define AGC_CTRL_MODE               RFIELD(8, 7)

#define AGC_STATUS                  RXDIG_REG(0x030)
#define AGC_STATUS_TABLE_SEL        RFIELD(14, 2)
#define AGC_STATUS_STATE            RFIELD(22, 3)
#define AGC_STATUS_GAIN_INDEX       RFIELD(25, 7)
#define AGC_STATE_SETTLED           3

#define RX_FRS_THRESHOLD0           RXDIG_REG(0x058)
#define RX_FRS_THRESHOLD1           RXDIG_REG(0x05C)
#define RX_FRS_THRESHOLD_VALUE      RFIELD(0, 8)
#define RX_FRS_TIMING0              RXDIG_REG(0x068)
#define RX_FRS_TIMING1              RXDIG_REG(0x06C)
#define RX_FRS_TIMING_VALUE         RFIELD(0, 16)

#define RX_PKT_CFG0                 RXDIG_REG(0x07C)
#define RX_PKT_CFG0_SYNC_LEN        RFIELD(8, 8)
#define RX_FRS_WINDOW               RXDIG_REG(0x080)
#define RX_FRS_WINDOW_LEN           RFIELD(8, 8)
#define RX_PKT_CFG1                 RXDIG_REG(0x084)
#define RX_PKT_CFG1_TIMEOUT         RFIELD(16, 8)

#define RX_GAIN_TABLE_BASE          RXDIG_REG(0x090)
#define RX_GAIN_TABLE_BASE1         RFIELD(8, 8)
#define RX_GAIN_TABLE_BASE2         RFIELD(16, 8)
#define RX_GAIN_TABLE_BASE3         RFIELD(24, 8)

#define RX_MODE0                    RXDIG_REG(0x094)
#define RX_MODE0_DEMOD              RFIELD(8, 8)
#define RX_MODE1                    RXDIG_REG(0x098)
#define RX_MODE1_PARAMS             RFIELD(0, 20)

#define AGC_CFG                     RXDIG_REG(0x138)
#define AGC_CFG_RESET               BIT(6)
#define AGC_CFG_GAIN_MODE           RFIELD(8, 2)
#define AGC_GAIN_MODE_AUTO          0
#define AGC_GAIN_MODE_FORCED        1
#define AGC_GAIN_MODE_HOLD          3
#define AGC_CFG_FREEZE              BIT(24)

#define RX_GAIN_FORCE               RXDIG_REG(0x13C)
#define RX_GAIN_FORCE_WORD          RFIELD(0, 16)
#define RX_GAIN_FORCE_INDEX         RFIELD(16, 8)

#define RX_PKT_CFG2                 RXDIG_REG(0x158)
#define RX_PKT_CFG2_BYTE0           RFIELD(0, 8)
#define RX_PKT_CFG2_BYTE1           RFIELD(8, 8)

#define RX_IQMM                     RXDIG_REG(0x168)
#define RX_IQMM_A                   RFIELD(4, 10)
#define RX_IQMM_B                   RFIELD(14, 10)
#define RX_IQMM_BYPASS              BIT(31)

#define RX_AGC_TARGET               RXDIG_REG(0x16C)
#define RX_AGC_TARGET_LEVEL         RFIELD(18, 6)
#define RX_AGC_TARGET_FRS           BIT(29)

#define RX_DEMOD_CTRL               RXDIG_REG(0x170)
#define RX_DEMOD_CTRL_WIDEBAND      BIT(16)

#define RX_RSSI_OFFSET              RXDIG_REG(0x18C)
#define RX_RSSI_OFFSET_VALUE        RFIELD(8, 8)

#define RX_MAXGAIN_CFG              RXDIG_REG(0x194)
#define RX_MAXGAIN_CFG_VALUE        0x3C0u

/* Max-gain limits: six 5-bit lanes per register at 0, 5, 10, 15, 20, 25. */
#define RX_MAXGAIN0                 RXDIG_REG(0x198)
#define RX_MAXGAIN1                 RXDIG_REG(0x19C)
#define RX_MAXGAIN2                 RXDIG_REG(0x1A0)
#define RX_MAXGAIN_LANE(n)          RFIELD(5 * (n), 5)

#define RX_RSSI_CAL1                RXDIG_REG(0x1C4)
#define RX_RSSI_CAL1_LNA            RFIELD(16, 8)
#define RX_RSSI_CAL1_MIX            RFIELD(24, 8)
#define RX_RSSI_CAL0                RXDIG_REG(0x1D0)
#define RX_RSSI_CAL0_BASE           RFIELD(16, 8)
#define RX_RSSI_CAL2                RXDIG_REG(0x1D4)
#define RX_RSSI_CAL2_ATTEN          RFIELD(24, 8)

#define COEX_GAIN                   RXDIG_REG(0x1CC)
#define COEX_GAIN_LNA               RFIELD(0, 7)
#define COEX_GAIN_MIX               RFIELD(8, 7)
#define COEX_GAIN2                  RXDIG_REG(0x214)
#define COEX_GAIN2_LIMIT            RFIELD(24, 7)

#define RX_PKT_STATUS               RXDIG_REG(0x1EC)
#define RX_PKT_STATUS_RSSI          RFIELD(0, 8)
#define RX_PKT_STATUS_CFO           RFIELD(8, 10)

#define RX_GAIN_TABLE(sel, index)   RXDIG_REG(0x500 + 0x100 * ((sel) - 1) + 4 * (index))

#define RX_PKT_PARAM_COUNT          7
#define RX_PKT_PARAM(n)             RXDIG_REG(0x570 + 4 * (n))

#define RX_IQMM_AUX                 RXDIG_REG(0x840)
#define RX_IQMM_AUX_GAIN            RFIELD(16, 8)
#define RX_IQMM_PHASE               RXDIG_REG(0x84C)
#define RX_IQMM_PHASE_MASK          0x0000FFFFu
#define RX_IQMM_PHASE_VALUE         RFIELD(16, 16)
#define RX_IQMM_SCALE               RXDIG_REG(0x940)
#define RX_IQMM_SCALE_VALUE         RFIELD(7, 25)

#define FRS_CORR_CTRL               RXDIG_REG(0xA8C)
#define FRS_CORR_CTRL_EN            BIT(4)
#define FRS_CORR_CTRL_LEN           RFIELD(8, 8)

#define FRS_CTRL                    RXDIG_REG(0xAD0)
#define FRS_CTRL_MODE               RFIELD(0, 16)
#define FRS_CTRL_GAIN               RFIELD(16, 8)
#define FRS_CTRL_OFFSET             RFIELD(24, 8)
#define FRS_STATUS                  RXDIG_REG(0xAD4)
#define FRS_STATUS_BUSY             BIT(1)
#define FRS_RSSI_PAIR(n)            RXDIG_REG(0xAD8 + 4 * (n))     /* n = 0..7 */
#define FRS_RSSI_CENTER             RXDIG_REG(0xAF8)
#define FRS_RSSI_CENTER_FLAGS_MASK  0xFFFFFF00u
#define FRS_RSSI_CENTER_CLIPPED     0x00000100u
#define FRS_TRIGGER                 RXDIG_REG(0xB5C)
#define FRS_TRIGGER_START           BIT(0)

#define RX_CORR_MAGIC               RXDIG_REG(0x19A0)
#define RX_CORR_MAGIC_VALUE         0x4A371FC3u

#define RX_FRS_MASK                 RXDIG_REG(0x1A98)
#define RX_FRS_MASK_ALL             0x1FFu

#endif /* BTP_REGS_RXDIG_H */
