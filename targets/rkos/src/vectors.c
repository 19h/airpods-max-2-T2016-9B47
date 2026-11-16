#include "rkos/rkos.h"

/*
 * Typed 48-entry ARMv7E-M boot vector table at RVA 0x00000000..0x000000C0.
 *
 * Because `link/rkos.ld` pins Reset_Handler (0x108), HardFault_Handler (0x290),
 * NMI_Handler (0x2E0), SVCall_Handler (0x374), and SysTick_Handler (0x34D8),
 * the linked `.vectors` section matches `../../rkos[0x00:0xC0]` byte-for-byte.
 */

typedef void (*rkos_vector_fn_t)(void);

#ifdef RKOS_HOST
#define RKOS_VEC_SECTION
#else
#define RKOS_VEC_SECTION __attribute__((section(".vectors"), used))
#endif

RKOS_VEC_SECTION const rkos_vector_fn_t g_vector_table[48] = {
    [0]  = (rkos_vector_fn_t)0,
    [1]  = Reset_Handler,
    [2]  = NMI_Handler,
    [3]  = HardFault_Handler,
    [4]  = HardFault_Handler,
    [5]  = HardFault_Handler,
    [6]  = HardFault_Handler,
    [7]  = HardFault_Handler,
    [8]  = HardFault_Handler,
    [9]  = HardFault_Handler,
    [10] = HardFault_Handler,
    [11] = SVCall_Handler,
    [12] = HardFault_Handler,
    [13] = HardFault_Handler,
    [14] = SVCall_Handler,
    [15] = SysTick_Handler,
    [16] = SysTick_Handler,
    [17] = SysTick_Handler,
    [18] = SysTick_Handler,
    [19] = SysTick_Handler,
    [20] = SysTick_Handler,
    [21] = SysTick_Handler,
    [22] = SysTick_Handler,
    [23] = SysTick_Handler,
    [24] = SysTick_Handler,
    [25] = SysTick_Handler,
    [26] = SysTick_Handler,
    [27] = SysTick_Handler,
    [28] = SysTick_Handler,
    [29] = SysTick_Handler,
    [30] = SysTick_Handler,
    [31] = SysTick_Handler,
    [32] = SysTick_Handler,
    [33] = SysTick_Handler,
    [34] = SysTick_Handler,
    [35] = SysTick_Handler,
    [36] = SysTick_Handler,
    [37] = SysTick_Handler,
    [38] = SysTick_Handler,
    [39] = SysTick_Handler,
    [40] = SysTick_Handler,
    [41] = SysTick_Handler,
    [42] = SysTick_Handler,
    [43] = SysTick_Handler,
    [44] = SysTick_Handler,
    [45] = SysTick_Handler,
    [46] = SysTick_Handler,
    [47] = SysTick_Handler,
};
