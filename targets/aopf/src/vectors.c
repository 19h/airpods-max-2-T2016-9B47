/*
 * vectors.c — Boot ARMv7E-M vector table at ROM_VECTORS (0x00000000..0x000000C0).
 *
 * Matches raw ../../aopf[0x00:0xC0] word-for-word via genuine function pointer
 * relocations:
 *   vec[0]      = 0x00000000 (Reset_Handler loads MSP/PSP from ROM_RODATA 0x72E78/7C)
 *   vec[1]      = Reset_Handler (0x0000693D)
 *   vec[2]      = NMI_Handler (0x00006A25)
 *   vec[3..10]  = HardFault_Default_Handler (0x000069D5)
 *   vec[11]     = SVCall_PendSV_Handler (0x00006AB9)
 *   vec[12..13] = HardFault_Default_Handler (0x000069D5)
 *   vec[14]     = SVCall_PendSV_Handler (0x00006AB9)
 *   vec[15..47] = SysTick_IRQ_Handler (0x0004E519)
 */

#include <stddef.h>
#include "aopf/memmap.h"
#include "aopf/rtkit.h"

typedef void (*aopf_vector_fn_t)(void);

#if defined(__arm__) || defined(__thumb__)
#define AOPF_VECTORS_SEC __attribute__((section(".vectors"), used))
#else
#define AOPF_VECTORS_SEC __attribute__((used))
#endif

AOPF_VECTORS_SEC
const aopf_vector_fn_t g_aopf_boot_vectors[AOPF_BOOT_VECTOR_COUNT] = {
    [0]  = (aopf_vector_fn_t)0,
    [1]  = Reset_Handler,
    [2]  = NMI_Handler,
    [3]  = HardFault_Default_Handler,
    [4]  = HardFault_Default_Handler,
    [5]  = HardFault_Default_Handler,
    [6]  = HardFault_Default_Handler,
    [7]  = HardFault_Default_Handler,
    [8]  = HardFault_Default_Handler,
    [9]  = HardFault_Default_Handler,
    [10] = HardFault_Default_Handler,
    [11] = SVCall_PendSV_Handler,
    [12] = HardFault_Default_Handler,
    [13] = HardFault_Default_Handler,
    [14] = SVCall_PendSV_Handler,
    [15] = SysTick_IRQ_Handler,
    [16] = SysTick_IRQ_Handler,
    [17] = SysTick_IRQ_Handler,
    [18] = SysTick_IRQ_Handler,
    [19] = SysTick_IRQ_Handler,
    [20] = SysTick_IRQ_Handler,
    [21] = SysTick_IRQ_Handler,
    [22] = SysTick_IRQ_Handler,
    [23] = SysTick_IRQ_Handler,
    [24] = SysTick_IRQ_Handler,
    [25] = SysTick_IRQ_Handler,
    [26] = SysTick_IRQ_Handler,
    [27] = SysTick_IRQ_Handler,
    [28] = SysTick_IRQ_Handler,
    [29] = SysTick_IRQ_Handler,
    [30] = SysTick_IRQ_Handler,
    [31] = SysTick_IRQ_Handler,
    [32] = SysTick_IRQ_Handler,
    [33] = SysTick_IRQ_Handler,
    [34] = SysTick_IRQ_Handler,
    [35] = SysTick_IRQ_Handler,
    [36] = SysTick_IRQ_Handler,
    [37] = SysTick_IRQ_Handler,
    [38] = SysTick_IRQ_Handler,
    [39] = SysTick_IRQ_Handler,
    [40] = SysTick_IRQ_Handler,
    [41] = SysTick_IRQ_Handler,
    [42] = SysTick_IRQ_Handler,
    [43] = SysTick_IRQ_Handler,
    [44] = SysTick_IRQ_Handler,
    [45] = SysTick_IRQ_Handler,
    [46] = SysTick_IRQ_Handler,
    [47] = SysTick_IRQ_Handler,
};
