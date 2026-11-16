/*
 * memmap.h — Exact virtual memory map and raw image layout of aopf
 * (Always-On Processor Firmware, T2016 Rev B0 / b515d / Laguna).
 */

#ifndef AOPF_MEMMAP_H
#define AOPF_MEMMAP_H

#include <stdint.h>

/* Raw image geometry (../../aopf) */
#define AOPF_RAW_IMAGE_SIZE          0x00091c40u
#define AOPF_RAW_UUID_HDR_FILEOFF    0x000000c0u
#define AOPF_RAW_RODATA_FILEOFF      0x00064000u
#define AOPF_RAW_DATA_FILEOFF        0x00077b20u
#define AOPF_RAW_FWSG_ENTRIES_OFF    0x00091be0u
#define AOPF_RAW_FWSG_HEADER_OFF     0x00091c20u

/* Executable / Read-Only ROM aperture (0x00000000..0x00077B14) */
#define AOPF_ROM_VECTORS_BASE        0x00000000u
#define AOPF_ROM_VECTORS_END         0x000000c0u
#define AOPF_ROM_VECTORS_SIZE        0x000000c0u
#define AOPF_BOOT_VECTOR_COUNT       48u

#define AOPF_ROM_HEADER_BASE         0x000000c0u
#define AOPF_ROM_HEADER_END          0x00000108u
#define AOPF_ROM_HEADER_SIZE         0x00000048u

#define AOPF_ROM_TEXT_BASE           0x000000c0u
#define AOPF_ROM_TEXT_CODE_START     0x00000108u
#define AOPF_ROM_TEXT_END            0x00064000u
#define AOPF_ROM_TEXT_SIZE           0x00063f40u

#define AOPF_ROM_RODATA_BASE         0x00064000u
#define AOPF_ROM_RODATA_END          0x00077b14u
#define AOPF_ROM_RODATA_SIZE         0x00013b14u
#define AOPF_VTOR_TABLE_BASE         0x00064000u
#define AOPF_VTOR_VECTOR_COUNT       94u
#define AOPF_CRC32_TABLE_BASE        0x00072ec8u
#define AOPF_RTKIT_BUILD_STR_VMA     0x00073958u
#define AOPF_DEFAULT_EMPTY_STR_VMA   0x0007397cu
#define AOPF_METADATA_TLV_BASE       0x000739bcu
#define AOPF_METADATA_TLV_SIZE       0x00000132u
#define AOPF_CPP_INIT_ARRAY_BASE     0x00077ae0u
#define AOPF_CPP_INIT_ARRAY_END      0x00077b14u

/* AOP SRAM aperture (0x20800000..0x2089BED8, 623.7 KiB) */
#define AOPF_RAM_BASE                0x20800000u
#define AOPF_RAM_DATA_BASE           0x20800000u
#define AOPF_RAM_DATA_END            0x2081a0b0u
#define AOPF_RAM_DATA_SIZE           0x0001a0b0u
#define AOPF_RAM_BSS_BASE            0x2081a0b0u
#define AOPF_RAM_BSS_CLEAR_BASE      0x2081b000u
#define AOPF_RAM_END                 0x2089bed8u
#define AOPF_RAM_TOTAL_SIZE          0x0009bed8u
#define AOPF_RAM_BSS_SIZE            (AOPF_RAM_END - AOPF_RAM_BSS_BASE)

/* Hardware stack regions painted with "RTKSTACK" at reset */
#define AOPF_PSP_STACK_BOTTOM        0x20899490u
#define AOPF_PSP_STACK_TOP           0x2089a890u
#define AOPF_PSP_STACK_SIZE          0x00001400u

#define AOPF_MSP_STACK_BOTTOM        0x2089b610u
#define AOPF_MSP_STACK_TOP           0x2089ba10u
#define AOPF_MSP_STACK_SIZE          0x00000400u

