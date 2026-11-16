/*
 * regs_seq.h -- packet sequencer, RF sequence engine, PRBS generator, system
 * clock gating and the MAC<->PHY doorbell block (0x4D154000).
 */
#ifndef BTP_REGS_SEQ_H
#define BTP_REGS_SEQ_H

#include "regs.h"

#define SEQ_BASE                    0x4D154000u
#define SEQ_REG(offset)             MMIO32(SEQ_BASE + (offset))

#define SEQ_CTRL                    SEQ_REG(0x000)
#define SEQ_CTRL_TX_START           BIT(0)
#define SEQ_CTRL_TONE_EN            BIT(1)
#define SEQ_CTRL_RF_LOAD            BIT(2)
#define SEQ_CTRL_TONE_LOAD          BIT(3)
#define SEQ_CTRL_CHAN_LOAD          BIT(4)
#define SEQ_CTRL_FORCE_TX           BIT(5)
#define SEQ_CTRL_PWR_LOAD           BIT(6)
#define SEQ_CTRL_TONE_MOD           BIT(14)
#define SEQ_CTRL_FRS_MODE_BITS      0x3Eu

#define SEQ_CFG                     SEQ_REG(0x004)
#define SEQ_CFG_TONE_CODE           RFIELD(12, 4)
#define SEQ_CFG_FRS_TONE_CODE       0xBu
#define SEQ_CFG_FRS_MODE            RFIELD(12, 5)   /* TONE_CODE plus the bit above it, cleared together by FRS */
#define SEQ_CFG_TONE_RUN            BIT(30)
#define SEQ_CFG_PRBS_RUN            BIT(31)

#define SEQ_CHAN                    SEQ_REG(0x008)
#define SEQ_CHAN_CHANNEL            RFIELD(0, 7)
#define SEQ_CHAN_PWR_CODE           RFIELD(8, 8)
#define SEQ_CHAN_TONE_OFFSET        BIT(16)
#define SEQ_CHAN_BAND               RFIELD(19, 3)
#define SEQ_CHAN_FRS_CHANNEL_BIAS   0x7Du

#define SEQ_RFSEQ_PTR               SEQ_REG(0x01C)

#define SEQ_IRQ                     SEQ_REG(0x020)
#define SEQ_IRQ_RX_MODE             RFIELD(0, 7)
#define SEQ_IRQ_TX_SETUP_DONE       BIT(16)
#define SEQ_IRQ_TX_DONE_ACK         BIT(17)
#define SEQ_IRQ_RX_SETUP_DONE       BIT(18)
#define SEQ_IRQ_RX_DONE_ACK         BIT(19)

#define SEQ_TONE_CFG                SEQ_REG(0x024)
#define SEQ_TONE_CFG_SOURCE         RFIELD(2, 3)

/* Per-packet command words written by the MAC sequencer. */
#define SEQ_PKT_CMD0                SEQ_REG(0x030)
#define SEQ_PKT_CMD0_CHANNEL        RFIELD(0, 7)
#define SEQ_PKT_CMD0_ALT_FORMAT     BIT(8)
#define SEQ_PKT_CMD0_FLAGS          RFIELD(8, 8)
#define SEQ_PKT_CMD0_BAND           RFIELD(14, 3)
#define SEQ_PKT_CMD0_ALT_RATE       RFIELD(17, 3)
#define SEQ_PKT_CMD0_LENGTH         RFIELD(23, 9)

#define SEQ_PKT_CMD1                SEQ_REG(0x034)
#define SEQ_PKT_CMD1_PWR_STEP       RFIELD(8, 4)
#define SEQ_PKT_CMD1_PWR_ALT        BIT(15)
#define SEQ_PKT_CMD1_PKT_TYPE       RFIELD(18, 4)
#define SEQ_PKT_CMD1_PKT_CLASS      RFIELD(17, 5)
#define SEQ_PKT_CMD1_RATE           RFIELD(19, 3)

#define SEQ_RF_CTRL                 SEQ_REG(0x038)
#define SEQ_RF_CTRL_TX_MODE         BIT(3)
#define SEQ_RF_CTRL_RX_MODE         BIT(4)
#define SEQ_RF_CTRL_HIGH_BAND       BIT(5)
#define SEQ_RF_CTRL_PA_GAIN         RFIELD(6, 5)
#define SEQ_RF_CTRL_PA_STAGE        RFIELD(12, 2)
#define SEQ_RF_CTRL_ALT_PATH        BIT(14)
#define SEQ_RF_CTRL_ALT_RATE        BIT(15)
#define SEQ_RF_CTRL_TX_ACTIVE       BIT(20)
#define SEQ_RF_CTRL_TX_ALT_RATE     BIT(21)
#define SEQ_RF_CTRL_SYNTH_BAND_LO   BIT(22)
#define SEQ_RF_CTRL_SYNTH_BAND_HI   BIT(23)
#define SEQ_RF_CTRL_CHAN_LATCH      BIT(24)
#define SEQ_RF_CTRL_PWR_MODE_LATCH  BIT(25)
#define SEQ_RF_CTRL_PWR_MODE        BIT(26)
#define SEQ_RF_CTRL_FRS_MODE        RFIELD(29, 3)
#define SEQ_RF_CTRL_FRS_MODE_SCAN   5u

