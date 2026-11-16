/*
 * threading.c -- SVC/PendSV entry and context-switch request consumption.
 * The basename is present in the firmware's scheduler assertion strings;
 * placement under rtkit/ is the reconstruction's subsystem boundary.
 */
#include "btp/btp.h"
#include "btp/rtkit.h"
#include "exception_frame.h"

/* @provenance RVA 0x5C04 _rtk_svc_pendsv_entry */
#ifndef BTP_HOST
__attribute__((naked))
#endif
void rtk_svcall_entry(uint32_t exc_return)
{
#ifndef BTP_HOST
    __asm volatile (
        "tst lr, #4\n"
        "beq rtk_exception_entry\n"
        "mrs r0, psp\n"
        "stmdb r0!, {r4-r11}\n"
        "tst lr, #16\n"
        "it eq\n"
        "vstmdbeq r0!, {s16-s31}\n"
        "str lr, [r0, #-8]!\n"
        "bl rtk_pendsv_switch\n"
        "ldr lr, [r0], #8\n"
        "tst lr, #16\n"
        "it eq\n"
        "vldmeq r0!, {s16-s31}\n"
        "ldm r0!, {r4-r11}\n"
        "msr psp, r0\n"
        "bx lr\n"
    );
#else
    struct rtk_exception_frame *frame;
    uint8_t *restored;
    uint32_t sp;

    /* SVCall/PendSV from handler mode is a kernel fault. */
    if ((exc_return & EXC_RETURN_PSP) == 0) {
        rtk_exception_entry(exc_return);
        return;
    }

    sp = cpu_get_psp() - RTK_FRAME_CORE_REGS - 8;
    if ((exc_return & EXC_RETURN_NO_FPU_FRAME) == 0)
        sp -= RTK_FRAME_FPU_REGS;
    frame = BTP_PTR(sp);
    /* A scheduler frame starts at EXC_RETURN, unlike a fault frame which
     * additionally has a 24-byte diagnostics prefix. */
    *(uint32_t *)frame = exc_return;

    restored = rtk_pendsv_switch((uint8_t *)frame, 0, NULL, NULL);
    cpu_set_psp((uint32_t)(uintptr_t)restored + RTK_FRAME_CORE_REGS + 8
                + ((exc_return & EXC_RETURN_NO_FPU_FRAME) ? 0 : RTK_FRAME_FPU_REGS));
#endif
}

/* @provenance RVA 0x0208 Fault_Handler */
#ifndef BTP_HOST
__attribute__((naked))
#endif
void SVC_Handler(uint32_t exc_return)
{
#ifndef BTP_HOST
    __asm volatile ("b.w rtk_svcall_entry");
#else
    rtk_svcall_entry(exc_return);
#endif
}

/* ---- scheduler hooks ------------------------------------------------------------ */

/* All three builds have the same ROM 0x011A helper: zero both output
 * slots. It does not retrieve pending targets. With this helper unchanged,
 * the caller retains its incoming frame even when the request gate opens. */
/* @provenance RVA 0x011A _rtk_clear_context_switch_targets */
void *rtk_clear_context_targets(void *save_target_out, void *restore_target_out)
{
    *(uint32_t *)save_target_out = 0;
    *(uint32_t *)restore_target_out = 0;
    return save_target_out;
}

/* A thread frame is recognised by bit 2 of its first word. */
#define RTK_FRAME_MARKER    0x04u

/* @provenance RVA 0x624C _rtk_thread_context_switch */
uint8_t *rtk_pendsv_switch(uint8_t *sp, int unused, uint8_t **switch_to, uint32_t *save_to)
{
    uint8_t **switch_slot = switch_to;
    uint32_t *save_slot = save_to;

    /* ROM 0x6256/0x6258: 8E251 and 8E258 use CMP #1 / BNE (== 1);
     * 9A348 uses LSLS #31 / BEQ (bit 0 set), as reconstructed here.
     * Values 0 and 1 take the same paths; odd values 3..255 newly enter.
     * The runtime producer's value domain is unknown. The unchanged helper
     * below zeroes both context slots, so this alone does not prove a
     * thread switch: the observed differences are flag consumption and
     * execution of the BASEPRI/helper sequence. */
    if (rtk.resched_pending & 1u) {
        uint32_t saved_basepri = cpu_get_basepri();

        rtk.resched_pending = 0;
        if (saved_basepri == 0 || saved_basepri > rtk_irq.basepri_level)
            cpu_set_basepri(rtk_irq.basepri_level);
        rtk_clear_context_targets(&save_slot, &switch_slot);
        cpu_set_basepri(saved_basepri);

        if (save_slot != NULL) {
            if ((*sp & RTK_FRAME_MARKER) == 0)
                rtk_assert_failed();
            *save_slot = (uint32_t)(uintptr_t)sp;
        }
        if (switch_slot != NULL) {
            sp = *switch_slot;
            if (*switch_slot == NULL)
                rtk_assert_failed();
            if ((*sp & RTK_FRAME_MARKER) == 0)
                rtk_assert_failed();
        }
    }
    return sp;
}

/* ROM 0x6240 compares active SP once, then branches to itself on LT.
 * This C model polls MSP instead; active-stack selection and loop behavior
 * remain a fidelity gap, so the generic ABI arguments are not assigned
 * invented roles here. */
/* @provenance RVA 0x6240 _rtk_check_active_stack_lower_bound */
uint32_t rtk_stack_check(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    while (cpu_get_msp() < MSP_STACK_BASE)
        ;
    return MSP_STACK_BASE;
}
