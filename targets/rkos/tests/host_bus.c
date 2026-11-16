#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_bus.h"

/*
 * Simulated ARMv7E-M VMA & MMIO bus for native 64-bit host behavioral testing
 * (`make -C targets/rkos check`).
 *
 * Maps:
 *   0x00000000..0x00017590 : Firmware ROM image (`.vectors` + `.rtkit.header` + `.rodata`)
 *   0x01000000..0x01000200 : Stage-2 SRAM trampoline window
 *   0x02000000..0x02010000 : Host scratch test window (for DER/BMID/stream fixtures)
 *   0x20000000..0x20000BA4 : Mutable copy of firmware `RAM_DATA` (`g_rkos_data`)
 *   0x20000BA4..0x2001F068 : Contiguous `RAM_BSS` backed by the 10 `g_state_*` blocks
 *   0x20020000..0x20028000 : Shared stage-1 -> stage-2 crashlog/BMID SRAM window
 *   0x00800000, 0x40000000..0x40410000, 0xE000E000..0xE000F000 : Simulated MMIO & NVIC/SCB
 */

#define HOST_SCRATCH_BASE 0x02000000u
#define HOST_SCRATCH_SIZE 0x00010000u
#define HOST_SHM_BASE     0x20020000u
#define HOST_SHM_SIZE     0x00008000u
#define HOST_CB_BASE      0x00018001u
#define HOST_CB_MAX       16u

static uint8_t g_host_rom[RKOS_ROM_RODATA_END];
static uint8_t g_host_sram_tramp[RKOS_SRAM_TRAMP_SIZE];
static uint8_t g_host_scratch[HOST_SCRATCH_SIZE];
static uint8_t g_host_ram_data_init[RKOS_RAM_DATA_SIZE];
static bool    g_host_init_captured = false;
static uint8_t g_host_shm[HOST_SHM_SIZE];

static uint32_t g_scs_words[RKOS_ARM_SCS_SIZE / 4u];
static uint32_t g_boot_remap_lock;
static uint32_t g_fuse_cfg0;
static uint32_t g_fuse_cfg1;
static uint32_t g_fuse_boot_mode;

static uint32_t g_cpu_ipsr;
static uint32_t g_cpu_primask;
static uint32_t g_cpu_basepri;
static uint32_t g_cpu_control;
static uint32_t g_cpu_msp;
static uint32_t g_cpu_psp;
static uint32_t g_wfi_cnt;
static uint32_t g_svc_cnt;

static char     g_uart_log[4096];
static size_t   g_uart_log_len;

static jmp_buf *g_trap_jmp;
static char     g_last_panic[256];
static uint32_t g_last_halt;

static rkos_host_irq_fn_t g_host_cbs[HOST_CB_MAX];
static uint32_t           g_host_cb_count;

typedef struct {
    uint32_t base;
    uint32_t size;
    void    *ptr;
} host_region_map_t;

static host_region_map_t g_bss_maps[10];

static void init_bss_maps(void)
{
    g_bss_maps[0] = (host_region_map_t){ RKOS_STATE_WARM_BASE,  sizeof(g_state_warm),  &g_state_warm };
    g_bss_maps[1] = (host_region_map_t){ RKOS_STATE_QSPI_BASE,  sizeof(g_state_qspi),  &g_state_qspi };
    g_bss_maps[2] = (host_region_map_t){ RKOS_STATE_IRQ_BASE,   sizeof(g_state_irq),   &g_state_irq };
    g_bss_maps[3] = (host_region_map_t){ RKOS_STATE_META_BASE,  sizeof(g_state_meta),  &g_state_meta };
    g_bss_maps[4] = (host_region_map_t){ RKOS_STATE_ARENA_BASE, sizeof(g_state_arena), &g_state_arena };
    g_bss_maps[5] = (host_region_map_t){ RKOS_STATE_PSP_BASE,   sizeof(g_state_psp),   &g_state_psp };
    g_bss_maps[6] = (host_region_map_t){ RKOS_STATE_PLAT_BASE,  sizeof(g_state_plat),  &g_state_plat };
    g_bss_maps[7] = (host_region_map_t){ RKOS_STATE_SCHED_BASE, sizeof(g_state_sched), &g_state_sched };
    g_bss_maps[8] = (host_region_map_t){ RKOS_STATE_FAULT_BASE, sizeof(g_state_fault), &g_state_fault };
    g_bss_maps[9] = (host_region_map_t){ RKOS_STATE_MSP_BASE,   sizeof(g_state_msp),   &g_state_msp };
}

