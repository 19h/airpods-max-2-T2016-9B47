#ifndef ACIB_MEMMAP_H
#define ACIB_MEMMAP_H

#include <stdint.h>

/*
 * Apple b515d (AirPods3,6) ACI / Bluetooth Controller (acib) Memory Map.
 * Derived from raw fwsg segment descriptors, acib.i64 segments, and
 * rtk_early_hw_init (RVA 0x07deb8) MPU region table.
 */

/* --- ROM & SRAM Segments --- */
#define ACIB_ROM_VECTORS_BASE       0x00000000u
#define ACIB_ROM_VECTORS_SIZE       0x000000C0u

#define ACIB_RTKIT_HEADER_BASE      0x000000C0u
#define ACIB_RTKIT_HEADER_SIZE      0x00000048u

#define ACIB_RESET_HANDLER_RVA      0x00000108u
#define ACIB_SECONDARY_VTOR_BASE    0x00000400u
#define ACIB_SECONDARY_VTOR_SIZE    0x00000240u

#define ACIB_ROM_TEXT_BASE          0x000000C0u
#define ACIB_ROM_TEXT_END           0x000EFEA8u
#define ACIB_ROM_TEXT_SIZE          (ACIB_ROM_TEXT_END - ACIB_ROM_TEXT_BASE)

#define ACIB_ROM_RODATA_BASE        0x000EFEA8u
#define ACIB_ROM_RODATA_END         0x000F984Cu
#define ACIB_ROM_RODATA_SIZE        (ACIB_ROM_RODATA_END - ACIB_ROM_RODATA_BASE)

#define ACIB_RAM_DATA_BASE          0x10000000u
#define ACIB_RAM_DATA_END           0x1000AC1Eu
#define ACIB_RAM_DATA_SIZE          (ACIB_RAM_DATA_END - ACIB_RAM_DATA_BASE)
#define ACIB_RAM_DATA_FILE_OFFSET   0x000F9860u

#define ACIB_RAM_BSS_BASE           0x1000AC1Eu
#define ACIB_RAM_BSS_ZERO_START     0x1000AC40u
#define ACIB_RAM_BSS_END            0x1002D2C4u
#define ACIB_RAM_BSS_SIZE           (ACIB_RAM_BSS_END - ACIB_RAM_BSS_BASE)

/* --- Stack Boundaries --- */
#define ACIB_PSP_INIT_STACK_BASE    0x1001AF80u
#define ACIB_PSP_INIT_STACK_TOP     0x1001B580u
#define ACIB_MSP_STACK_BASE         0x1001B580u
#define ACIB_MSP_STACK_TOP          0x1001BD80u
#define ACIB_MSP_STACK_SIZE         0x00000800u

/* --- MPU Regions Configured by rtk_early_hw_init (0x07deb8) --- */
#define ACIB_MPU_R0_PERIPH_BASE     0x40000000u /* order 0x1C (512 MB), Device, XN=1, AP=1 */
#define ACIB_MPU_R1_ROM_BASE        0x00000000u /* order 0x13 (1 MB),   Normal, XN=0, AP=5 */
#define ACIB_MPU_R2_SRAM_BASE       0x10000000u /* order 0x11 (256 KB), Normal, XN=1, AP=3 */
#define ACIB_MPU_R3_BT_SHM_BASE     0x521C0000u /* order 0x0F (64 KB),  Normal, XN=1, AP=1 */
#define ACIB_MPU_R4_SOC_SHM_BASE    0x20000000u /* order 0x17 (16 MB),  Normal, XN=1, AP=1 */
#define ACIB_MPU_R5_PHY_MBOX_BASE   0x4D280000u /* order 0x0B (4 KB),   Normal, XN=1, AP=1 */
#define ACIB_MPU_R6_ROM_HI_BASE     0x000F0000u /* order 0x0F (64 KB),  Normal, XN=0, AP=1 */

/* --- Peripheral & Inter-Processor Apertures --- */
#define ACIB_ACI_IF_BASE            0x42440000u
#define ACIB_W3_PHY_SEQ_BASE        0x4D154000u
#define ACIB_W3_PHY_SEQ_SIZE        0x00001000u

/* Shared MAC <-> W3 PHY (phyb) 4 KB Mailbox SRAM */
#define ACIB_PHY_MBOX_BASE          0x4D280000u
#define ACIB_PHY_MBOX_REQ_BASE      0x4D280000u
#define ACIB_PHY_MBOX_RESP_BASE     0x4D280400u
#define ACIB_PHY_MBOX_LEGACY_RESP   0x4D2801E6u
#define ACIB_PHY_MBOX_SIZE          0x00001000u

/* Bluetooth Controller Hardware Blocks */
#define ACIB_SOC_CHIP_CFG_BASE      0x520DC000u
#define ACIB_BT_MODEM_BASE          0x520E4000u
#define ACIB_BT_TIMER_BASE          0x52160000u
#define ACIB_BT_EVTRIG_BASE         0x521D8000u

/* ARMv7-M Core Debug & System Control Space */
#define ACIB_ARM_DWT_BASE           0xE0001000u
#define ACIB_ARM_SCS_BASE           0xE000E000u
#define ACIB_ARM_NVIC_BASE          0xE000E100u
#define ACIB_ARM_SCB_BASE           0xE000ED00u
#define ACIB_ARM_MPU_BASE           0xE000ED90u

#endif /* ACIB_MEMMAP_H */
