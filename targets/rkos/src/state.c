#include "rkos/rkos.h"

/*
 * Pinned RAM_BSS state blocks covering 100% of `0x20000BA4..0x2001F068`
 * (`0x1E4C4` bytes) with zero wildcard `.bss` symbols.
 */

#ifdef RKOS_HOST
#define RKOS_STATE_SEC(name)
#else
#define RKOS_STATE_SEC(name) __attribute__((section(name), aligned(4), used))
#endif

RKOS_STATE_SEC(".state.warm")  rkos_state_warm_t  g_state_warm;
RKOS_STATE_SEC(".state.qspi")  rkos_state_qspi_t  g_state_qspi;
RKOS_STATE_SEC(".state.irq")   rkos_state_irq_t   g_state_irq;
RKOS_STATE_SEC(".state.meta")  rkos_state_meta_t  g_state_meta;
RKOS_STATE_SEC(".state.arena") rkos_state_arena_t g_state_arena;
RKOS_STATE_SEC(".state.psp")   rkos_state_psp_t   g_state_psp;
RKOS_STATE_SEC(".state.plat")  rkos_state_plat_t  g_state_plat;
RKOS_STATE_SEC(".state.sched") rkos_state_sched_t g_state_sched;
RKOS_STATE_SEC(".state.fault") rkos_state_fault_t g_state_fault;
RKOS_STATE_SEC(".state.msp")   rkos_state_msp_t   g_state_msp;