void rkos_host_bus_reset(void)
{
    if (!g_host_init_captured) {
        memcpy(g_host_ram_data_init, g_rkos_data, RKOS_RAM_DATA_SIZE);
        g_host_init_captured = true;
    }

    memset(g_host_rom, 0, sizeof(g_host_rom));
    memcpy(g_host_rom + RKOS_RTKIT_HDR_BASE, g_rkos_rtkit_hdr, RKOS_RTKIT_HDR_SIZE);
    memcpy(g_host_rom + RKOS_ROM_RODATA_BASE, g_rkos_rodata, RKOS_ROM_RODATA_SIZE);

    memcpy(g_rkos_data, g_host_ram_data_init, RKOS_RAM_DATA_SIZE);
    memset(g_host_sram_tramp, 0, sizeof(g_host_sram_tramp));
    memset(g_host_scratch, 0, sizeof(g_host_scratch));
    memset(g_host_shm, 0, sizeof(g_host_shm));

    init_bss_maps();
    for (size_t i = 0u; i < 10u; i++) {
        memset(g_bss_maps[i].ptr, 0, g_bss_maps[i].size);
    }

    memset(g_scs_words, 0, sizeof(g_scs_words));
    g_scs_words[(RKOS_MPU_TYPE - RKOS_ARM_SCS_BASE) / 4u] = 0x00000800u; /* 8 MPU regions */

    g_boot_remap_lock = 0u;
    g_fuse_cfg0 = 0x00002016u;
    g_fuse_cfg1 = 0x000000B0u;
    g_fuse_boot_mode = 0x00000001u;

    g_cpu_ipsr = 0u;
    g_cpu_primask = 0u;
    g_cpu_basepri = 0u;
    g_cpu_control = 2u;
    g_cpu_msp = 0x2001F060u;
    g_cpu_psp = 0x2001D940u;
    g_wfi_cnt = 0u;
    g_svc_cnt = 0u;

    memset(g_uart_log, 0, sizeof(g_uart_log));
    g_uart_log_len = 0u;

    g_trap_jmp = NULL;
    memset(g_last_panic, 0, sizeof(g_last_panic));
    g_last_halt = 0u;
    memset(g_host_cbs, 0, sizeof(g_host_cbs));
    g_host_cb_count = 0u;
}

void *rkos_host_translate(uint32_t addr, size_t size)
{
    if (size == 0u) {
        size = 1u;
    }
    if ((uint64_t)addr + size <= RKOS_ROM_RODATA_END) {
        return g_host_rom + addr;
    }
    if (addr >= RKOS_SRAM_TRAMP_BASE &&
        (uint64_t)addr + size <= (uint64_t)RKOS_SRAM_TRAMP_BASE + RKOS_SRAM_TRAMP_SIZE) {
        return g_host_sram_tramp + (addr - RKOS_SRAM_TRAMP_BASE);
    }
    if (addr >= HOST_SCRATCH_BASE &&
        (uint64_t)addr + size <= (uint64_t)HOST_SCRATCH_BASE + HOST_SCRATCH_SIZE) {
        return g_host_scratch + (addr - HOST_SCRATCH_BASE);
    }
    if (addr >= RKOS_RAM_DATA_BASE &&
        (uint64_t)addr + size <= RKOS_RAM_DATA_END) {
        return g_rkos_data + (addr - RKOS_RAM_DATA_BASE);
    }
    for (size_t i = 0u; i < 10u; i++) {
        uint32_t b = g_bss_maps[i].base;
        uint32_t e = b + g_bss_maps[i].size;
        if (addr >= b && (uint64_t)addr + size <= e) {
            return (uint8_t *)g_bss_maps[i].ptr + (addr - b);
        }
    }
    if (addr >= HOST_SHM_BASE &&
        (uint64_t)addr + size <= (uint64_t)HOST_SHM_BASE + HOST_SHM_SIZE) {
        return g_host_shm + (addr - HOST_SHM_BASE);
    }

    fprintf(stderr, "[host_bus] unmapped VMA access addr=0x%08x size=%zu\n", addr, size);
    abort();
}

