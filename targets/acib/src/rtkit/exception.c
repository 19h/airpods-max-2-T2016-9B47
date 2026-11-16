#include "acib/rtkit.h"
#include "acib/bus.h"
#include "acib/memmap.h"

/*
 * Exception/fault entry veneers, stack-protector abort, fault frame
 * extractor, and Thumb-2 call-site LR unwinder reconstructed from acib ROM.
 */

/* External pending_c RTKit panic/unwind helpers */
extern void     sub_6BDDC(const char *fmt, uint32_t *args_ptr, uint32_t arg2, uint32_t arg3, uint32_t *sp_arg);
extern void     sub_EAC84(uint32_t code, uint32_t sub, const char *fmt, uint32_t *args_ptr, uint32_t *sp_arg);
extern void     sub_6CFF4(uint32_t halt_reason);
extern uint32_t sub_EA86E(uint32_t addr);
extern void     sub_E5E78(void *exc_frame, uint32_t ipsr);

#if !defined(ACIB_HOST_TEST)

/* Relocated HardFault/MemManage/BusFault/UsageFault veneer shim (RVA 0x0e4a50, pending_c) */
ACIB_SECTION(".pinned.fault") ACIB_NAKED ACIB_USED
void Fault_Handler(void)
{
    __asm__ volatile(
        "ldr     r0, =0x1001b580\n\t"
        "cmp     sp, r0\n\t"
        "1: blt  1b\n\t"
        "ldr     r1, =0x000f52e0\n\t"
        "ldr     r1, [r1]\n\t"
        "cmp     sp, r1\n\t"
        "2: bgt  2b\n\t"
        "tst.w   lr, #4\n\t"
        "ite     eq\n\t"
        "mrseq   r0, msp\n\t"
        "mrsne   r0, psp\n\t"
        "stmdb   r0!, {r4, r5, r6, r7, r8, r9, r10, r11}\n\t"
        "str     lr, [r0, #-8]!\n\t"
        "sub.w   r0, r0, #0x18\n\t"
        "tst.w   lr, #4\n\t"
        "it      eq\n\t"
        "msreq   msp, r0\n\t"
        "mrs     r1, ipsr\n\t"
        "bl      sub_E5E78\n\t"
        "3: b    3b\n\t"
    );
}

/* @retained_native RVA 0x0e4a94 NMI_Handler */
ACIB_SECTION(".pinned.nmi") ACIB_NAKED ACIB_USED
void NMI_Handler(void)
{
    __asm__ volatile(
        "ldr     r0, =0x1001b580\n\t"
        "cmp     sp, r0\n\t"
        "1: blt  1b\n\t"
        "ldr     r1, =0x000f52e0\n\t"
        "ldr     r1, [r1]\n\t"
        "cmp     sp, r1\n\t"
        "2: bgt  2b\n\t"
        "tst.w   lr, #4\n\t"
        "ite     eq\n\t"
        "mrseq   r0, msp\n\t"
        "mrsne   r0, psp\n\t"
        "stmdb   r0!, {r4, r5, r6, r7, r8, r9, r10, r11}\n\t"
        "str     lr, [r0, #-8]!\n\t"
        "sub.w   r0, r0, #0x18\n\t"
        "tst.w   lr, #4\n\t"
        "ite     eq\n\t"
        "msreq   msp, r0\n\t"
        "msrne   psp, r0\n\t"
        "ldr     r5, =g_rtk_nmi_saved_sp\n\t"
        "mov     r4, lr\n\t"
        "mrs     r1, ipsr\n\t"
        "str     r0, [r5]\n\t"
        "bl      SysTick_Handler\n\t"
        "mov     lr, r4\n\t"
        "mov.w   r0, #0\n\t"
        "str     r0, [r5]\n\t"
        "tst.w   lr, #4\n\t"
        "ite     eq\n\t"
        "mrseq   r0, msp\n\t"
        "mrsne   r0, psp\n\t"
        "add.w   r0, r0, #0x20\n\t"
        "ldm.w   r0!, {r4, r5, r6, r7, r8, r9, r10, r11}\n\t"
        "tst.w   lr, #4\n\t"
        "ite     eq\n\t"
        "msreq   msp, r0\n\t"
        "msrne   psp, r0\n\t"
        "bx      lr\n\t"
    );
}

