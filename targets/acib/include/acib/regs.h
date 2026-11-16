#ifndef ACIB_REGS_H
#define ACIB_REGS_H

#include "acib/memmap.h"

/* --- ARMv7-M DWT Registers --- */
#define ACIB_DWT_CTRL_ADDR          (ACIB_ARM_DWT_BASE + 0x000u)
#define ACIB_DWT_CYCCNT_ADDR        (ACIB_ARM_DWT_BASE + 0x004u)

/* --- ARMv7-M NVIC Registers --- */
#define ACIB_NVIC_ISER0_ADDR        (ACIB_ARM_SCS_BASE + 0x100u) /* 0xE000E100 */
#define ACIB_NVIC_ICER0_ADDR        (ACIB_ARM_SCS_BASE + 0x180u) /* 0xE000E180 */
#define ACIB_NVIC_ISPR0_ADDR        (ACIB_ARM_SCS_BASE + 0x200u) /* 0xE000E200 */
#define ACIB_NVIC_ICPR0_ADDR        (ACIB_ARM_SCS_BASE + 0x280u) /* 0xE000E280 */
#define ACIB_NVIC_IPR0_ADDR         (ACIB_ARM_SCS_BASE + 0x400u) /* 0xE000E400 */

/* --- ARMv7-M SCB Registers --- */
#define ACIB_SCB_ICSR_ADDR          (ACIB_ARM_SCB_BASE + 0x004u) /* 0xE000ED04 */
#define ACIB_SCB_VTOR_ADDR          (ACIB_ARM_SCB_BASE + 0x008u) /* 0xE000ED08 */
#define ACIB_SCB_AIRCR_ADDR         (ACIB_ARM_SCB_BASE + 0x00Cu) /* 0xE000ED0C */
#define ACIB_SCB_SCR_ADDR           (ACIB_ARM_SCB_BASE + 0x010u) /* 0xE000ED10 */
#define ACIB_SCB_CCR_ADDR           (ACIB_ARM_SCB_BASE + 0x014u) /* 0xE000ED14 */
#define ACIB_SCB_SHPR1_ADDR         (ACIB_ARM_SCB_BASE + 0x018u) /* 0xE000ED18 */
#define ACIB_SCB_SHPR2_ADDR         (ACIB_ARM_SCB_BASE + 0x01Cu) /* 0xE000ED1C */
#define ACIB_SCB_SHPR3_ADDR         (ACIB_ARM_SCB_BASE + 0x020u) /* 0xE000ED20 */
#define ACIB_SCB_SHCSR_ADDR         (ACIB_ARM_SCB_BASE + 0x024u) /* 0xE000ED24 */
#define ACIB_SCB_CPACR_ADDR         (ACIB_ARM_SCB_BASE + 0x088u) /* 0xE000ED88 */

#define ACIB_SCB_ICSR_PENDSTCLR     (1u << 25)
#define ACIB_SCB_ICSR_PENDSTSET     (1u << 26)
#define ACIB_SCB_ICSR_PENDSVSET     (1u << 28)
#define ACIB_SCB_ICSR_NMIPENDSET    (1u << 31)

/* --- ARMv7-M MPU Registers --- */
#define ACIB_MPU_TYPE_ADDR          (ACIB_ARM_MPU_BASE + 0x000u) /* 0xE000ED90 */
#define ACIB_MPU_CTRL_ADDR          (ACIB_ARM_MPU_BASE + 0x004u) /* 0xE000ED94 */
#define ACIB_MPU_RNR_ADDR           (ACIB_ARM_MPU_BASE + 0x008u) /* 0xE000ED98 */
#define ACIB_MPU_RBAR_ADDR          (ACIB_ARM_MPU_BASE + 0x00Cu) /* 0xE000ED9C */
#define ACIB_MPU_RASR_ADDR          (ACIB_ARM_MPU_BASE + 0x010u) /* 0xE000EDA0 */

#define ACIB_MPU_CTRL_ENABLE        (1u << 0)

/* --- W3 PHY Sequencer & Inter-Processor Doorbell Registers (0x4D154000) --- */
#define ACIB_PHY_SEQ_RF_CTRL_ADDR       (ACIB_W3_PHY_SEQ_BASE + 0x038u) /* 0x4D154038 */
#define ACIB_PHY_IPC_TO_MAC_STATUS_ADDR (ACIB_W3_PHY_SEQ_BASE + 0xC64u) /* 0x4D154C64 (W1C) */
#define ACIB_PHY_IPC_FROM_MAC_DB_ADDR   (ACIB_W3_PHY_SEQ_BASE + 0xC68u) /* 0x4D154C68 */

