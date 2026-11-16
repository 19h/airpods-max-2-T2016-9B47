#ifndef ACIB_BUS_H
#define ACIB_BUS_H

#include <stdint.h>

/*
 * Hardware MMIO and address-mapped shared memory accessors.
 *
 * On ARMv7E-M target builds, these compile to single volatile LDR/STR
 * instructions matching raw firmware semantics.
 * On ACIB_HOST_TEST builds, reads/writes route through tests/host_bus.c so
 * unit tests can observe MMIO transactions, W1C handshakes, and 0x4D280000
 * shared mailbox memory deterministically.
 */

#if defined(ACIB_HOST_TEST)

uint32_t acib_host_mmio_read32(uint32_t addr);
void     acib_host_mmio_write32(uint32_t addr, uint32_t val);
uint16_t acib_host_mmio_read16(uint32_t addr);
void     acib_host_mmio_write16(uint32_t addr, uint16_t val);
uint8_t  acib_host_mmio_read8(uint32_t addr);
void     acib_host_mmio_write8(uint32_t addr, uint8_t val);
void    *acib_host_translate_ptr(uint32_t addr);

static inline uint32_t acib_mmio_read32(uint32_t addr)
{
    return acib_host_mmio_read32(addr);
}

static inline void acib_mmio_write32(uint32_t addr, uint32_t val)
{
    acib_host_mmio_write32(addr, val);
}

static inline uint16_t acib_mmio_read16(uint32_t addr)
{
    return acib_host_mmio_read16(addr);
}

static inline void acib_mmio_write16(uint32_t addr, uint16_t val)
{
    acib_host_mmio_write16(addr, val);
}

static inline uint8_t acib_mmio_read8(uint32_t addr)
{
    return acib_host_mmio_read8(addr);
}

static inline void acib_mmio_write8(uint32_t addr, uint8_t val)
{
    acib_host_mmio_write8(addr, val);
}

static inline void *acib_addr_ptr(uint32_t addr)
{
    return acib_host_translate_ptr(addr);
}

#else

static inline uint32_t acib_mmio_read32(uint32_t addr)
{
    return *(volatile uint32_t *)(uintptr_t)addr;
}

static inline void acib_mmio_write32(uint32_t addr, uint32_t val)
{
    *(volatile uint32_t *)(uintptr_t)addr = val;
}

static inline uint16_t acib_mmio_read16(uint32_t addr)
{
    return *(volatile uint16_t *)(uintptr_t)addr;
}

static inline void acib_mmio_write16(uint32_t addr, uint16_t val)
{
    *(volatile uint16_t *)(uintptr_t)addr = val;
}

static inline uint8_t acib_mmio_read8(uint32_t addr)
{
    return *(volatile uint8_t *)(uintptr_t)addr;
}

static inline void acib_mmio_write8(uint32_t addr, uint8_t val)
{
    *(volatile uint8_t *)(uintptr_t)addr = val;
}

static inline void *acib_addr_ptr(uint32_t addr)
{
    return (void *)(uintptr_t)addr;
}

#endif /* ACIB_HOST_TEST */

static inline uint32_t acib_read_unaligned_le32(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint32_t)b[0] |
           ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

static inline void acib_write_unaligned_le32(void *p, uint32_t v)
{
    uint8_t *b = (uint8_t *)p;
    b[0] = (uint8_t)(v & 0xFFu);
    b[1] = (uint8_t)((v >> 8) & 0xFFu);
    b[2] = (uint8_t)((v >> 16) & 0xFFu);
    b[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static inline uint16_t acib_read_unaligned_le16(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}

static inline void acib_write_unaligned_le16(void *p, uint16_t v)
{
    uint8_t *b = (uint8_t *)p;
    b[0] = (uint8_t)(v & 0xFFu);
    b[1] = (uint8_t)((v >> 8) & 0xFFu);
}

#endif /* ACIB_BUS_H */