/* Relocated SVCall/PendSV context-switch veneer shim (RVA 0x0e4b14, pending_c) */
ACIB_SECTION(".pinned.svcall") ACIB_NAKED ACIB_USED
void SVCall_Handler(void)
{
    __asm__ volatile(
        "mrs     r0, psp\n\t"
        "stmdb   r0!, {r4, r5, r6, r7, r8, r9, r10, r11}\n\t"
        "str     lr, [r0, #-8]!\n\t"
        "bl      rtk_sched_switch\n\t"
        "ldr     lr, [r0], #8\n\t"
        "ldm.w   r0!, {r4, r5, r6, r7, r8, r9, r10, r11}\n\t"
        "msr     psp, r0\n\t"
        "bx      lr\n\t"
    );
}

/* Relocated warm-boot sleep context-restore veneer shim (RVA 0x0e4bb0, pending_c) */
ACIB_SECTION(".pinned.sleep_restore") ACIB_NAKED ACIB_USED
uint32_t rtk_sleep_restore_context(void)
{
    __asm__ volatile(
        "ldr     r1, =0x10000074\n\t"
        "ldr     r2, =0xbaadf1ac\n\t"
        "str     r2, [r1]\n\t"
        "ldr     r2, =0x1000006c\n\t"
        "ldr     r0, [r2]\n\t"
        "cmp     r0, #0\n\t"
        "beq     1f\n\t"
        "mov.w   r4, #0xe000e000\n\t"
        "add.w   r0, r0, #0x5c\n\t"
        "ldmdb   r0!, {r5, r6, r7, r8, r9, r10, r11}\n\t"
        "str.w   r5, [r4, #0xd08]\n\t"
        "str.w   r6, [r4, #0xd14]\n\t"
        "str.w   r7, [r4, #0xd18]\n\t"
        "str.w   r8, [r4, #0xd1c]\n\t"
        "str.w   r9, [r4, #0xd20]\n\t"
        "str.w   r10, [r4, #0xd24]\n\t"
        "ldmdb   r0!, {r3, r5, r6, r7}\n\t"
        "msr     control, r3\n\t"
        "msr     primask, r5\n\t"
        "msr     faultmask, r6\n\t"
        "msr     basepri, r7\n\t"
        "str.w   r11, [r4, #0xd88]\n\t"
        "ldmdb   r0!, {r2, r3}\n\t"
        "msr     psp, r2\n\t"
        "msr     msp, r3\n\t"
        "ldmdb   r0!, {r4, r5, r6, r7, r8, r9, r10, r11, r12, lr}\n\t"
        "ldr     r2, =0x10000064\n\t"
        "ldr     r3, [r2]\n\t"
        "add.w   r3, r3, #1\n\t"
        "str     r3, [r2]\n\t"
        "mov.w   r0, #0\n\t"
        "bx      lr\n\t"
        "1: b    1b\n\t"
    );
}

#else

void     Fault_Handler(void) {}
void     NMI_Handler(void) {}
void     SVCall_Handler(void) {}
uint32_t rtk_sleep_restore_context(void) { return 0u; }

#endif /* !ACIB_HOST_TEST */

/* @provenance RVA 0x06bf30 rtk_abort_panic */
void rtk_abort_panic(const char *fmt, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    uint32_t varargs[3];
    varargs[0] = arg1;
    varargs[1] = arg2;
    varargs[2] = arg3;

    sub_6BDDC(fmt, varargs, arg2, arg3, varargs);
    sub_EAC84(0x20u, 0u, fmt, varargs, varargs);
    sub_6CFF4(4u);
}

