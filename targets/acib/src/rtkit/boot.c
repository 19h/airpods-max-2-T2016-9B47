#include "acib/rtkit.h"
#include "acib/aci.h"
#include "acib/bus.h"
#include "acib/libc.h"
#include "acib/memmap.h"
#include "acib/regs.h"

/*
 * Early hardware boot, MPU 7-region setup (including 0x4D280000 MAC<->PHY
 * shared mailbox), DWT/BT-timer CPU frequency calibration, board property
 * loading, and stack watermark routines reconstructed from acib ROM.
 */

#define ACIB_RAM_WAKE_COUNT_ADDR     0x10000064u
#define ACIB_RAM_SLEEP_COUNT_ADDR    0x10000068u

#define ACIB_ROM_TAG_BTM1_VAL_ADDR   0x000F966Fu
#define ACIB_ROM_TAG_BTM2_VAL_ADDR   0x000F967Bu
#define ACIB_ROM_TAG_FPGA_VAL_ADDR   0x000F9687u
#define ACIB_ROM_TAG_DEVF_VAL_ADDR   0x000F9693u

#if !defined(ACIB_HOST_TEST)
/* Relocated ARMv7E-M reset entry shim (RVA 0x000108, tracked as pending_c in ledger.json) */
ACIB_SECTION(".pinned.reset") ACIB_NAKED ACIB_USED
void Reset_Handler(void)
{
    __asm__ volatile(
        "ldr     r1, =0x100016e4\n\t"
        "ldr     r1, [r1]\n\t"
        "movw    r0, #0x4b0b\n\t"
        "movt    r0, #0xcafe\n\t"
        "cmp     r0, r1\n\t"
        "ittt    eq\n\t"
        "ldreq   r0, =0x10000078\n\t"
        "ldreq   r0, [r0]\n\t"
        "bxeq    r0\n\t"
        "cpsid   i\n\t"
        "ldr     r0, =0x000f52e0\n\t"
        "ldr     r0, [r0]\n\t"
        "msr     msp, r0\n\t"
        "ldr     r0, =0x000f52d4\n\t"
        "ldr     r0, [r0]\n\t"
        "msr     psp, r0\n\t"
        "mov.w   r0, #2\n\t"
        "msr     control, r0\n\t"
        "isb     sy\n\t"
        "ldr     r7, =0x77777700\n\t"
        "ldr     r0, =0x1000ac40\n\t"
        "ldr     r1, =0x1002d2c4\n\t"
        "mov.w   r2, #0\n\t"
        "1:\n\t"
        "cmp     r0, r1\n\t"
        "bhs     2f\n\t"
        "str     r2, [r0], #4\n\t"
        "b       1b\n\t"
        "2:\n\t"
        "bl      rtk_early_hw_init\n\t"
        "mrs     r0, control\n\t"
        "orr     r0, r0, #2\n\t"
        "msr     control, r0\n\t"
        "bl      main_entry\n\t"
        "3:\n\t"
        "b       3b\n\t"
    );
}
#else
void Reset_Handler(void)
{
}
#endif

/* @provenance RVA 0x0706d0 sub_706D0 */
uint32_t acib_mpu_default_attr(void)
{
    return 0u;
}

/* @provenance RVA 0x0706d8 sub_706D8 */
uint32_t acib_mpu_enable(void)
{
    uint32_t ctrl = acib_mmio_read32(ACIB_MPU_CTRL_ADDR);
    acib_mmio_write32(ACIB_MPU_CTRL_ADDR, ctrl | ACIB_MPU_CTRL_ENABLE);
    return ACIB_MPU_CTRL_ADDR;
}

/* @provenance RVA 0x0706e8 sub_706E8 */
uint32_t acib_mpu_disable(void)
{
    uint32_t ctrl = acib_mmio_read32(ACIB_MPU_CTRL_ADDR);
    acib_mmio_write32(ACIB_MPU_CTRL_ADDR, ctrl & ~ACIB_MPU_CTRL_ENABLE);
    return ACIB_MPU_CTRL_ADDR;
}

/* @provenance RVA 0x0706f8 sub_706F8 */
uint32_t acib_mpu_config_region(uint32_t region, uint32_t base, uint32_t size_order,
                                uint32_t srd, uint32_t tex_scb, uint32_t xn, uint32_t ap)
{
    acib_mmio_write32(ACIB_MPU_RNR_ADDR, region);
    acib_mmio_write32(ACIB_MPU_RASR_ADDR, 0u);
    acib_mmio_write32(ACIB_MPU_RBAR_ADDR, base & 0xFFFFFFE0u);

    uint32_t attr = (tex_scb & 0x003F0000u) | (ap << 24) | (xn << 28);
    acib_mmio_write32(ACIB_MPU_RASR_ADDR, attr);

    uint32_t rasr = acib_mmio_read32(ACIB_MPU_RASR_ADDR) |
                    ((size_order & 0x1Fu) << 1) |
                    ((srd & 0xFFu) << 8) |
                    1u;
    acib_mmio_write32(ACIB_MPU_RASR_ADDR, rasr);
    return rasr;
}

