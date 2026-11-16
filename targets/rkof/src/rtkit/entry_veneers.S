/*
 * Architectural ARMv7E-M naked exception entry veneers.
 * Explicitly tracked as `retained_native` in verification/ledger.json and
 * byte-checked against ../../../rkof by tools/verify_load_image.py:
 *   - Reset_Handler   @ 0x000001F8 (size 0x98)
 *   - Fault_Handler   @ 0x00000290 (size 0x50)
 *   - NMI_Handler     @ 0x000002E0 (size 0x94)
 *   - SVCall_Handler  @ 0x00000374 (size 0x32)
 *   - Default_Handler @ 0x00016F70 (size 0x1C)
 */

    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb

    .section .text.entry_veneers, "ax", %progbits

    .global Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    ldr     r1, .Lreset_boot_guard_ptr
    ldr     r0, [r1]
    ldr     r2, .Lreset_cold_magic
    cmp     r0, r2
    it      eq
    beq.w   .Lreset_spin
    ldr     r3, .Lreset_warm_magic
    cmp     r0, r3
    ittt    eq
    ldreq   r0, .Lreset_warm_entry_ptr
    ldreq   r0, [r0]
    bxeq    r0
    str     r2, [r1]
    mov.w   r5, #0
    ldr     r7, .Lreset_reg_fill
    ldr     r0, .Lreset_msp_top_ptr
    ldr     r0, [r0]
    add.w   r6, r0, r5
    msr     msp, r6
    ldr     r1, .Lreset_psp_top_ptr
    ldr     r1, [r1]
    add.w   r6, r1, r5
    msr     psp, r6
    ldr     r2, .Lreset_poison_ptr
    ldrd    r2, r3, [r2]
    ldr     r4, .Lreset_msp_sz_ptr
    ldr     r4, [r4]
1:
    strd    r2, r3, [r0, #-8]!
    subs    r4, #8
    bhi     1b
    ldr     r4, .Lreset_psp_sz_ptr
    ldr     r4, [r4]
2:
    strd    r2, r3, [r1, #-8]!
    subs    r4, #8
    bhi     2b
    mrs     r0, control
    orr     r0, r0, #2
    msr     control, r0
    mov     r0, r5
    cbnz    r0, .Lreset_boot_guard_ptr
    b.w     rtkit_start

    .balign 4
.Lreset_boot_guard_ptr: .word 0x20000190
.Lreset_cold_magic:     .word 0xBAADF1AC
.Lreset_warm_magic:     .word 0xCAFE4B0B
.Lreset_warm_entry_ptr: .word 0x20000194
.Lreset_reg_fill:       .word 0x77777700
.Lreset_msp_top_ptr:    .word 0x001C2EE0
.Lreset_psp_top_ptr:    .word 0x001C2ED8
.Lreset_poison_ptr:     .word 0x001C2F20
.Lreset_msp_sz_ptr:     .word 0x001C2EE4
.Lreset_psp_sz_ptr:     .word 0x001E08AC
.Lreset_spin:
    b       .Lreset_spin
    mov     r8, r8
    .size   Reset_Handler, . - Reset_Handler

    .org 0x98
    .global Fault_Handler
    .type Fault_Handler, %function
Fault_Handler:
    ldr     r0, .Lfault_msp_limit
    cmp     sp, r0
1:
    blt     1b
    ldr     r1, .Lfault_msp_top_ptr
    ldr     r1, [r1]
    cmp     sp, r1
2:
    bgt     2b
    tst.w   lr, #4
    ite     eq
    mrseq   r0, msp
    mrsne   r0, psp
    stmdb   r0!, {r4-r11}
    tst.w   lr, #0x10
    it      eq
    vstmdbeq r0!, {s16-s31}
    str     lr, [r0, #-8]!
    sub.w   r0, r0, #0x18
    tst.w   lr, #4
    it      eq
    msreq   msp, r0
    mrs     r1, ipsr
    bl      sub_1748C
3:
    b       3b
    .short  0x0000

    .balign 4
.Lfault_msp_limit:   .word 0x200D36B0
.Lfault_msp_top_ptr: .word 0x001C2EE0
    .size   Fault_Handler, . - Fault_Handler

    .org 0xE8
    .global NMI_Handler
    .type NMI_Handler, %function
NMI_Handler:
    ldr     r0, .Lnmi_msp_limit
    cmp     sp, r0
1:
    blt     1b
    ldr     r1, .Lnmi_msp_top_ptr
    ldr     r1, [r1]
    cmp     sp, r1
2:
    bgt     2b
    tst.w   lr, #4
    ite     eq
    mrseq   r0, msp
    mrsne   r0, psp
    stmdb   r0!, {r4-r11}
    tst.w   lr, #0x10
    it      eq
    vstmdbeq r0!, {s16-s31}
    str     lr, [r0, #-8]!
    sub.w   r0, r0, #0x18
    tst.w   lr, #4
    ite     eq
    msreq   msp, r0
    msrne   psp, r0
    ldr     r5, .Lnmi_frame_slot
    mov     r4, lr
    mrs     r1, ipsr
    str     r0, [r5]
    bl      Default_Handler
    mov     lr, r4
    mov.w   r0, #0
    str     r0, [r5]
    tst.w   lr, #4
    ite     eq
    mrseq   r0, msp
    mrsne   r0, psp
    add.w   r0, r0, #0x20
    tst.w   lr, #0x10
    it      eq
    vldmiaeq r0!, {s16-s31}
    ldmia.w r0!, {r4-r11}
    tst.w   lr, #4
    ite     eq
    msreq   msp, r0
    msrne   psp, r0
    bx      lr
    .short  0x0000

    .balign 4
.Lnmi_msp_limit:   .word 0x200D36B0
.Lnmi_msp_top_ptr: .word 0x001C2EE0
.Lnmi_frame_slot:  .word 0x20024180
    .size   NMI_Handler, . - NMI_Handler

    .org 0x17C
    .global SVCall_Handler
    .type SVCall_Handler, %function
SVCall_Handler:
    mrs     r0, psp
    stmdb   r0!, {r4-r11}
    tst.w   lr, #0x10
    it      eq
    vstmdbeq r0!, {s16-s31}
    str     lr, [r0, #-8]!
    bl      rtkit_sched_switch
    ldr     lr, [r0], #8
    tst.w   lr, #0x10
    it      eq
    vldmiaeq r0!, {s16-s31}
    ldmia.w r0!, {r4-r11}
    msr     psp, r0
    bx      lr
    .size   SVCall_Handler, . - SVCall_Handler

    /* Default_Handler pinned at exact firmware VMA 0x00016F70 */
    .section .text.default_handler, "ax", %progbits
    .global Default_Handler
    .type Default_Handler, %function
Default_Handler:
    ldr     r0, .Ldef_msp_limit
    cmp     sp, r0
1:
    blt     1b
    ldr     r0, .Ldef_irq_disp_ptr
    ldr     r1, [r0, #0x28]!
    ldr     r2, [r1, #0x0C]
    movs    r1, #0
    bx      r2
    nop

    .balign 4
.Ldef_msp_limit:    .word 0x200D36B0
.Ldef_irq_disp_ptr: .word 0x2000D368
    .size   Default_Handler, . - Default_Handler
