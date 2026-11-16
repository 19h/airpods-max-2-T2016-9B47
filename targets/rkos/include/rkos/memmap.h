#ifndef RKOS_MEMMAP_H
#define RKOS_MEMMAP_H

#include <stddef.h>
#include <stdint.h>

/*
 * Exact memory map of `rkos` (Apple H2 / b515d Stage-1 RTKitOS bootloader).
 * Derived from `rkos` (`fwsg` wire trailer) and `rkos.i64` (15 IDA segments).
 */

/* Program image segments */
#define RKOS_ROM_VECTORS_BASE   0x00000000u
#define RKOS_ROM_VECTORS_END    0x000000C0u
#define RKOS_ROM_VECTORS_SIZE   0x000000C0u

#define RKOS_RTKIT_HDR_BASE     0x000000C0u
#define RKOS_RTKIT_HDR_END      0x00000108u
#define RKOS_RTKIT_HDR_SIZE     0x00000048u

#define RKOS_ROM_TEXT_BASE      0x000000C0u
#define RKOS_ROM_TEXT_END       0x00013090u
#define RKOS_ROM_TEXT_SIZE      0x00012FD0u

#define RKOS_ROM_RODATA_BASE    0x00013090u
#define RKOS_ROM_RODATA_END     0x00017590u
#define RKOS_ROM_RODATA_SIZE    0x00004500u

#define RKOS_RUNTIME_VTOR_BASE  0x00013200u
#define RKOS_RUNTIME_VTOR_END   0x00013360u
#define RKOS_CRC32_TABLE_RVA    0x00014A74u
#define RKOS_SHA512_K_RVA       0x00016180u
#define RKOS_TLV_META_RVA       0x0001714Cu
#define RKOS_INIT_ARRAY_BASE    0x00017580u
#define RKOS_INIT_ARRAY_END     0x00017590u

/* Stage-2 handoff trampoline target */
#define RKOS_SRAM_TRAMP_BASE    0x01000000u
#define RKOS_SRAM_TRAMP_SIZE    0x00000200u

/* Initialized SRAM (`__DATA` in fwsg, file offset 0x20000..0x20BA4) */
#define RKOS_RAM_DATA_BASE      0x20000000u
#define RKOS_RAM_DATA_END       0x20000BA4u
#define RKOS_RAM_DATA_SIZE      0x00000BA4u
#define RKOS_RAM_DATA_FILE_OFF  0x00020000u

/* Uninitialized SRAM (`RAM_BSS`, zeroed from 0x20000BC0..0x2001F068 on cold reset) */
#define RKOS_RAM_BSS_BASE       0x20000BA4u
#define RKOS_RAM_BSS_ZERO_BASE  0x20000BC0u
#define RKOS_RAM_BSS_END        0x2001F068u
#define RKOS_RAM_BSS_SIZE       0x0001E4C4u
#define RKOS_RAM_TOTAL_MEMSZ    0x0001F068u

/* Shared crashlog handoff window above stage-1 BSS */
#define RKOS_CRASHLOG_SHM_BASE  0x20020000u

/* Key RAM_DATA landmarks */
#define RKOS_WARM_BOOT_MAGIC_VA 0x20000190u /* raw Reset_Main 0x1F8 compares this word */
#define RKOS_WARM_BOOT_ENTRY_VA 0x20000194u /* resume target when magic == RESUME */
#define RKOS_WARM_MAGIC_RESUME  0xCAFE4B0Bu /* -> bx *0x20000194 */
#define RKOS_WARM_MAGIC_BOOTED  0xBAADF1ACu /* -> beq 0x28C; written on cold boot */
#define RKOS_STACK_GUARD_VA     0x20000198u
#define RKOS_RTBUDDY_DRV_VA     0x20000240u
#define RKOS_IRQ_COUNT_VA       0x200002B8u
#define RKOS_SPI_FLASH_DRV_VA   0x20000300u
#define RKOS_REMAP_CFG_VA       0x200004C8u
#define RKOS_CRASHLOG_STATE_VA  0x20000730u
#define RKOS_CURRENT_TASK_VA    0x2000084Cu
#define RKOS_TRAMPOLINE_SRC_VA  0x20000988u

/* Key RAM_BSS state block boundaries */
#define RKOS_STATE_WARM_BASE    0x20000BA4u /* 0x001C bytes */
#define RKOS_STATE_QSPI_BASE    0x20000BC0u /* 0x0180 bytes: QSPI ctrl + flash */
#define RKOS_STATE_IRQ_BASE     0x20000D40u /* 0x0338 bytes: NMI SP, IRQ vectors, DMAdrv */
#define RKOS_STATE_META_BASE    0x20001078u /* 0x00C8 bytes: heap ptrs + 9x16B mem regions */
#define RKOS_STATE_ARENA_BASE   0x20001140u /* 0x1C000 bytes: main kernel heap arena */
#define RKOS_STATE_PSP_BASE     0x2001D140u /* 0x0800 bytes: PSP stack (top 0x2001D940) */
#define RKOS_STATE_PLAT_BASE    0x2001D940u /* 0x0390 bytes: platform drivers & heap structs */
#define RKOS_STATE_SCHED_BASE   0x2001DCD0u /* 0x0130 bytes: ready bitmap/queues, timers */
#define RKOS_STATE_FAULT_BASE   0x2001DE00u /* 0x0260 bytes: fault state, MPU shadow, remap */
#define RKOS_STATE_MSP_BASE     0x2001E060u /* 0x1008 bytes: MSP stack (top 0x2001F060) + tail */