/* @provenance RVA 0x0e4c48 __stack_chk_fail */
void __stack_chk_fail(void)
{
    rtk_abort_panic("stk_fail", 0u, 0u, 0u);
}

/* @provenance RVA 0x071230 sub_71230 */
void acib_panic_null(uint32_t unused_r0, uint32_t unused_r1, uint32_t arg2, uint32_t arg3)
{
    (void)unused_r0;
    (void)unused_r1;
    rtk_abort_panic(NULL, 0u, arg2, arg3);
}

/* @provenance RVA 0x0725fc sub_725FC */
void acib_panic_code_line(uint32_t encoded, uint32_t arg1)
{
    rtk_abort_panic(NULL, arg1, encoded & 0x3Fu, encoded >> 12);
}

/* @provenance RVA 0x072614 sub_72614 */
void acib_panic_code_group(uint32_t encoded, uint32_t arg1, uint32_t unused_r2, uint32_t arg3)
{
    (void)unused_r2;
    rtk_abort_panic(NULL, arg1, encoded & 0x3Fu, arg3);
}

/* @provenance RVA 0x07264c sub_7264C */
void acib_ctx_store_fault_arg(uint8_t *ctx, uint32_t val)
{
    acib_write_unaligned_le32(ctx + 0x1A0u, val);
}

/* @provenance RVA 0x0e6040 sub_E6040 */
void rtk_fault_extract_frame(const uint8_t *raw_ctx, struct acib_fault_summary *out)
{
    uint32_t r0_code   = acib_read_unaligned_le32(raw_ctx + 0x10u);
    uint32_t fault_sub = acib_read_unaligned_le32(raw_ctx + 0x14u);
    uint32_t exc_lr    = acib_read_unaligned_le32(raw_ctx + 0x18u);
    uint32_t saved_sp  = acib_read_unaligned_le32(raw_ctx + 0x1Cu);

    /* Bit 4 of EXC_RETURN selects standard (+0x40 -> +0x54..0x5C) vs extended (+0x94..0x9C) */
    bool bit4_set = ((exc_lr & 0x10u) != 0u);
    uint32_t lr_off   = bit4_set ? 0x54u : 0x94u;
    uint32_t pc_off   = bit4_set ? 0x58u : 0x98u;
    uint32_t xpsr_off = bit4_set ? 0x5Cu : 0x9Cu;

    out->r0_or_code    = r0_code;
    out->zero_04       = 0u;
    out->fault_sub     = fault_sub;
    out->zero_0c       = 0u;
    out->fault_sub_dup = fault_sub;
    out->zero_14       = 0u;
    out->pc            = acib_read_unaligned_le32(raw_ctx + pc_off);
    out->zero_1c       = 0u;
    out->lr            = acib_read_unaligned_le32(raw_ctx + lr_off);
    out->zero_24       = 0u;
    out->sp            = saved_sp;
    out->zero_2c       = 0u;
    out->xpsr          = acib_read_unaligned_le32(raw_ctx + xpsr_off);
    out->zero_34       = 0u;
}

/* @provenance RVA 0x0e6474 sub_E6474 */
uint32_t rtk_unwind_adjust_lr(uint32_t lr_val)
{
    uint32_t even_lr = lr_val & ~1u;
    uint32_t cand2 = even_lr - 2u;

    if (sub_EA86E(cand2) == 0u) {
        return even_lr - 3u;
    }

    uint16_t insn16 = acib_read_unaligned_le16(acib_addr_ptr(cand2));
    /* BLX Rm is 01000111 1xxxx000 (0x4780 when Rm bits [6:3] cleared) */
    if ((insn16 & ~0x0078u) == 0x4780u) {
        return cand2;
    }

    uint32_t cand4 = even_lr - 4u;
    if (sub_EA86E(cand4) == 0u) {
        return even_lr - 3u;
    }
    return cand4;
}
