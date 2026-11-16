/*
 * boot.c -- RTKit reset path: stacks, core configuration, cold/warm boot.
 *
 * The vector table's initial-SP slot is zero; both stacks are installed and
 * painted here before any C code with a frame runs.  A warm boot (the MAC
 * restarted the PHY without a power cycle, see warm_boot_detected()) skips the
 * .bss clear and the MPU probe and goes straight to btp_main().
 */
#include "btp/btp.h"
#include "btp/rtkit.h"
#include "btp/status.h"
#include "btp/nvv.h"

/* ---- stacks ------------------------------------------------------------------- */

#ifdef BTP_HOST
static void rtk_paint_stack(uint32_t base, uint32_t top)
{
    uint32_t addr;

    for (addr = top; addr > base; ) {
        addr -= sizeof(uint64_t);
        BTP_REF(volatile uint64_t, addr) = STACK_PAINT_PATTERN;
    }
}
#endif

/* @provenance RVA 0x5B50 _rtk_reset_init_stacks */
#ifndef BTP_HOST
__attribute__((naked))
#endif
void rtk_init_stacks(void)
{
#ifndef BTP_HOST
    /* No C frame can exist before changing stacks. The register-only entry
     * follows 0x5B50, including its bottom-of-backtrace sentinel. */
    __asm volatile (
        "movw r7, #0x7700\n"
        "movt r7, #0x7777\n"
        "ldr r0, =0x4d28a2c8\n"
        "msr msp, r0\n"
        "ldr r1, =0x4d289ec8\n"
        "msr psp, r1\n"
        "ldr r2, =0x534b5452\n"
        "ldr r3, =0x4b434154\n"
        "movw r4, #0x400\n"
        "1: strd r2, r3, [r0, #-8]!\n"
        "subs r4, #8\n"
        "bhi 1b\n"
        "movw r4, #0x400\n"
        "2: strd r2, r3, [r1, #-8]!\n"
        "subs r4, #8\n"
        "bhi 2b\n"
        "mrs r0, control\n"
        "orr r0, r0, #2\n"
        "msr control, r0\n"
        "b.w rtk_boot\n"
    );
#else
    cpu_set_msp(MSP_STACK_TOP);
    cpu_set_psp(PSP_STACK_TOP);

    rtk_paint_stack(MSP_STACK_BASE, MSP_STACK_TOP);
    rtk_paint_stack(PSP_STACK_BASE, PSP_STACK_TOP);

    /* Thread mode runs on the PSP from here on. */
    cpu_set_control(cpu_get_control() | CONTROL_SPSEL);
    rtk_boot();
#endif
}

/* @provenance RVA 0x0200 Reset_Handler */
#ifndef BTP_HOST
__attribute__((naked))
#endif
void Reset_Handler(void)
{
#ifndef BTP_HOST
    __asm volatile ("b.w rtk_init_stacks");
#else
    rtk_init_stacks();
#endif
}

/* ---- core configuration --------------------------------------------------------- */

/* Shared by the cold and warm paths: stack alignment, trapping and the FPU. */
static void rtk_core_init(bool warm_boot)
{
    if (warm_boot) {
        /* Warm boot has two observable CCR RMW operations at 0x012C. */
        SCB_CCR |= SCB_CCR_STKALIGN | SCB_CCR_DIV_0_TRP;
        SCB_CCR &= ~SCB_CCR_UNALIGN_TRP;
    } else {
        SCB_CCR = (SCB_CCR | SCB_CCR_STKALIGN | SCB_CCR_DIV_0_TRP)
                  & ~SCB_CCR_UNALIGN_TRP;
    }

    SCB_CPACR = 0;
    FPU_FPCCR = FPU_FPCCR_ASPEN | FPU_FPCCR_LSPEN;
    FPU_FPDSCR = 0;
    SCB_CPACR = SCB_CPACR_CP10_CP11_FULL;
}

/* SVCall and PendSV at the BASEPRI boundary, everything else at 0. */
static void rtk_set_exception_priorities(void)
{
    SCB_SHPR1 = 0;
    SCB_SHPR2 = RTK_BASEPRI_LEVEL_DEFAULT << 24;    /* SVCall */
    SCB_SHPR3 = RTK_BASEPRI_LEVEL_DEFAULT << 16;    /* PendSV */
}

#define RTK_SHCSR_FAULTS_ENABLED \
    (SCB_SHCSR_MEMFAULTENA | SCB_SHCSR_BUSFAULTENA | SCB_SHCSR_USGFAULTENA)