#define SEQ_TX_CFG                  SEQ_REG(0x03C)
#define SEQ_TX_CFG_RAMP             RFIELD(8, 8)
#define SEQ_TX_CFG_RAMP_DEFAULT     0x80u
#define SEQ_TX_CFG_RAMP_EN          BIT(2)
#define SEQ_TX_CFG_ALT_RATE         BIT(16)

#define SEQ_REG_UNLOCK              SEQ_REG(0x048)
#define SEQ_REG_UNLOCK_PRBS_RESET   BIT(0)
#define SEQ_REG_UNLOCK_RADIO        BIT(4)
#define SEQ_REG_UNLOCK_RADIO_SEQ    BIT(5)
#define SEQ_REG_UNLOCK_MODEM        BIT(6)

/* 48-bit timestamps: low word + 16-bit high half. */
#define SEQ_RX_TRIGGER_TS_LO        SEQ_REG(0x118)
#define SEQ_RX_TRIGGER_TS_HI        SEQ_REG(0x11C)
#define SEQ_TX_TRIGGER_TS_LO        SEQ_REG(0x128)
#define SEQ_TX_TRIGGER_TS_HI        SEQ_REG(0x12C)
#define SEQ_TIMESTAMP_LO            SEQ_REG(0xC80)
#define SEQ_TIMESTAMP_HI            SEQ_REG(0xC84)

/* PRBS / modulated test transmitter. */
#define PRBS_CTRL                   SEQ_REG(0xB00)
#define PRBS_CTRL_EN                BIT(0)
#define PRBS_CTRL_PATTERN           RFIELD(17, 4)
#define PRBS_CTRL_RATE              RFIELD(24, 4)
#define PRBS_CFG                    SEQ_REG(0xB04)
#define PRBS_CFG_RUN                BIT(0)
#define PRBS_CFG_RESET              BIT(8)
#define PRBS_SEED0                  SEQ_REG(0xB20)
#define PRBS_SEED1                  SEQ_REG(0xB24)
#define PRBS_LENGTH                 SEQ_REG(0xB74)

/* System clock gating / reset. */
#define SYS_CLK_CTRL0               SEQ_REG(0xC08)
#define SYS_CLK_CTRL1               SEQ_REG(0xC0C)
#define SYS_CLK_CTRL_EN             BIT(16)
#define SYS_CLK_CTRL2               SEQ_REG(0xC10)
#define SYS_CLK_CTRL2_PHY_EN        BIT(0)
#define SYS_CLK_CTRL2_MODEM_EN      BIT(1)
#define SYS_CLK_CTRL2_RESET_ACK     BIT(2)
#define SYS_CLK_CTRL2_RESET_REQ     BIT(3)
#define SYS_RESET_CLK_EN            SEQ_REG(0xC70)
#define SYS_RESET_ASSERT            SEQ_REG(0xC74)
#define SYS_RESET_DEASSERT          SEQ_REG(0xC78)
#define SYS_RESET_ALL_MASK          0xFu

/* MAC <-> PHY doorbell. */
#define IPC_DOORBELL                SEQ_REG(0xC60)     /* PHY -> MAC */
#define IPC_STATUS                  SEQ_REG(0xC68)     /* MAC -> PHY, read in the IRQ */
#define IPC_ACK                     SEQ_REG(0xC6C)
#define IPC_EVENT_MASK              0x0000FFFFu

#define IPC_TO_MAC_CRASH            BIT(0)
#define IPC_TO_MAC_WARM_BOOT_ACK    BIT(1)
#define IPC_TO_MAC_FRS_DONE         BIT(2)
#define IPC_TO_MAC_RESPONSE         BIT(15)

#define IPC_FROM_MAC_FRS_REQUEST    BIT(0)
#define IPC_FROM_MAC_RESERVED1      BIT(1)
#define IPC_FROM_MAC_COEX_REQUEST   BIT(2)
#define IPC_FROM_MAC_COMMAND        BIT(15)

#endif /* BTP_REGS_SEQ_H */