/* @provenance RVA 0x07deb8 rtk_early_hw_init */
void rtk_early_hw_init(void)
{
    acib_mpu_disable();
    acib_mpu_config_region(0u, ACIB_MPU_R0_PERIPH_BASE,   0x1Cu, 0u, 0x00000000u, 1u, 1u);
    acib_mpu_config_region(1u, ACIB_MPU_R1_ROM_BASE,      0x13u, 0u, 0x00220000u, 0u, 5u);
    acib_mpu_config_region(2u, ACIB_MPU_R2_SRAM_BASE,     0x11u, 0u, 0x00200000u, 1u, 3u);
    acib_mpu_config_region(3u, ACIB_MPU_R3_BT_SHM_BASE,   0x0Fu, 0u, 0x00200000u, 1u, 1u);
    acib_mpu_config_region(4u, ACIB_MPU_R4_SOC_SHM_BASE,  0x17u, 0u, 0x00200000u, 1u, 1u);
    acib_mpu_config_region(5u, ACIB_MPU_R5_PHY_MBOX_BASE, 0x0Bu, 0u, 0x00200000u, 1u, 1u);
    acib_mpu_config_region(6u, ACIB_MPU_R6_ROM_HI_BASE,   0x0Fu, 0u, 0x00200000u, 0u, 1u);
    acib_mpu_enable();
}

/* @provenance RVA 0x070734 sub_70734 */
uint32_t acib_dwt_cyccnt_read(void)
{
    return acib_mmio_read32(ACIB_DWT_CYCCNT_ADDR);
}

/* @provenance RVA 0x0707a4 sub_707A4 */
uint32_t acib_measure_cpu_hz(uint32_t timer_base_addr)
{
    uint32_t us_reg = timer_base_addr + 0x20u;
    uint32_t t0 = acib_mmio_read32(us_reg);
    uint32_t t_edge;
    uint32_t cyc_start;

    do {
        t_edge = acib_mmio_read32(us_reg);
        cyc_start = acib_mmio_read32(ACIB_DWT_CYCCNT_ADDR);
    } while (t_edge == t0);

    uint32_t us_delta;
    uint32_t cyc_end;
    do {
        uint32_t t_now = acib_mmio_read32(us_reg);
        cyc_end = acib_mmio_read32(ACIB_DWT_CYCCNT_ADDR);
        us_delta = t_now - t_edge;
    } while (us_delta < 20u);

    uint32_t cyc_delta = cyc_end - cyc_start;
    uint64_t numer = ((uint64_t)cyc_delta * 1000000ULL) + (uint64_t)(us_delta >> 1);
    uint32_t hz = (uint32_t)(numer / (uint64_t)us_delta);
    uint32_t fallback = g_acib_clock_ctx.fallback_cpu_hz;
    return (hz != 0u) ? hz : fallback;
}

/* @provenance RVA 0x070788 sub_70788 */
uint32_t acib_get_calibrated_cpu_hz(void)
{
    uint32_t us_reg = ACIB_BT_TIMER_US_ADDR;
    uint32_t initial = acib_mmio_read32(us_reg);

    for (uint32_t tries = 0xB6u; tries > 0u; tries--) {
        if (acib_mmio_read32(us_reg) != initial) {
            break;
        }
    }
    if (acib_mmio_read32(us_reg) != initial) {
        return acib_measure_cpu_hz(ACIB_BT_TIMER_BASE);
    }
    return g_acib_clock_ctx.fallback_cpu_hz;
}

/* @provenance RVA 0x07de44 sub_7DE44 */
void acib_clear_sleep_deadline(void)
{
    g_acib_clock_ctx.sleep_state = 11u;
}

/* @provenance RVA 0x07de50 sub_7DE50 */
void *acib_clock_timing_init(void)
{
    g_acib_clock_ctx.ref_clk_hz = 40000000u; /* 0x02625A00 */
    uint32_t cpu_hz = acib_get_calibrated_cpu_hz();
    g_acib_clock_ctx.calibrated_cpu_hz = cpu_hz;
    g_acib_clock_ctx.inv_clk_ratio = 160000000u / (cpu_hz - 5000000u);
    g_acib_clock_ctx.cycles_per_us = 2u * ((cpu_hz + 500000u) / 2000000u);

    bool is_low_freq = (cpu_hz + 500000u) < 120000000u;
    g_acib_clock_ctx.low_freq_mode = is_low_freq ? 1u : 0u;
    g_acib_clock_ctx.fallback_cpu_hz = is_low_freq ? 80000000u : 160000000u;
    return &g_acib_clock_ctx.deadline_hdr[0];
}