/* Hardware MMIO apertures (from `rkos.i64` segments & `mmio_xrefs.json`) */
#define RKOS_MMIO_BOOT_BASE     0x00800000u
#define RKOS_MMIO_BOOT_SIZE     0x00001000u

#define RKOS_MMIO_TIMERS_BASE   0x40000000u
#define RKOS_MMIO_TIMERS_SIZE   0x00001000u

#define RKOS_MMIO_UART_BASE     0x40004000u
#define RKOS_MMIO_UART_SIZE     0x00001000u

#define RKOS_MMIO_QSPI_BASE     0x40007000u
#define RKOS_MMIO_QSPI_SIZE     0x00001000u

#define RKOS_MMIO_PMGR_WDT_BASE 0x4000C000u
#define RKOS_MMIO_PMGR_WDT_SIZE 0x00001000u

#define RKOS_MMIO_TRNG_BASE     0x40200000u
#define RKOS_MMIO_TRNG_SIZE     0x00004000u

#define RKOS_MMIO_SPIDMA_BASE   0x402C0000u
#define RKOS_MMIO_SPIDMA_SIZE   0x00004000u

#define RKOS_MMIO_RTBUDDY_BASE  0x40340000u
#define RKOS_MMIO_RTBUDDY_SIZE  0x00004000u

#define RKOS_MMIO_FUSE_BASE     0x40400000u
#define RKOS_MMIO_FUSE_SIZE     0x00010000u

#define RKOS_ARM_SCS_BASE       0xE000E000u
#define RKOS_ARM_SCS_SIZE       0x00001000u

/* Portable memory/MMIO accessors for freestanding ARMv7E-M vs native host tests */
#ifdef RKOS_HOST
void *rkos_host_translate(uint32_t addr, size_t size);
uint32_t rkos_host_mmio_read32(uint32_t addr);
void rkos_host_mmio_write32(uint32_t addr, uint32_t val);
uint16_t rkos_host_mmio_read16(uint32_t addr);
void rkos_host_mmio_write16(uint32_t addr, uint16_t val);
uint8_t rkos_host_mmio_read8(uint32_t addr);
void rkos_host_mmio_write8(uint32_t addr, uint8_t val);
uint32_t rkos_host_invoke_irq_cb(uint32_t fn_va, uint32_t ctx_va);
uint32_t rkos_host_invoke_fn4_cb(uint32_t fn_va, uint32_t arg0, uint32_t arg1,
                                 uint32_t arg2, uint32_t arg3);
uint32_t rkos_host_invoke_mux_write(uint32_t fn_va, uint32_t dev_va, uint32_t mode,
                                    const uint8_t *buf, uint32_t *io_len);

#define RKOS_VMA_PTR(type, addr)   ((type)rkos_host_translate((uint32_t)(addr), sizeof(*(type)0)))
#define RKOS_VMA_RAW(addr, sz)     rkos_host_translate((uint32_t)(addr), (sz))
#define RKOS_REG32_READ(addr)      rkos_host_mmio_read32((uint32_t)(addr))
#define RKOS_REG32_WRITE(addr, v)  rkos_host_mmio_write32((uint32_t)(addr), (uint32_t)(v))
#define RKOS_REG16_READ(addr)      rkos_host_mmio_read16((uint32_t)(addr))
#define RKOS_REG16_WRITE(addr, v)  rkos_host_mmio_write16((uint32_t)(addr), (uint16_t)(v))
#define RKOS_REG8_READ(addr)       rkos_host_mmio_read8((uint32_t)(addr))
#define RKOS_REG8_WRITE(addr, v)   rkos_host_mmio_write8((uint32_t)(addr), (uint8_t)(v))
#else
#define RKOS_VMA_PTR(type, addr)   ((type)(uintptr_t)(addr))
#define RKOS_VMA_RAW(addr, sz)     ((void *)(uintptr_t)(addr))
#define RKOS_REG32_READ(addr)      (*(volatile uint32_t *)(uintptr_t)(addr))
#define RKOS_REG32_WRITE(addr, v)  (*(volatile uint32_t *)(uintptr_t)(addr) = (uint32_t)(v))
#define RKOS_REG16_READ(addr)      (*(volatile uint16_t *)(uintptr_t)(addr))
#define RKOS_REG16_WRITE(addr, v)  (*(volatile uint16_t *)(uintptr_t)(addr) = (uint16_t)(v))
#define RKOS_REG8_READ(addr)       (*(volatile uint8_t *)(uintptr_t)(addr))
#define RKOS_REG8_WRITE(addr, v)   (*(volatile uint8_t *)(uintptr_t)(addr) = (uint8_t)(v))
#endif

#endif /* RKOS_MEMMAP_H */
