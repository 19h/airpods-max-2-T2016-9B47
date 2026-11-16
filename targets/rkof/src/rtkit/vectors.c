#include "rkof/memmap.h"
#include "rkof/rtkit.h"

#if !defined(RKOF_HOST_BUILD)

/*
 * Architectural ARMv7E-M 48-entry exception vector table at VMA 0x00000000.
 * Matches raw ../../rkof[0x00..0xBF] byte-for-byte via linker symbol resolution.
 */
__attribute__((section(".vectors"), used))
void (*const g_rkof_vectors[48])(void) = {
    [0]  = NULL,
    [1]  = Reset_Handler,
    [2]  = NMI_Handler,
    [3]  = Fault_Handler,
    [4]  = Fault_Handler,
    [5]  = Fault_Handler,
    [6]  = Fault_Handler,
    [7]  = Fault_Handler,
    [8]  = Fault_Handler,
    [9]  = Fault_Handler,
    [10] = Fault_Handler,
    [11] = SVCall_Handler,
    [12] = Fault_Handler,
    [13] = Fault_Handler,
    [14] = SVCall_Handler,
    [15] = Default_Handler,
    [16] = Default_Handler,
    [17] = Default_Handler,
    [18] = Default_Handler,
    [19] = Default_Handler,
    [20] = Default_Handler,
    [21] = Default_Handler,
    [22] = Default_Handler,
    [23] = Default_Handler,
    [24] = Default_Handler,
    [25] = Default_Handler,
    [26] = Default_Handler,
    [27] = Default_Handler,
    [28] = Default_Handler,
    [29] = Default_Handler,
    [30] = Default_Handler,
    [31] = Default_Handler,
    [32] = Default_Handler,
    [33] = Default_Handler,
    [34] = Default_Handler,
    [35] = Default_Handler,
    [36] = Default_Handler,
    [37] = Default_Handler,
    [38] = Default_Handler,
    [39] = Default_Handler,
    [40] = Default_Handler,
    [41] = Default_Handler,
    [42] = Default_Handler,
    [43] = Default_Handler,
    [44] = Default_Handler,
    [45] = Default_Handler,
    [46] = Default_Handler,
    [47] = Default_Handler,
};

#endif /* !RKOF_HOST_BUILD */

/*
 * Pinned RAM_BSS state definitions matching STATE_*_BASE in memmap.h
 * and verified against build/rkof.map by tools/verify_layout.py.
 */
RKOF_STATE(".state.bss_start")     uint8_t  g_state_bss_start[4];
RKOF_STATE(".state.nmi_frame")     uint8_t  g_state_nmi_frame[0xA0];
RKOF_STATE(".state.carveout")      uint32_t g_state_carveout[8];
RKOF_STATE(".state.core_ptrs")     uintptr_t g_state_core_ptrs[4];
RKOF_STATE(".state.coproc_ref")    uint8_t  g_state_coproc_ref[4];
RKOF_STATE(".state.coproc_dyn")    uint8_t  g_state_coproc_dyn[0x80];
RKOF_STATE(".state.psp_stack")     uint8_t  g_state_psp_stack[0x800];
RKOF_STATE(".state.gtb_ctx")       uint32_t g_state_gtb_ctx;
RKOF_STATE(".state.phy_cal_ptrs")  uintptr_t g_state_phy_cal_ptrs[29];
RKOF_STATE(".state.sched_bitmap")  uint8_t  g_state_sched_bitmap[0x108];
RKOF_STATE(".state.sched_pending") uint8_t  g_state_sched_pending[4];
RKOF_STATE(".state.sched_ctx")     uint8_t  g_state_sched_ctx[0x80];
RKOF_STATE(".state.exc_state")     uint8_t  g_state_exc_state[0x7C];
RKOF_STATE(".state.panic_hook")    uint8_t  g_state_panic_hook[0x18];
RKOF_STATE(".state.spicy_log")     uintptr_t g_state_spicy_log;
RKOF_STATE(".state.heap_lock")     uint32_t g_state_heap_lock[4];
RKOF_STATE(".state.crash_magic")   uint8_t  g_state_crash_magic[0x4C];
RKOF_STATE(".state.fault_info")    uint8_t  g_state_fault_info[0x18];
RKOF_STATE(".state.bt_phy_otp")    uint8_t  g_state_bt_phy_otp[0x3C];
RKOF_STATE(".state.mpu")           uint8_t  g_state_mpu[0x190];
RKOF_STATE(".state.irq_arg")       uintptr_t g_state_irq_arg[74];
RKOF_STATE(".state.irq_fn")        uintptr_t g_state_irq_fn[74];
RKOF_STATE(".state.msp_stack")     uint8_t  g_state_msp_stack[0x1000];
RKOF_STATE(".state.irq_prio")      uint32_t g_state_irq_prio[2];
