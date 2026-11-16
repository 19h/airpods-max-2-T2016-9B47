#ifndef ACIB_VECTORS_H
#define ACIB_VECTORS_H

#include "acib/acib.h"

/*
 * Primary 48-entry Cortex-M vector table at VTOR = 0x00000000 (0xc0 bytes).
 * Entries [17] = 0x00010000 and [18] = 0x0003005b store boot/protocol version
 * words before sub_707F0 switches SCB->VTOR to 0x00000400.
 */
struct acib_vector_table {
    uintptr_t initial_msp;       /* [0]  0x1001bd80 */
    uintptr_t reset;             /* [1]  Reset_Handler (0x00000109) */
    uintptr_t nmi;               /* [2]  NMI_Handler   (0x000e4a95) */
    uintptr_t hard_fault;        /* [3]  Fault_Handler (0x000e4a51) */
    uintptr_t mem_manage;        /* [4]  Fault_Handler (0x000e4a51) */
    uintptr_t bus_fault;         /* [5]  Fault_Handler (0x000e4a51) */
    uintptr_t usage_fault;       /* [6]  Fault_Handler (0x000e4a51) */
    uintptr_t reserved_7_10[4];  /* [7..10] Fault_Handler */
    uintptr_t svcall;            /* [11] SVCall_Handler (0x000e4b15) */
    uintptr_t debug_mon;         /* [12] Fault_Handler (0x000e4a51) */
    uintptr_t reserved_13;       /* [13] Fault_Handler (0x000e4a51) */
    uintptr_t pendsv;            /* [14] SVCall_Handler (0x000e4b15) */
    uintptr_t systick;           /* [15] SysTick_Handler (0x000e58f1) */
    uintptr_t irq0;              /* [16] 0 */
    uintptr_t meta_word_17;      /* [17] 0x00010000 */
    uintptr_t proto_ver_18;      /* [18] 0x0003005b (major=3, minor=91) */
    uintptr_t irq3_31[29];       /* [19..47] 0 */
};

extern const struct acib_vector_table g_acib_vector_table;

#endif /* ACIB_VECTORS_H */