/* @provenance RVA 0x07ddf0 sub_7DDF0 */
void *acib_load_rtkit_board_props(void)
{
    memcpy_wrapper(&g_acib_board_props.btm1, acib_addr_ptr(ACIB_ROM_TAG_BTM1_VAL_ADDR), 4u);
    memcpy_wrapper(&g_acib_board_props.btm2, acib_addr_ptr(ACIB_ROM_TAG_BTM2_VAL_ADDR), 4u);
    memcpy_wrapper(&g_acib_board_props.fpga, acib_addr_ptr(ACIB_ROM_TAG_FPGA_VAL_ADDR), 4u);
    return memcpy_wrapper(&g_acib_board_props.dev_flag,
                          acib_addr_ptr(ACIB_ROM_TAG_DEVF_VAL_ADDR), 1u);
}

/* @provenance RVA 0x07e18c sub_7E18C */
uint32_t acib_board_set_flags(void)
{
    uint32_t btm1 = g_acib_board_props.btm1;
    if ((btm1 & 2u) != 0u) {
        return 1u;
    }
    return (btm1 >> 2) & 1u;
}

/* @provenance RVA 0x07e1a4 sub_7E1A4 */
uint32_t acib_board_has_flag_bit0(void)
{
    uint8_t low = (uint8_t)(g_acib_board_props.btm1 & 0xFFu);
    return 1u ^ ((low & 8u) >> 3);
}

/* @provenance RVA 0x07e1b8 sub_7E1B8 */
uint32_t acib_board_get_rev_nibble(void)
{
    g_boot_mode_ctx[9] = 2u;
    return 1u;
}

/* @provenance RVA 0x07e1c8 sub_7E1C8 */
uint32_t acib_soc_is_fpga_emulation(void)
{
    uint16_t plat_id = (uint16_t)(acib_mmio_read32(ACIB_SOC_PLAT_ID_ADDR) & 0xFFFFu);
    return (plat_id == ACIB_SOC_PLAT_ID_FPGA_VAL) ? 1u : 0u;
}

/* @provenance RVA 0x0e60a8 rtk_get_wake_count */
uint32_t rtk_get_wake_count(void)
{
    return *(uint32_t *)acib_addr_ptr(ACIB_RAM_WAKE_COUNT_ADDR);
}

/* @provenance RVA 0x0e60b4 rtk_get_sleep_count */
uint32_t rtk_get_sleep_count(void)
{
    return *(uint32_t *)acib_addr_ptr(ACIB_RAM_SLEEP_COUNT_ADDR);
}

/* @provenance RVA 0x0e60c0 rtk_stack_watermark_fill */
void rtk_stack_watermark_fill(uint32_t *stack_words, uint32_t byte_len)
{
    while (byte_len >= 8u) {
        stack_words[1] = ACIB_STACK_WATERMARK_HI;
        stack_words[0] = ACIB_STACK_WATERMARK_LO;
        stack_words += 2;
        byte_len -= 8u;
    }
}

/* @provenance RVA 0x0e60dc rtk_stack_watermark_check */
uint32_t rtk_stack_watermark_check(const uint32_t *stack_words, uint32_t byte_len)
{
    while (byte_len != 0u) {
        if (stack_words[0] != ACIB_STACK_WATERMARK_LO ||
            stack_words[1] != ACIB_STACK_WATERMARK_HI) {
            return byte_len;
        }
        stack_words += 2;
        byte_len -= 8u;
    }
    return 0u;
}

/* @provenance RVA 0x070bb4 rtk_init_stacks_watermark */
void rtk_init_stacks_watermark(void)
{
    uint32_t psp_bytes = (uint32_t)sizeof(g_psp_init_stack);
#if !defined(ACIB_HOST_TEST)
    uintptr_t cur_sp;
    __asm__ volatile("mov %0, sp" : "=r"(cur_sp));
    uintptr_t psp_base = (uintptr_t)g_psp_init_stack;
    if (cur_sp > psp_base + 16u && cur_sp <= psp_base + sizeof(g_psp_init_stack)) {
        psp_bytes = (uint32_t)(cur_sp - psp_base - 16u);
    }
#endif
    rtk_stack_watermark_fill(g_psp_init_stack, psp_bytes);
    rtk_stack_watermark_fill(g_msp_stack, ACIB_MSP_STACK_SIZE);
}
