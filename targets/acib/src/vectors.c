#include "acib/vectors.h"
#include "acib/memmap.h"
#include "acib/rtkit.h"

/*
 * Typed 48-entry primary ARMv7E-M vector table at VTOR = 0x00000000 (0xc0 B).
 * Verified byte-for-byte against raw acib[0x00..0xc0] by verify_load_image.py.
 */
ACIB_SECTION(".vectors") ACIB_USED
const struct acib_vector_table g_acib_vector_table = {
    .initial_msp   = ACIB_MSP_STACK_TOP,
    .reset         = (uintptr_t)Reset_Handler,
    .nmi           = (uintptr_t)NMI_Handler,
    .hard_fault    = (uintptr_t)Fault_Handler,
    .mem_manage    = (uintptr_t)Fault_Handler,
    .bus_fault     = (uintptr_t)Fault_Handler,
    .usage_fault   = (uintptr_t)Fault_Handler,
    .reserved_7_10 = {
        (uintptr_t)Fault_Handler,
        (uintptr_t)Fault_Handler,
        (uintptr_t)Fault_Handler,
        (uintptr_t)Fault_Handler,
    },
    .svcall        = (uintptr_t)SVCall_Handler,
    .debug_mon     = (uintptr_t)Fault_Handler,
    .reserved_13   = (uintptr_t)Fault_Handler,
    .pendsv        = (uintptr_t)SVCall_Handler,
    .systick       = (uintptr_t)SysTick_Handler,
    .irq0          = 0u,
    .meta_word_17  = 0x00010000u,
    .proto_ver_18  = 0x0003005Bu,
    .irq3_31       = { 0u },
};
