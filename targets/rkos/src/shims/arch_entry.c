#include "rkos/rkos.h"

/*
 * Hand-written ARMv7E-M boot/exception SHIMS (ledger: `pending_c`, `c_shim`).
 *
 * These 9 naked routines exist only so `rkos.elf` links and its pinned vector
 * entries resolve. They are NOT the original instructions: raw Reset_Handler
 * (0x108), Reset_Main (0x1F8), HardFault (0x290: SP bounds checks + stmdb/vstmdb
 * fault frame), NMI (0x2E0), SVCall (0x374), sub_47C, SysTick (0x34D8: dispatch
 * through [[obj+0x50]+0xC]), sub_3B0C and sub_13084 remain unreconstructed.
 * They carry no provenance tags, and tools/verify_fidelity.py enforces that.
 */

#ifdef RKOS_HOST

void Reset_Handler(void) {}
void Reset_Main(void) {}
void HardFault_Handler(void) {}
void NMI_Handler(void) {}
void SVCall_Handler(void) {}
void SysTick_Handler(void) {}
void rkos_sleep_save_and_wfi(void *ctx) { (void)ctx; }
void rkos_sleep_restore(const void *ctx) { (void)ctx; }
void rkos_halt_spin(void) {}

#else

/* SHIM for RVA 0x00108. Raw Reset_Handler instead: if *0x4000C080 bit1 is clear,
 * clears bit16 of *0x4000C02C, programs 0x4000C0A0..0x4000C0B8/0x4000C120 from ROM
 * words (0x1640C..0x16454 +8), polling *0x4000C12C == 1 between writes, sets bit16
 * again, stores ROM word 0x16474 to *0x008000F0, then b.w Reset_Main. The shim's
 * own magic check below is an earlier, incorrect guess and is not raw behaviour. */
__attribute__((naked, section(".text.entry.reset"), used))
void Reset_Handler(void)
{
    __asm volatile(
        "cpsid i\n\t"
        "ldr   r0, =0x00800000\n\t"
        "movs  r1, #1\n\t"
        "str   r1, [r0]\n\t"
        "ldr   r0, =0x20000190\n\t"
        "ldr   r1, [r0]\n\t"
        "ldr   r2, =0x2568254c\n\t"
        "cmp   r1, r2\n\t"
        "bne   1f\n\t"
        "movs  r1, #0\n\t"
        "str   r1, [r0]\n\t"
        "ldr   r2, [r0, #4]\n\t"
        "cbz   r2, 1f\n\t"
        "bx    r2\n\t"
        "1:\n\t"
        "ldr   r0, =0x20000bc0\n\t"
        "ldr   r1, =0x2001f068\n\t"
        "movs  r2, #0\n\t"
        "2:\n\t"
        "cmp   r0, r1\n\t"
        "bhs   3f\n\t"
        "str   r2, [r0], #4\n\t"
        "b     2b\n\t"
        "3:\n\t"
        "b     Reset_Main\n\t"
    );
}

/* RVA 0x001F8: Establishes MSP (0x2001F060) & PSP (0x2001D940), paints stacks,
 * enables FPU, initializes stack cookie, and enters rtkit_start(). */
__attribute__((naked, section(".text.entry.reset_main"), used))
void Reset_Main(void)
{
    __asm volatile(
        "cpsid i\n\t"
        "ldr   r0, =0x2001f060\n\t"
        "msr   msp, r0\n\t"
        "ldr   r0, =0x2001d940\n\t"
        "msr   psp, r0\n\t"
        "movs  r0, #2\n\t"
        "msr   control, r0\n\t"
        "isb   sy\n\t"
        "bl    rkos_stack_guard_init\n\t"
        "b     rtkit_start\n\t"
    );
}

/* RVA 0x00290: Fault capture stub for HardFault/MemManage/BusFault/UsageFault.
 * Builds extended register frame on MSP and calls rtkit_exception_halt(sp, ipsr). */
__attribute__((naked, section(".text.entry.hardfault"), used))
void HardFault_Handler(void)
{
    __asm volatile(
        "cpsid i\n\t"
        "tst   lr, #4\n\t"
        "ite   eq\n\t"
        "mrseq r1, msp\n\t"
        "mrsne r1, psp\n\t"
        "mrs   r2, control\n\t"
        "push  {r4-r11, lr}\n\t"
        "sub   sp, #0x24\n\t"
        "str   r1, [sp, #0x1c]\n\t"
        "str   lr, [sp, #0x18]\n\t"
        "str   r2, [sp, #0x20]\n\t"
        "mov   r0, sp\n\t"
        "mrs   r1, ipsr\n\t"
        "b     rtkit_exception_halt\n\t"
    );
}

