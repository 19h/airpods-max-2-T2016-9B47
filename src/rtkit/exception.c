/*
 * exception.c -- RTKit fault entry and architectural frame capture.
 *
 * Every system exception the firmware does not service lands in
 * rtk_exception_entry(), which reserves a frame on the faulting stack,
 * decodes the fault registers into it and calls the W3 panic bridge.
 * SVCall/PendSV entry and scheduler hooks live in threading.c; application
 * crash reporting lives in ../w3/exception.c.
 */
#include "btp/btp.h"
#include "btp/rtkit.h"
#include "exception_frame.h"

/* Exception numbers with a dedicated RTKit class: 1..6, SVCall, PendSV. */
#define RTK_CLASSIFIED_EXCEPTIONS   0x243Fu

static const uint8_t exception_class_by_number[14] = {
    [ARM_EXC_RESET - 1]        = RTK_EXC_RESET,
    [ARM_EXC_NMI - 1]          = RTK_EXC_NMI,
    [ARM_EXC_HARDFAULT - 1]    = RTK_EXC_HARDFAULT,
    [ARM_EXC_MEMMANAGE - 1]    = RTK_EXC_DATA_ABORT,
    [ARM_EXC_BUSFAULT - 1]     = RTK_EXC_DATA_ABORT,
    [ARM_EXC_USAGEFAULT - 1]   = RTK_EXC_UNDEF,
    [ARM_EXC_SVCALL - 1]       = RTK_EXC_SVC,
    [ARM_EXC_DEBUGMONITOR - 1] = RTK_EXC_RESET,
    [ARM_EXC_PENDSV - 1]       = RTK_EXC_SVC,
};

static uint32_t frame_word(const struct rtk_exception_frame *frame, uint32_t offset)
{
    return *(const uint32_t *)((const uint8_t *)frame + offset);
}

/* ---- entry veneers ------------------------------------------------------------ */

/* @provenance RVA 0x5BB4 _rtk_nmi_and_kernel_fault_entry */
#ifndef BTP_HOST
__attribute__((naked))
#endif
void rtk_exception_entry(uint32_t exc_return)
{
#ifndef BTP_HOST
    __asm volatile (
        "ldr r0, =0x4d289ec8\n"
        "cmp sp, r0\n"
        "1: blt 1b\n"
        "ldr r1, =0x4d28a2c8\n"
        "cmp sp, r1\n"
        "2: bgt 2b\n"
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "stmdb r0!, {r4-r11}\n"
        "tst lr, #16\n"
        "it eq\n"
        "vstmdbeq r0!, {s16-s31}\n"
        "str lr, [r0, #-8]!\n"
        "sub r0, r0, #24\n"
        "tst lr, #4\n"
        "it eq\n"
        "msreq msp, r0\n"
        "mrs r1, ipsr\n"
        "bl rtk_handle_exception\n"
        "3: b 3b\n"
    );
#else
    struct rtk_exception_frame *frame;
    uint32_t sp;

    /* Spin for the debugger if the handler stack itself is corrupt. */
    while (cpu_get_msp() < MSP_STACK_BASE)
        ;
    while (cpu_get_msp() > MSP_STACK_TOP)
        ;

    sp = (exc_return & EXC_RETURN_PSP) ? cpu_get_psp() : cpu_get_msp();
    sp -= RTK_FRAME_CORE_REGS;
    if ((exc_return & EXC_RETURN_NO_FPU_FRAME) == 0)
        sp -= RTK_FRAME_FPU_REGS;
    sp -= sizeof(struct rtk_exception_frame);

    frame = BTP_PTR(sp);
    frame->exc_return = exc_return;
    if ((exc_return & EXC_RETURN_PSP) == 0)
        cpu_set_msp(sp);

    rtk_handle_exception(frame, cpu_get_ipsr());
#endif
}

/* @provenance RVA 0x0204 NMI_Handler */
#ifndef BTP_HOST
__attribute__((naked))
#endif
void Fault_Handler(uint32_t exc_return)
{
#ifndef BTP_HOST
    __asm volatile ("b.w rtk_exception_entry");
#else
    rtk_exception_entry(exc_return);
#endif
}

