/*
 * bus.h — MMIO register bus accessors for ARMv7E-M hardware and host tests.
 */

#ifndef AOPF_BUS_H
#define AOPF_BUS_H

#include <stdint.h>

#if defined(__arm__) || defined(__thumb__)

static inline uint32_t bus_read32(uintptr_t addr)
{
    return *(volatile const uint32_t *)addr;
}

static inline void bus_write32(uintptr_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}

static inline uint8_t bus_read8(uintptr_t addr)
{
    return *(volatile const uint8_t *)addr;
}

static inline void bus_write8(uintptr_t addr, uint8_t val)
{
    *(volatile uint8_t *)addr = val;
}

static inline void bus_read128(uintptr_t addr, uint32_t out_words[4])
{
    register const uint32_t *src __asm__("r4") = (const uint32_t *)addr;
    register uint32_t w0 __asm__("r0");
    register uint32_t w1 __asm__("r1");
    register uint32_t w2 __asm__("r2");
    register uint32_t w3 __asm__("r3");
    __asm__ volatile(
        "ldm %[base], {%[r0], %[r1], %[r2], %[r3]}"
        : [r0] "=l"(w0), [r1] "=l"(w1), [r2] "=l"(w2), [r3] "=l"(w3)
        : [base] "l"(src)
        : "memory"
    );
    out_words[3] = w3;
    out_words[2] = w2;
    out_words[1] = w1;
    out_words[0] = w0;
}

#else /* Host test MMIO bus */

uint32_t aopf_host_bus_read32(uintptr_t addr);
void aopf_host_bus_write32(uintptr_t addr, uint32_t val);
uint8_t aopf_host_bus_read8(uintptr_t addr);
void aopf_host_bus_write8(uintptr_t addr, uint8_t val);

static inline uint32_t bus_read32(uintptr_t addr)
{
    return aopf_host_bus_read32(addr);
}

static inline void bus_write32(uintptr_t addr, uint32_t val)
{
    aopf_host_bus_write32(addr, val);
}

static inline uint8_t bus_read8(uintptr_t addr)
{
    return aopf_host_bus_read8(addr);
}

static inline void bus_write8(uintptr_t addr, uint8_t val)
{
    aopf_host_bus_write8(addr, val);
}

static inline void bus_read128(uintptr_t addr, uint32_t out_words[4])
{
    out_words[0] = aopf_host_bus_read32(addr + 0x00u);
    out_words[1] = aopf_host_bus_read32(addr + 0x04u);
    out_words[2] = aopf_host_bus_read32(addr + 0x08u);
    out_words[3] = aopf_host_bus_read32(addr + 0x0cu);
}

#endif

static inline void bus_set32(uintptr_t addr, uint32_t mask)
{
    bus_write32(addr, bus_read32(addr) | mask);
}

static inline void bus_clear32(uintptr_t addr, uint32_t mask)
{
    bus_write32(addr, bus_read32(addr) & ~mask);
}

static inline void bus_rmw32(uintptr_t addr, uint32_t clear_mask, uint32_t set_mask)
{
    bus_write32(addr, (bus_read32(addr) & ~clear_mask) | set_mask);
}

#endif /* AOPF_BUS_H */