/* @provenance RVA 0x5C40 _rtk_stack_chk_init */
void rtk_stack_guard_init(void)
{
    memcpy(&__stack_chk_guard, &rtk_meta_stack_guard.value, rtk_meta_stack_guard.length);
}

/* @provenance RVA 0x5C58 __stack_chk_fail */
void __stack_chk_fail(void)
{
    rtk_abort();
}

/* Probe the MPU and record its geometry; regions are never enabled. */
static void rtk_mpu_probe(void)
{
    uint32_t type = MPU_TYPE;
    uint32_t region_count = FIELD_GET(RFIELD(MPU_TYPE_DREGION_SHIFT, 8), type);

    rtk_mpu.region_count = region_count;
    if ((type & MPU_TYPE_SEPARATE) == 0 && region_count != 0) {
        uint32_t rbar;

        MPU_RBAR = MPU_RBAR_ADDR_MASK;
        rbar = MPU_RBAR;
        rtk_mpu.region_mask = region_count >= 32
                             ? UINT32_MAX : (1u << region_count) - 1u;
        rtk_mpu.min_region_size = (0u - (rbar & MPU_RBAR_ADDR_MASK)) & rbar;

        memset(rtk_mpu.regions, 0, sizeof rtk_mpu.regions);
        for (uint32_t i = 0; i < region_count; ++i) {
            rtk_mpu.regions[i].rasr = 0;
            rtk_mpu.regions[i].rbar = MPU_RBAR_ADDR_MASK;
        }
    }
}

/* @provenance RVA 0x605C _rtk_cold_boot_init */
void rtk_cold_boot(void)
{
    uint8_t data_reload = 0;

    /* Clear .bss around the two stacks (which were just painted). */
    memset(BTP_PTR(STATE_CLOCK_BASE), 0, PSP_STACK_BASE - STATE_CLOCK_BASE);
    memset(BTP_PTR(MSP_STACK_BASE), 0, 0);
    memset(BTP_PTR(STATE_DIAG_BASE), 0, RAM_END - STATE_DIAG_BASE);
    cpu_dsb();

    /* DSRL: whether the loader must reload .data on a restart (0 here). */
    memcpy(&data_reload, &rtk_meta_data_reload.value, rtk_meta_data_reload.length);
    if (data_reload != 0) {
        /* These are runtime addresses, not offsets in the padded raw dump.
         * The loader must expose the initialized RAM image at 0x88D2.
         * The original copies through the 0x9900-byte alignment boundary.
         * In 8E251/8E258 the corresponding ROM literal is 0x88CF, not
         * 0x88D2; both builds still place raw RAM bytes at file offset
         * 0x88E0. The +3 change follows the longer RTKit string in 9A348,
         * and is distinct from the aligned 0x88D0/0x88D4 header values. */
        memmove(BTP_PTR(MAC_MAILBOX_BASE), BTP_PTR(0x88D2u), 0x9900u);
    }

    rtk_stack_guard_init();
    rtk_core_init(false);
    cpu_dsb();
    cpu_isb();
    rtk_mpu_probe();

    rtk_set_exception_priorities();
    rtk_irq.basepri_level = RTK_BASEPRI_LEVEL_DEFAULT;
    rtk_irq.priority_level = RTK_IRQ_PRIORITY_DEFAULT;
    SCB_SHCSR = RTK_SHCSR_FAULTS_ENABLED;

    /* Interrupts stay masked until btp_irq_init(). */
    cpu_set_primask(1);
    cpu_set_basepri(RTK_BASEPRI_LEVEL_DEFAULT);

    /* The image has no constructors: __init_array_start == __init_array_end. */
    {
        void (**ctor)(void) = BTP_PTR(RAM_BSS_BASE);
        void (**ctor_end)(void) = BTP_PTR(RAM_BSS_BASE);

        for (; ctor < ctor_end; ++ctor)
            (*ctor)();
    }

    btp_main();
}

/* @provenance RVA 0x0124 _rtk_warm_boot_init */
void rtk_warm_boot(void)
{
    rtk_core_init(true);
    rtk_set_exception_priorities();
    SCB_SHCSR = RTK_SHCSR_FAULTS_ENABLED;

    rtk_configure_irq_priority(RTK_IRQ_PRIORITY_DEFAULT);
    cpu_set_primask(1);
    cpu_set_basepri(rtk_irq.basepri_level);
    btp_main();
}

/* @provenance RVA 0x0188 btp_late_init_noop */
int rtk_platform_late_init(void)
{
    return 0;
}

/* @provenance RVA 0x018C _rtk_boot_router */
void rtk_boot(void)
{
    if (warm_boot_detected())
        rtk_warm_boot();
    rtk_cold_boot();
}