/* ---- frame capture ---------------------------------------------------------- */

/* @provenance RVA 0x6302 _rtk_exc_get_frame_size */
uint32_t rtk_exception_frame_size(const struct rtk_exception_frame *frame)
{
    if (frame != NULL && (frame->exc_return & EXC_RETURN_NO_FPU_FRAME))
        return RTK_FRAME_SIZE_BASIC;
    return RTK_FRAME_SIZE_EXTENDED;
}

static uint32_t rtk_classify_exception(uint32_t exception_number)
{
    uint32_t index = exception_number - 1;

    if (index <= 13 && ((RTK_CLASSIFIED_EXCEPTIONS >> index) & 1u))
        return exception_class_by_number[index];
    return exception_number > ARM_EXC_SYSTICK ? RTK_EXC_IRQ : RTK_EXC_UNKNOWN;
}

/*
 * Stack pointer of the interrupted code: the hardware frame starts after the
 * saved registers, is 8 or 26 words long depending on the stacked LR, and
 * carries a padding word when xPSR.STKALIGN says the stack was realigned.
 */
static uint32_t rtk_caller_sp(const struct rtk_exception_frame *frame)
{
    bool basic = (frame->exc_return & EXC_RETURN_NO_FPU_FRAME) != 0;
    uint32_t xpsr_offset = basic ? 0x5C : 0x9C;
    uint32_t lr_offset = basic ? 0x54 : 0x94;
    uint32_t stacked_lr = frame_word(frame, lr_offset);
    uint32_t xpsr = frame_word(frame, xpsr_offset);
    uint32_t hw_frame_end = (stacked_lr & EXC_RETURN_NO_FPU_FRAME) ? 0x48 : 0xD0;

    return (uint32_t)(uintptr_t)frame + 0x18 + hw_frame_end
           + ((xpsr & XPSR_STKALIGN_PAD) ? 4 : 0);
}

/* @provenance RVA 0x6314 _rtk_capture_fault_and_panic */
void rtk_handle_exception(struct rtk_exception_frame *frame, uint32_t exception_number)
{
    uint32_t exception_class;
    uint32_t bfar, cfsr, dfsr, hfsr, mmfar;
    uint32_t basepri, control, faultmask, primask;

    ++rtk.fault_count;
    exception_class = rtk_classify_exception(exception_number);

    rtk.exception_frame = (uint32_t)(uintptr_t)frame;
    rtk.exception_frame_size = rtk_exception_frame_size(frame);
    frame->exception_number = exception_number;
    rtk.exception_class = exception_class;

    bfar = SCB_BFAR;
    cfsr = SCB_CFSR;
    dfsr = SCB_DFSR;
    hfsr = SCB_HFSR;
    mmfar = SCB_MMFAR;

    basepri = cpu_get_basepri();
    control = cpu_get_control();
    faultmask = cpu_get_faultmask();
    primask = cpu_get_primask();

    frame->control = control;
    frame->cfsr = 0;
    frame->masks = (basepri << 8) | ((faultmask & 1u) << 1) | (primask & 1u);

    frame->status = 0;
    frame->fault_address = 0;
    frame->caller_sp = rtk_caller_sp(frame);

    switch (exception_number) {
    case ARM_EXC_HARDFAULT:
        frame->cfsr = cfsr;
        frame->status = hfsr;
        frame->fault_address = (cfsr & SCB_CFSR_BUSFAULT_MASK) ? bfar : mmfar;
        break;
    case ARM_EXC_MEMMANAGE:
        frame->status = cfsr;
        frame->fault_address = mmfar;
        break;
    case ARM_EXC_BUSFAULT:
        frame->status = cfsr;
        frame->fault_address = bfar;
        break;
    case ARM_EXC_USAGEFAULT:
        frame->status = cfsr;
        break;
    case ARM_EXC_DEBUGMONITOR:
        frame->status = dfsr;
        break;
    default:
        break;
    }

    btp_exception_panic(frame, exception_class);
    rtk_platform_halt();
}