/* RVA 0x002E0: NMI / Watchdogbark handler; saves SP to 0x20000D40 and dispatches
 * via IRQ_CommonHandler() before faulting if unhandled. */
__attribute__((naked, section(".text.entry.nmi"), used))
void NMI_Handler(void)
{
    __asm volatile(
        "cpsid i\n\t"
        "push  {r4-r11, lr}\n\t"
        "ldr   r0, =0x20000d40\n\t"
        "mov   r1, sp\n\t"
        "str   r1, [r0]\n\t"
        "bl    IRQ_CommonHandler\n\t"
        "pop   {r4-r11, pc}\n\t"
    );
}

/* RVA 0x00374: SVCall / PendSV context-switch entry; saves r4-r11 (+ s16-s31 if
 * FPCA active) onto PSP, calls rtkit_sched_switch(psp), restores next thread. */
__attribute__((naked, section(".text.entry.svcall"), used))
void SVCall_Handler(void)
{
    __asm volatile(
        "mrs   r0, psp\n\t"
        "tst   lr, #0x10\n\t"
        "it    eq\n\t"
        "vstmdbeq r0!, {s16-s31}\n\t"
        "stmdb r0!, {r4-r11, lr}\n\t"
        "bl    rtkit_sched_switch\n\t"
        "ldmia r0!, {r4-r11, lr}\n\t"
        "tst   lr, #0x10\n\t"
        "it    eq\n\t"
        "vldmiaeq r0!, {s16-s31}\n\t"
        "msr   psp, r0\n\t"
        "isb   sy\n\t"
        "bx    lr\n\t"
    );
}

/* RVA 0x034D8: SysTick & external IRQ common entry; invokes IRQ_CommonHandler()
 * and pends PendSV (SCB_ICSR = 0x10000000) when g_sched_resched_pending != 0. */
__attribute__((naked, section(".text.entry.systick"), used))
void SysTick_Handler(void)
{
    __asm volatile(
        "push  {r4, lr}\n\t"
        "bl    IRQ_CommonHandler\n\t"
        "ldr   r0, =0x2001dde8\n\t"
        "ldr   r1, [r0]\n\t"
        "cbz   r1, 1f\n\t"
        "movs  r1, #0\n\t"
        "str   r1, [r0]\n\t"
        "ldr   r2, =0xe000ed04\n\t"
        "mov.w r3, #0x10000000\n\t"
        "str   r3, [r2]\n\t"
        "1:\n\t"
        "pop   {r4, pc}\n\t"
    );
}

/* RVA 0x0047C: Deep-sleep CPU state save + WFI stub. */
__attribute__((naked))
void rkos_sleep_save_and_wfi(void *ctx __attribute__((unused)))
{
    __asm volatile(
        "stmia r0!, {r4-r11, lr}\n\t"
        "mrs   r1, msp\n\t"
        "mrs   r2, psp\n\t"
        "mrs   r3, control\n\t"
        "stmia r0!, {r1-r3}\n\t"
        "dsb   sy\n\t"
        "wfi\n\t"
        "isb   sy\n\t"
        "sub   r0, #0x30\n\t"
        "ldmia r0, {r4-r11, pc}\n\t"
    );
}

/* RVA 0x03B0C: Deep-sleep CPU state restore stub. */
__attribute__((naked))
void rkos_sleep_restore(const void *ctx __attribute__((unused)))
{
    __asm volatile(
        "add   r1, r0, #0x24\n\t"
        "ldmia r1, {r2-r4}\n\t"
        "msr   msp, r2\n\t"
        "msr   psp, r3\n\t"
        "msr   control, r4\n\t"
        "isb   sy\n\t"
        "ldmia r0, {r4-r11, pc}\n\t"
    );
}

/* RVA 0x13084: Terminal infinite spin loop at end of ROM_TEXT. */
__attribute__((naked))
void rkos_halt_spin(void)
{
    __asm volatile(
        "cpsid i\n\t"
        "1:\n\t"
        "wfe\n\t"
        "b     1b\n\t"
    );
}

#endif /* !RKOS_HOST */
