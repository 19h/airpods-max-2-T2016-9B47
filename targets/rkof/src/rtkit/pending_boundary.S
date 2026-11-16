/*
 * Clean link-boundary stubs for external `pending_c` functions referenced by
 * reconstructed C11 modules. Every symbol here is classified as `pending_c`
 * in verification/ledger.json (never claimed as `reconstructed_c`).
 */

    .syntax unified
    .cpu cortex-m4
    .thumb

    .macro PENDING_STUB sym
    .section .text.pending.\sym, "ax", %progbits
    .weak \sym
    .type \sym, %function
\sym:
    movs    r0, #0
    movs    r1, #0
    bx      lr
    .endm

    PENDING_STUB rtkit_app_init
    PENDING_STUB rtkit_heap_alloc
    PENDING_STUB rtkit_heap_free
    PENDING_STUB sub_13328
    PENDING_STUB sub_141B0
    PENDING_STUB sub_141B8
    PENDING_STUB sub_15830
    PENDING_STUB sub_167E8
    PENDING_STUB sub_17C98
    PENDING_STUB sub_17CEC
    PENDING_STUB sub_1878C
    PENDING_STUB sub_189C4
    PENDING_STUB sub_18EC0
    PENDING_STUB sub_1A5A8
    PENDING_STUB sub_1A5F4
    PENDING_STUB sub_1A646
    PENDING_STUB sub_1AA14
    PENDING_STUB sub_1BA50
    PENDING_STUB sub_1BA72
    PENDING_STUB sub_22F14
    PENDING_STUB sub_23B78
    PENDING_STUB sub_C7A8C
    PENDING_STUB sub_F1C94
    PENDING_STUB sub_F1D1C
    PENDING_STUB sub_186A8C
    PENDING_STUB sub_186BE4
    PENDING_STUB sub_187222
    PENDING_STUB sub_187240
    PENDING_STUB sub_18726C
    PENDING_STUB sub_187A64
    PENDING_STUB sub_1880F8
    PENDING_STUB sub_188300
    PENDING_STUB sub_1887D8
    PENDING_STUB sub_18E930
    PENDING_STUB sub_19CBB4
    PENDING_STUB sub_19CC9C
    PENDING_STUB sub_1A234C
    PENDING_STUB sub_1A295C
    PENDING_STUB sub_1A2A3C
    PENDING_STUB sub_1A3204
    PENDING_STUB sub_1A375C
