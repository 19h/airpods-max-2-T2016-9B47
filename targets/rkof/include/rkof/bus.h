#ifndef RKOF_BUS_H
#define RKOF_BUS_H

#include <stddef.h>
#include <stdint.h>

#if defined(RKOF_HOST_BUILD)

uint32_t rkof_host_mmio_read32(uintptr_t addr);
void rkof_host_mmio_write32(uintptr_t addr, uint32_t val);
uint16_t rkof_host_mmio_read16(uintptr_t addr);
void rkof_host_mmio_write16(uintptr_t addr, uint16_t val);
uint8_t rkof_host_mmio_read8(uintptr_t addr);
void rkof_host_mmio_write8(uintptr_t addr, uint8_t val);
void *rkof_host_translate_ptr(uintptr_t addr);

#define MMIO32_READ(addr)        rkof_host_mmio_read32((uintptr_t)(addr))
#define MMIO32_WRITE(addr, val)  rkof_host_mmio_write32((uintptr_t)(addr), (uint32_t)(val))
#define MMIO16_READ(addr)        rkof_host_mmio_read16((uintptr_t)(addr))
#define MMIO16_WRITE(addr, val)  rkof_host_mmio_write16((uintptr_t)(addr), (uint16_t)(val))
#define MMIO8_READ(addr)         rkof_host_mmio_read8((uintptr_t)(addr))
#define MMIO8_WRITE(addr, val)   rkof_host_mmio_write8((uintptr_t)(addr), (uint8_t)(val))
#define RKOF_PTR(type, addr)     ((type)rkof_host_translate_ptr((uintptr_t)(addr)))
#define RKOF_STATE(sec)
#define RKOF_USED
#define RKOF_WEAK                __attribute__((weak))
#define RKOF_NORETURN            __attribute__((noreturn))

#else /* ARMv7E-M freestanding build */

#define MMIO32(addr)             (*(volatile uint32_t *)(uintptr_t)(addr))
#define MMIO16(addr)             (*(volatile uint16_t *)(uintptr_t)(addr))
#define MMIO8(addr)              (*(volatile uint8_t *)(uintptr_t)(addr))

#define MMIO32_READ(addr)        MMIO32(addr)
#define MMIO32_WRITE(addr, val)  (MMIO32(addr) = (uint32_t)(val))
#define MMIO16_READ(addr)        MMIO16(addr)
#define MMIO16_WRITE(addr, val)  (MMIO16(addr) = (uint16_t)(val))
#define MMIO8_READ(addr)         MMIO8(addr)
#define MMIO8_WRITE(addr, val)   (MMIO8(addr) = (uint8_t)(val))
#define RKOF_PTR(type, addr)     ((type)(uintptr_t)(addr))
#define RKOF_STATE(sec)          __attribute__((section(sec), used))
#define RKOF_USED                __attribute__((used))
#define RKOF_WEAK                __attribute__((weak))
#define RKOF_NORETURN            __attribute__((noreturn))

#endif /* RKOF_HOST_BUILD */

static inline void mmio32_set_bits(uintptr_t addr, uint32_t mask)
{
    MMIO32_WRITE(addr, MMIO32_READ(addr) | mask);
}

static inline void mmio32_clear_bits(uintptr_t addr, uint32_t mask)
{
    MMIO32_WRITE(addr, MMIO32_READ(addr) & ~mask);
}

static inline void mmio32_rmw_mask(uintptr_t addr, uint32_t clear_mask, uint32_t set_mask)
{
    MMIO32_WRITE(addr, (MMIO32_READ(addr) & ~clear_mask) | set_mask);
}

#endif /* RKOF_BUS_H */