/* 0x4D154C64: PHY -> MAC interrupt status / W1C bits */
#define ACIB_PHY_STATUS_CRASH           (1u << 0)  /* 0x0001: PHY panic/crash dump ready */
#define ACIB_PHY_STATUS_WARM_BOOT_ACK   (1u << 1)  /* 0x0002: PHY warm-boot handshake ack */
#define ACIB_PHY_STATUS_FRS_DONE        (1u << 2)  /* 0x0004: Fast RSSI Scan completed */
#define ACIB_PHY_STATUS_CMD_ACK         (1u << 15) /* 0x8000: Synchronous command response */

/* 0x4D154C68: MAC -> PHY doorbell trigger bits */
#define ACIB_PHY_DB_FRS_START           (1u << 0)  /* 0x0001: Start FRS channel scan */
#define ACIB_PHY_DB_FRS_ABORT           (1u << 1)  /* 0x0002: Abort / timeout FRS scan */
#define ACIB_PHY_DB_COEX_UPDATE         (1u << 2)  /* 0x0004: Coex grant word updated */
#define ACIB_PHY_DB_CMD_REQ             (1u << 15) /* 0x8000: Synchronous command request */

/* --- ACI Host Interface & Bluetooth Hardware Registers --- */
#define ACIB_ACI_IF_GATE_CTRL0_ADDR (ACIB_ACI_IF_BASE + 0x410u)     /* 0x42440410 */
#define ACIB_ACI_IF_GATE_CTRL1_ADDR (ACIB_ACI_IF_BASE + 0x418u)     /* 0x42440418 */

#define ACIB_SOC_PLAT_ID_ADDR       (ACIB_SOC_CHIP_CFG_BASE + 0x0D8u) /* 0x520DC0D8 */
#define ACIB_SOC_PLAT_ID_FPGA_VAL   0xFC00u

#define ACIB_BT_MODEM_IRQ_EN_ADDR   (ACIB_BT_MODEM_BASE + 0x150u)   /* 0x520E4150 */
#define ACIB_BT_MODEM_CFG_218_ADDR  (ACIB_BT_MODEM_BASE + 0x218u)   /* 0x520E4218 */
#define ACIB_BT_MODEM_CFG_274_ADDR  (ACIB_BT_MODEM_BASE + 0x274u)   /* 0x520E4274 */
#define ACIB_BT_MODEM_LUT0_ADDR     (ACIB_BT_MODEM_BASE + 0x300u)   /* 0x520E4300 */
#define ACIB_BT_MODEM_LUT1_ADDR     (ACIB_BT_MODEM_BASE + 0x304u)   /* 0x520E4304 */
#define ACIB_BT_MODEM_LUT2_ADDR     (ACIB_BT_MODEM_BASE + 0x308u)   /* 0x520E4308 */
#define ACIB_BT_MODEM_LUT3_ADDR     (ACIB_BT_MODEM_BASE + 0x30Cu)   /* 0x520E430C */
#define ACIB_BT_MODEM_RX_GAIN0_ADDR (ACIB_BT_MODEM_BASE + 0x320u)   /* 0x520E4320 */
#define ACIB_BT_MODEM_RX_GAIN1_ADDR (ACIB_BT_MODEM_BASE + 0x324u)   /* 0x520E4324 */
#define ACIB_BT_MODEM_SLOT_A_BASE   (ACIB_BT_MODEM_BASE + 0x400u)   /* 0x520E4400 */
#define ACIB_BT_MODEM_SLOT_B_BASE   (ACIB_BT_MODEM_BASE + 0x680u)   /* 0x520E4680 */
#define ACIB_BT_MODEM_CLK_DIV_ADDR  (ACIB_BT_MODEM_BASE + 0x928u)   /* 0x520E4928 */
#define ACIB_BT_MODEM_CAP_VAL_ADDR  (ACIB_BT_MODEM_BASE + 0x93Cu)   /* 0x520E493C */
#define ACIB_BT_MODEM_CAP_STS_ADDR  (ACIB_BT_MODEM_BASE + 0x940u)   /* 0x520E4940 */
#define ACIB_BT_MODEM_CTRL_944_ADDR (ACIB_BT_MODEM_BASE + 0x944u)   /* 0x520E4944 */

#define ACIB_BT_TIMER_US_ADDR       (ACIB_BT_TIMER_BASE + 0x020u)   /* 0x52160020 */

#endif /* ACIB_REGS_H */