/* Key RAM_DATA & RAM_BSS globals */
#define AOPF_RAM_BUILD_BANNER_VMA    0x2080c290u
#define AOPF_PEER_TABLE_VMA          0x2080c4ccu
#define AOPF_MBOX_MOTION_DEV_VMA     0x2080c4f0u
#define AOPF_MBOX_AP_DEV_VMA         0x2080c5d0u
#define AOPF_SPI_AUX_DEV_VMA         0x2080c728u
#define AOPF_SPMI_ARB_DEV_VMA        0x2080c7a0u
#define AOPF_TIMER_BANK_DEV_VMA      0x2080c7f8u
#define AOPF_I2CM0_DEV_VMA           0x2080c820u
#define AOPF_SPI0_DEV_VMA            0x2080c8b8u
#define AOPF_SCM_SPI0_DEV_VMA        0x2080c900u
#define AOPF_SCM_I2CM0_DEV_VMA       0x2080c998u
#define AOPF_SPMI_DEV_VMA            0x2080ca30u
#define AOPF_SPMI_BUS_DEV_VMA        0x2080ca48u
#define AOPF_FUSE_OTP_DEV_VMA        0x2080cb10u
#define AOPF_EP_CTRL_DEV_VMA         0x2080cb38u
#define AOPF_SYSTICK_DEV_VMA         0x2080cba8u
#define AOPF_TIMEBASE_DEV_VMA        0x2080cbe8u
#define AOPF_AON_TIMER_DEV_VMA       0x2080cc40u
#define AOPF_WDT_DEV_VMA             0x2080ccd8u
#define AOPF_MBOX_4002C000_DEV_VMA   0x2080cd20u
#define AOPF_MBOX_42218000_DEV_VMA   0x2080ce00u
#define AOPF_GPIO_DEV_VMA            0x2080d530u
#define AOPF_PMGR_DEV_VMA            0x2080d888u
#define AOPF_BATT_OVERRIDE_STR_VMA   0x2080dae4u
#define AOPF_NVIC_DEV_VMA            0x20819808u
#define AOPF_AP_EP_LIST_VMA          0x20819978u
#define AOPF_WARM_BOOT_MAGIC_VMA     0x20819ca0u
#define AOPF_WARM_BOOT_ENTRY_VMA     0x20819ca4u
#define AOPF_STACK_CHK_GUARD_VMA     0x2080c2d0u
#define AOPF_VM_INIT_STATE_VMA       0x2089aa80u
#define AOPF_VM_REGION_HEAD_VMA      0x2089aa84u
#define AOPF_CXA_GUARD_LOCK_VMA      0x2089ab80u
#define AOPF_CXA_GUARD_DEPTH_VMA     0x2089ab84u
#define AOPF_HEAP_FASTLOCK_VMA       0x2089a8a4u
#define AOPF_HEAP_CTX_STORAGE_VMA    0x2089a8a8u
#define AOPF_HEAP_CTX_PTR_VMA        0x2089bc90u
#define AOPF_IRQ_HANDLER_TABLE_VMA   0x2089ba10u
#define AOPF_MPU_SHADOW_TABLE_VMA    0x2089bc9cu

/* 17 Peripheral MMIO Apertures */
#define AOPF_MMIO_MBOX_SHARED_BASE   0x4002c000u
#define AOPF_MMIO_MBOX_SHARED_SIZE   0x00004000u

#define AOPF_MMIO_SCM_SPI0_BASE      0x42004000u
#define AOPF_MMIO_SCM_SPI0_SIZE      0x00004000u

#define AOPF_MMIO_SPI0_BASE          0x42008000u
#define AOPF_MMIO_SPI0_SIZE          0x00001000u

#define AOPF_MMIO_SCM_I2CM0_BASE     0x4200c000u
#define AOPF_MMIO_SCM_I2CM0_SIZE     0x00004000u

#define AOPF_MMIO_I2CM0_BASE         0x42010000u
#define AOPF_MMIO_I2CM0_SIZE         0x00001000u

#define AOPF_MMIO_SPI_AUX_BASE       0x42015000u
#define AOPF_MMIO_SPI_AUX_SIZE       0x00001000u

#define AOPF_MMIO_TIMEBASE_BASE      0x42016000u
#define AOPF_MMIO_TIMEBASE_SIZE      0x00001000u

#define AOPF_MMIO_AOP_PWR_WDT_BASE   0x42018000u
#define AOPF_MMIO_AOP_PWR_WDT_SIZE   0x00001000u

#define AOPF_MMIO_GPIO_BASE          0x42019000u
#define AOPF_MMIO_GPIO_SIZE          0x00001000u

#define AOPF_MMIO_FUSE_OTP_BASE      0x4201c000u
#define AOPF_MMIO_FUSE_OTP_SIZE      0x00001000u

#define AOPF_MMIO_TIMER_BANK_BASE    0x42020000u
#define AOPF_MMIO_TIMER_BANK_SIZE    0x00001000u

#define AOPF_MMIO_SPMI_BASE          0x42038000u
#define AOPF_MMIO_SPMI_SIZE          0x00004000u

#define AOPF_MMIO_AON_WDT_BASE       0x42200000u
#define AOPF_MMIO_AON_WDT_SIZE       0x00001000u

#define AOPF_MMIO_MBOX_AUX_BASE      0x42218000u
#define AOPF_MMIO_MBOX_AUX_SIZE      0x00004000u

#define AOPF_MMIO_MBOX_AP_BASE       0x42220000u
#define AOPF_MMIO_MBOX_AP_SIZE       0x00008000u

#define AOPF_MMIO_PMGR_BASE          0x42440000u
#define AOPF_MMIO_PMGR_SIZE          0x0000f000u

#define AOPF_MMIO_MBOX_MOTION_BASE   0x440c4000u
#define AOPF_MMIO_MBOX_MOTION_SIZE   0x00008000u

/* ARMv7E-M Core Private Peripheral Bus (PPB) */
#define AOPF_ARM_DWT_BASE            0xe0001000u
#define AOPF_ARM_DWT_SIZE            0x00001000u
#define AOPF_ARM_SCS_BASE            0xe000e000u
#define AOPF_ARM_SCS_SIZE            0x00001000u

#endif /* AOPF_MEMMAP_H */
