/*
 * host_bus.c -- sparse address model for host-side regression tests.
 *
 * The firmware addresses its peripherals and the memory it shares with the
 * host absolutely.  For tests we back the three windows the image touches with
 * ordinary memory and let BTP_PTR/BTP_REF resolve into them.  Nothing here is
 * part of the target image.
 */
#include "btp/btp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROM_WINDOW_BASE   0x00000000u
#define ROM_WINDOW_SIZE   0x00010000u
#define RAM_WINDOW_BASE   0x4D000000u
#define RAM_WINDOW_SIZE   0x00400000u
#define SCS_WINDOW_BASE   0xE0000000u
#define SCS_WINDOW_SIZE   0x00010000u

unsigned long btp_host_wfi_count(void);

static uint8_t *rom_window;
static uint8_t *ram_window;
static uint8_t *scs_window;
static uint32_t host_sysregs[CPU_HOST_REG_COUNT];
static unsigned long wfi_count;

static void *window(uint32_t address, uint32_t *offset, uint32_t *limit)
{
    if (address - ROM_WINDOW_BASE < ROM_WINDOW_SIZE) {
        *offset = address - ROM_WINDOW_BASE;
        *limit = ROM_WINDOW_SIZE;
        return rom_window;
    }
    if (address - RAM_WINDOW_BASE < RAM_WINDOW_SIZE) {
        *offset = address - RAM_WINDOW_BASE;
        *limit = RAM_WINDOW_SIZE;
        return ram_window;
    }
    if (address - SCS_WINDOW_BASE < SCS_WINDOW_SIZE) {
        *offset = address - SCS_WINDOW_BASE;
        *limit = SCS_WINDOW_SIZE;
        return scs_window;
    }
    return NULL;
}

void *btp_bus_ptr(uint32_t address, uint32_t size)
{
    uint32_t offset, limit;
    void *base = window(address, &offset, &limit);

    if (!base) {
        fprintf(stderr, "btp_bus: unmapped address 0x%08x (size %u)\n", address, size);
        abort();
    }
    if (offset + size > limit) {
        fprintf(stderr, "btp_bus: access crosses window at 0x%08x\n", address);
        abort();
    }
    return (uint8_t *)base + offset;
}

void btp_bus_reset(void)
{
    free(rom_window);
    free(ram_window);
    free(scs_window);
    rom_window = calloc(1, ROM_WINDOW_SIZE);
    ram_window = calloc(1, RAM_WINDOW_SIZE);
    scs_window = calloc(1, SCS_WINDOW_SIZE);
    if (!rom_window || !ram_window || !scs_window)
        abort();
    memset(host_sysregs, 0, sizeof host_sysregs);
    wfi_count = 0;
}

uint32_t cpu_host_sysreg_get(unsigned index)
{
    return index < CPU_HOST_REG_COUNT ? host_sysregs[index] : 0;
}

void cpu_host_sysreg_set(unsigned index, uint32_t value)
{
    if (index < CPU_HOST_REG_COUNT)
        host_sysregs[index] = value;
}

/* The firmware parks the core in WFI after a panic; under test that is a
 * failure, not a wait. */
void cpu_host_wfi(void)
{
    if (++wfi_count > 1000) {
        fprintf(stderr, "host: core halted in WFI (panic or halt path reached)\n");
        abort();
    }
}

unsigned long btp_host_wfi_count(void)
{
    return wfi_count;
}