uint32_t rkos_host_mmio_read32(uint32_t addr)
{
    if (addr == RKOS_BOOT_REMAP_LOCK) {
        return g_boot_remap_lock;
    }
    if (addr == RKOS_FUSE_CHIP_CFG0) {
        return g_fuse_cfg0;
    }
    if (addr == RKOS_FUSE_CHIP_CFG1) {
        return g_fuse_cfg1;
    }
    if (addr == RKOS_FUSE_BOOT_MODE) {
        return g_fuse_boot_mode;
    }
    if (addr >= RKOS_MMIO_UART_BASE && addr < RKOS_MMIO_UART_BASE + RKOS_MMIO_UART_SIZE) {
        if (addr == RKOS_MMIO_UART_BASE + 0x10u) {
            return 0x06u; /* TX FIFO ready */
        }
        return 0u;
    }
    if (addr >= RKOS_ARM_SCS_BASE && addr + 4u <= RKOS_ARM_SCS_BASE + RKOS_ARM_SCS_SIZE) {
        uint32_t off = (addr - RKOS_ARM_SCS_BASE) / 4u;
        return g_scs_words[off];
    }
    return 0u;
}

void rkos_host_mmio_write32(uint32_t addr, uint32_t val)
{
    if (addr == RKOS_BOOT_REMAP_LOCK) {
        g_boot_remap_lock = val;
        return;
    }
    if (addr >= RKOS_MMIO_UART_BASE && addr < RKOS_MMIO_UART_BASE + RKOS_MMIO_UART_SIZE) {
        if (addr == RKOS_MMIO_UART_BASE + 0x20u && g_uart_log_len + 1u < sizeof(g_uart_log)) {
            g_uart_log[g_uart_log_len++] = (char)(val & 0xFFu);
            g_uart_log[g_uart_log_len] = '\0';
        }
        return;
    }
    if (addr >= RKOS_ARM_SCS_BASE && addr + 4u <= RKOS_ARM_SCS_BASE + RKOS_ARM_SCS_SIZE) {
        /* Model NVIC write-1-to-set / write-1-to-clear semantics! */
        if (addr >= RKOS_NVIC_ISER_BASE && addr < RKOS_NVIC_ISER_BASE + 0x20u) {
            uint32_t idx = (addr - RKOS_ARM_SCS_BASE) / 4u;
            g_scs_words[idx] |= val;
            return;
        }
        if (addr >= RKOS_NVIC_ICER_BASE && addr < RKOS_NVIC_ICER_BASE + 0x20u) {
            uint32_t iser_idx = ((addr - RKOS_NVIC_ICER_BASE) + (RKOS_NVIC_ISER_BASE - RKOS_ARM_SCS_BASE)) / 4u;
            g_scs_words[iser_idx] &= ~val;
            return;
        }
        if (addr >= RKOS_NVIC_ISPR_BASE && addr < RKOS_NVIC_ISPR_BASE + 0x20u) {
            uint32_t idx = (addr - RKOS_ARM_SCS_BASE) / 4u;
            g_scs_words[idx] |= val;
            return;
        }
        if (addr >= RKOS_NVIC_ICPR_BASE && addr < RKOS_NVIC_ICPR_BASE + 0x20u) {
            uint32_t ispr_idx = ((addr - RKOS_NVIC_ICPR_BASE) + (RKOS_NVIC_ISPR_BASE - RKOS_ARM_SCS_BASE)) / 4u;
            g_scs_words[ispr_idx] &= ~val;
            return;
        }
        if (addr == RKOS_SCB_ICSR) {
            uint32_t idx = (RKOS_SCB_ICSR - RKOS_ARM_SCS_BASE) / 4u;
            if ((val & RKOS_ICSR_PENDSTSET) != 0u) {
                g_scs_words[idx] |= RKOS_ICSR_PENDSTSET;
            }
            if ((val & RKOS_ICSR_PENDSTCLR) != 0u) {
                g_scs_words[idx] &= ~RKOS_ICSR_PENDSTSET;
            }
            if ((val & RKOS_ICSR_PENDSVSET) != 0u) {
                g_scs_words[idx] |= RKOS_ICSR_PENDSVSET;
            }
            if ((val & RKOS_ICSR_PENDSVCLR) != 0u) {
                g_scs_words[idx] &= ~RKOS_ICSR_PENDSVSET;
            }
            return;
        }
        g_scs_words[(addr - RKOS_ARM_SCS_BASE) / 4u] = val;
    }
}

