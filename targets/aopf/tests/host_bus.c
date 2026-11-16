/*
 * host_bus.c — Deterministic simulated MMIO register bus, CPU special-register
 * state, and VMA-to-host-memory translation for native host behavioral tests.
 */

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "aopf/bus.h"
#include "aopf/cpu.h"
#include "aopf/memmap.h"

extern const uint8_t g_rtkit_image_header[0x48];
extern const uint8_t g_rom_rodata_bank[0x13b14];
extern uint8_t g_ram_data_bank[0x1a0b0];

/* Simulated AOP SRAM (0x20800000..0x2089BED8) */
static uint8_t g_host_sram[AOPF_RAM_TOTAL_SIZE];
static int g_sram_seeded;

/* Simulated MMIO register table (sparse key-value array) */
#define HOST_MMIO_MAX_SLOTS 256u

typedef struct host_mmio_slot {
    uintptr_t addr;
    uint32_t  val;
    int       used;
} host_mmio_slot_t;

static host_mmio_slot_t g_mmio_slots[HOST_MMIO_MAX_SLOTS];
static uint32_t g_host_primask;
static uint32_t g_host_basepri;
static uint32_t g_host_ipsr;
static uint32_t g_host_msp = AOPF_MSP_STACK_TOP;

static int g_expect_panic;
static jmp_buf g_panic_jmp;
static char g_recorded_panic[160];

void aopf_host_reset_env(void);
void aopf_host_set_ipsr(uint32_t exc_num);
void aopf_host_set_msp(uint32_t msp_val);
void aopf_host_record_panic(const char *msg);
const char *aopf_host_last_panic(void);
jmp_buf *aopf_host_arm_panic_trap(void);
void aopf_host_disarm_panic_trap(void);

void aopf_host_reset_env(void)
{
    memset(g_mmio_slots, 0, sizeof(g_mmio_slots));
    memcpy(g_host_sram, g_ram_data_bank, AOPF_RAM_DATA_SIZE);
    memset(g_host_sram + AOPF_RAM_DATA_SIZE, 0, AOPF_RAM_TOTAL_SIZE - AOPF_RAM_DATA_SIZE);
    g_sram_seeded = 1;
    g_host_primask = 0u;
    g_host_basepri = 0u;
    g_host_ipsr = 0u;
    g_host_msp = AOPF_MSP_STACK_TOP;
    g_expect_panic = 0;
    g_recorded_panic[0] = '\0';
}

uint32_t aopf_host_get_primask(void) { return g_host_primask; }
void aopf_host_set_primask(uint32_t reg_val) { g_host_primask = reg_val & 1u; }
uint32_t aopf_host_get_basepri(void) { return g_host_basepri; }
void aopf_host_set_basepri(uint32_t reg_val) { g_host_basepri = reg_val & 0xffu; }
uint32_t aopf_host_get_ipsr(void) { return g_host_ipsr; }
void aopf_host_set_ipsr(uint32_t exc_num) { g_host_ipsr = exc_num; }
uint32_t aopf_host_get_msp(void) { return g_host_msp; }
void aopf_host_set_msp(uint32_t msp_val) { g_host_msp = msp_val; }

void *aopf_host_translate_vma(uint32_t vma_addr)
{
    if (!g_sram_seeded) {
        aopf_host_reset_env();
    }
    if (vma_addr >= AOPF_ROM_HEADER_BASE && vma_addr < AOPF_ROM_HEADER_END) {
        return (void *)(uintptr_t)&g_rtkit_image_header[vma_addr - AOPF_ROM_HEADER_BASE];
    }
    if (vma_addr >= AOPF_ROM_RODATA_BASE && vma_addr < AOPF_ROM_RODATA_END) {
        return (void *)(uintptr_t)&g_rom_rodata_bank[vma_addr - AOPF_ROM_RODATA_BASE];
    }
    if (vma_addr >= AOPF_RAM_BASE && vma_addr < AOPF_RAM_END) {
        return &g_host_sram[vma_addr - AOPF_RAM_BASE];
    }
    return NULL;
}

uint32_t aopf_host_bus_read32(uintptr_t addr)
{
    for (size_t idx = 0u; idx < HOST_MMIO_MAX_SLOTS; idx++) {
        if (g_mmio_slots[idx].used && g_mmio_slots[idx].addr == addr) {
            return g_mmio_slots[idx].val;
        }
    }
    return 0u;
}

void aopf_host_bus_write32(uintptr_t addr, uint32_t val)
{
    for (size_t idx = 0u; idx < HOST_MMIO_MAX_SLOTS; idx++) {
        if (g_mmio_slots[idx].used && g_mmio_slots[idx].addr == addr) {
            g_mmio_slots[idx].val = val;
            return;
        }
    }
    for (size_t idx = 0u; idx < HOST_MMIO_MAX_SLOTS; idx++) {
        if (!g_mmio_slots[idx].used) {
            g_mmio_slots[idx].used = 1;
            g_mmio_slots[idx].addr = addr;
            g_mmio_slots[idx].val = val;
            return;
        }
    }
}

uint8_t aopf_host_bus_read8(uintptr_t addr)
{
    uintptr_t word_addr = addr & ~(uintptr_t)3u;
    uint32_t shift = (uint32_t)(addr & 3u) * 8u;
    return (uint8_t)((aopf_host_bus_read32(word_addr) >> shift) & 0xffu);
}

void aopf_host_bus_write8(uintptr_t addr, uint8_t val)
{
    uintptr_t word_addr = addr & ~(uintptr_t)3u;
    uint32_t shift = (uint32_t)(addr & 3u) * 8u;
    uint32_t word_val = aopf_host_bus_read32(word_addr);
    word_val = (word_val & ~(0xffu << shift)) | ((uint32_t)val << shift);
    aopf_host_bus_write32(word_addr, word_val);
}

jmp_buf *aopf_host_arm_panic_trap(void)
{
    g_expect_panic = 1;
    g_recorded_panic[0] = '\0';
    return &g_panic_jmp;
}

void aopf_host_disarm_panic_trap(void)
{
    g_expect_panic = 0;
}

const char *aopf_host_last_panic(void)
{
    return g_recorded_panic;
}

void aopf_host_record_panic(const char *msg)
{
    if (msg != NULL) {
        strncpy(g_recorded_panic, msg, sizeof(g_recorded_panic) - 1u);
        g_recorded_panic[sizeof(g_recorded_panic) - 1u] = '\0';
    }
    if (g_expect_panic) {
        g_expect_panic = 0;
        longjmp(g_panic_jmp, 1);
    }
}
