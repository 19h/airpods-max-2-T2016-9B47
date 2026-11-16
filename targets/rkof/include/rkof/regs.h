#ifndef RKOF_REGS_H
#define RKOF_REGS_H

#include <stdint.h>

/*
 * Apple T2016 (b515d) SoC & W3 PHY/Modem MMIO Register Definitions for rkof
 * Verified against literal pools in ../../rkof
 */

/* Early Boot Remap & Scratch Aperture (rtkit_entry @ 0x000108) */
#define RKOF_SCRATCH_RTSI_ADDR          0x008000F0u
#define RKOF_REMAP_BASE                 0x4000C000u
#define RKOF_REMAP_CTRL_ADDR            0x4000C02Cu
#define RKOF_REMAP_CTRL_ENABLE_BIT      0x00010000u
#define RKOF_REMAP_STATUS_ADDR          0x4000C080u
#define RKOF_REMAP_STATUS_DONE_BIT      0x00000002u
#define RKOF_REMAP_CODE_RMC1_ADDR       0x4000C0A0u
#define RKOF_REMAP_CODE_RMC2_ADDR       0x4000C0A4u
#define RKOF_REMAP_CODE_RMC3_ADDR       0x4000C0A8u
#define RKOF_REMAP_DATA_RMD1_ADDR       0x4000C0B0u
#define RKOF_REMAP_DATA_RMD2_ADDR       0x4000C0B4u
#define RKOF_REMAP_DATA_RMD3_ADDR       0x4000C0B8u
#define RKOF_REMAP_RMFI_ADDR            0x4000C120u
#define RKOF_REMAP_ACK_ADDR             0x4000C12Cu

/* Pinmux / GPIO Pad Control (bt_phy_otp_init @ 0x0FD7C0, phy_shell_cmd @ 0x0FE120) */
#define RKOF_UART_PINMUX_AUX_ADDR       0x420190BCu
#define RKOF_UART_PINMUX_TX_ADDR        0x42019204u
#define RKOF_UART_PINMUX_RX_ADDR        0x42019208u

/* PMGR PHY Clocks, Mux & OTP Power (0x42440000..0x42445FFF) */
#define RKOF_PMGR_PHY_CLK0_ADDR         0x42440400u
#define RKOF_PMGR_PHY_CLK8_ADDR         0x42440408u
#define RKOF_PMGR_PHY_CLK10_ADDR        0x42440410u
#define RKOF_PMGR_PHY_CLK18_ADDR        0x42440418u
#define RKOF_PMGR_PHY_CLK20_ADDR        0x42440420u
#define RKOF_PMGR_OTP_PWR_ADDR          0x42443000u
#define RKOF_PMGR_PHY_MUX_ADDR          0x42445028u

/* RF PMGR & Analog Isolation (0x4246E000..0x4247AFFF) */
#define RKOF_RF_PMGR_BASE               0x4246E004u
#define RKOF_RF_PMGR_CAL_TRIM_ADDR      0x4246E008u
#define RKOF_RF_PMGR_PLL_CFG_ADDR       0x4246E020u
#define RKOF_RF_PMGR_CLK_GATE_ADDR      0x4246E030u
#define RKOF_RF_PMGR_ANA_EN_ADDR        0x4246E034u
#define RKOF_RF_PMGR_DIG_EN_ADDR        0x4246E03Cu
#define RKOF_RF_PMGR_BIAS_ADDR          0x4246E070u
#define RKOF_RF_PMGR_LDO_ADDR           0x4246E074u
#define RKOF_RF_ISOLATION_ADDR          0x4247A004u

/* W3 PHY Top & Modem Reset / Warm-Restore Window (0x4D000000..0x4D155000) */
#define RKOF_W3_GLOBAL_TIMEBASE_ADDR    0x4D000800u
#define RKOF_W3_PHY_TOP_CTRL_ADDR       0x4D140000u
#define RKOF_W3_PHY_TOP_RST_BIT5        (1u << 5)
#define RKOF_W3_PHY_TOP_RST_BIT16       (1u << 16)
#define RKOF_W3_PHY_TOP_RST_BIT19       (1u << 19)

#define RKOF_W3_MODEM_WARM_MIN_ADDR     0x4D154000u
#define RKOF_W3_MODEM_WARM_MAX_ADDR     0x4D155000u
#define RKOF_W3_MODEM_RESET_ADDR        0x4D154048u
#define RKOF_W3_MODEM_RESET_BIT4        (1u << 4)
#define RKOF_W3_MODEM_RESET_MASK45      0x00000030u
#define RKOF_W3_MODEM_RESET_BIT6        (1u << 6)

#endif /* RKOF_REGS_H */
