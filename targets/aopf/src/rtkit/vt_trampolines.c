/*
 * vt_trampolines.c — Exact-RVA 4-byte Thumb-2 branch veneers so indirect calls
 * through unmodified ROM_RODATA RTKit C driver vtables (0x73AF0..0x73FC8) reach
 * our reconstructed C11 driver implementations in aopf.elf.
 *
 * Tracked explicitly as retained_native veneers (never claimed as reconstructed_c).
 */

#include "aopf/rtkit.h"

#if defined(__arm__) || defined(__thumb__)

#define AOPF_VT_TRAMP(hex_rva, target_sym)                                    \
    __attribute__((naked, section(".vt." #hex_rva), used))                    \
    void vt_tramp_##hex_rva(void);                                            \
    void vt_tramp_##hex_rva(void)                                             \
    {                                                                         \
        __asm__ volatile("b.w " #target_sym "\n\t");                          \
    }

AOPF_VT_TRAMP(4e614, nvic_get_cpsr)
AOPF_VT_TRAMP(4e61c, nvic_set_irq_enabled)
AOPF_VT_TRAMP(4e69c, nvic_register_irq)
AOPF_VT_TRAMP(4e6e0, nvic_set_irq_priority)
AOPF_VT_TRAMP(4e774, nvic_dispatch_active_irq)
AOPF_VT_TRAMP(4e850, nvic_trigger_irq)
AOPF_VT_TRAMP(4e8b4, nvic_nop)
AOPF_VT_TRAMP(4e8b8, nvic_ioctl)

AOPF_VT_TRAMP(514f8, rtk_unimplemented_method)
AOPF_VT_TRAMP(51574, SysTick_ioctl)
AOPF_VT_TRAMP(51640, SysTick_setCallback)
AOPF_VT_TRAMP(51648, SysTick_armTicks)
AOPF_VT_TRAMP(51684, SysTick_armUs)
AOPF_VT_TRAMP(516c4, SysTick_cancel)
AOPF_VT_TRAMP(5171c, SysTick_usToTicks)
AOPF_VT_TRAMP(51748, SysTick_ticksToUs)

AOPF_VT_TRAMP(520a4, Mbox_ioctl)
AOPF_VT_TRAMP(5223c, Mbox_unimpl04)
AOPF_VT_TRAMP(52244, Mbox_setIrqHandler)
AOPF_VT_TRAMP(523b4, Mbox_tryReadOne)

AOPF_VT_TRAMP(52da0, Fuse_ioctl)
AOPF_VT_TRAMP(52e40, Fuse_readWords)
AOPF_VT_TRAMP(52ef8, PMGR_ioctl)
AOPF_VT_TRAMP(52f3c, PMGR_getPowerState)

AOPF_VT_TRAMP(55914, TimerBank_ioctl)
AOPF_VT_TRAMP(55984, TimerBank_configChannel)
AOPF_VT_TRAMP(559bc, TimerBank_enableChannel)
AOPF_VT_TRAMP(55a08, TimerBank_startChannel)
AOPF_VT_TRAMP(55a54, TimerBank_stopChannel)
AOPF_VT_TRAMP(55a74, TimerBank_unimpl28)
AOPF_VT_TRAMP(55a7c, TimerBank_unimpl32)

AOPF_VT_TRAMP(5a540, Timebase_syncOffset)
AOPF_VT_TRAMP(5a630, Timebase_usToTicks)
AOPF_VT_TRAMP(5a654, Timebase_ticksToUs)
AOPF_VT_TRAMP(5a678, Timebase_getTicksWrap)
AOPF_VT_TRAMP(5a67e, Timebase_ioctl)
AOPF_VT_TRAMP(5a6ae, Timebase_getTicks)

AOPF_VT_TRAMP(5a928, WDT_ioctl)
AOPF_VT_TRAMP(5a9e4, WDT_setRunning)
AOPF_VT_TRAMP(5aa02, WDT_kick)
AOPF_VT_TRAMP(5aa10, WDT_setBarkTimeoutUs)
AOPF_VT_TRAMP(5aaa8, WDT_getBarkTimeoutUs)
AOPF_VT_TRAMP(5aac0, WDT_setBiteEnable)
AOPF_VT_TRAMP(5aaf8, WDT_setBiteTimeoutUs)
AOPF_VT_TRAMP(5ab8c, WDT_getBiteTimeoutUs)
AOPF_VT_TRAMP(5aba0, WDT_setCallback)

#endif