uint8_t rkos_host_mmio_read8(uint32_t addr)
{
    uint32_t aligned = addr & ~3u;
    uint32_t shift = (addr & 3u) * 8u;
    return (uint8_t)((rkos_host_mmio_read32(aligned) >> shift) & 0xFFu);
}

void rkos_host_mmio_write8(uint32_t addr, uint8_t val)
{
    uint32_t aligned = addr & ~3u;
    uint32_t shift = (addr & 3u) * 8u;
    uint32_t cur = rkos_host_mmio_read32(aligned);
    uint32_t next = (cur & ~(0xFFu << shift)) | ((uint32_t)val << shift);
    rkos_host_mmio_write32(aligned, next);
}

uint32_t rkos_arch_get_ipsr(void)    { return g_cpu_ipsr; }
uint32_t rkos_arch_get_primask(void) { return g_cpu_primask; }
void     rkos_arch_set_primask(uint32_t val) { g_cpu_primask = val & 1u; }
uint32_t rkos_arch_get_basepri(void) { return g_cpu_basepri; }
void     rkos_arch_set_basepri(uint32_t val) { g_cpu_basepri = val & 0xFFu; }
uint32_t rkos_arch_get_control(void) { return g_cpu_control; }
uint32_t rkos_arch_get_msp(void)     { return g_cpu_msp; }
uint32_t rkos_arch_get_psp(void)     { return g_cpu_psp; }
void     rkos_arch_dsb(void)         {}
void     rkos_arch_isb(void)         {}
void     rkos_arch_wfi(void)         { g_wfi_cnt++; }
void     rkos_arch_svc0(void)        { g_svc_cnt++; }

void rkos_host_set_ipsr(uint32_t ipsr) { g_cpu_ipsr = ipsr; }
void rkos_host_set_psp(uint32_t psp)   { g_cpu_psp = psp; }
void rkos_host_set_msp(uint32_t msp)   { g_cpu_msp = msp; }

uint32_t rkos_host_register_cb(rkos_host_irq_fn_t fn)
{
    if (g_host_cb_count >= HOST_CB_MAX) {
        abort();
    }
    uint32_t idx = g_host_cb_count++;
    g_host_cbs[idx] = fn;
    return HOST_CB_BASE + idx * 4u;
}

uint32_t rkos_host_invoke_irq_cb(uint32_t fn_va, uint32_t ctx_va)
{
    if (fn_va >= HOST_CB_BASE) {
        uint32_t idx = (fn_va - HOST_CB_BASE) / 4u;
        if (idx < g_host_cb_count && g_host_cbs[idx] != NULL) {
            return (uint32_t)g_host_cbs[idx](ctx_va);
        }
    }
    return 0xFFFFFFFFu;
}

void rkos_host_arm_trap(jmp_buf *jb)  { g_trap_jmp = jb; }
void rkos_host_disarm_trap(void)      { g_trap_jmp = NULL; }
const char *rkos_host_last_panic_msg(void) { return g_last_panic; }
uint32_t rkos_host_last_halt_code(void)    { return g_last_halt; }
uint32_t rkos_host_wfi_count(void)         { return g_wfi_cnt; }
uint32_t rkos_host_svc_count(void)         { return g_svc_cnt; }
const char *rkos_host_uart_tx_log(void)    { return g_uart_log; }

void rkos_host_on_panic(const char *msg)
{
    snprintf(g_last_panic, sizeof(g_last_panic), "%s", (msg != NULL) ? msg : "<null>");
}

void rkos_host_on_halt(uint32_t code)
{
    g_last_halt = code;
    if (g_trap_jmp != NULL) {
        jmp_buf *jb = g_trap_jmp;
        g_trap_jmp = NULL;
        longjmp(*jb, 1);
    }
    fprintf(stderr, "[host_bus] unhandled firmware halt code=%u panic='%s'\n",
            code, g_last_panic);
    abort();
}
