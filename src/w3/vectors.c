/*
 * vectors.c -- Cortex-M vector table and the thin IRQ entry points.
 *
 * The table has 48 slots.  Slot 0 is zero because the stacks are established
 * in software by rtk_init_stacks(); every system exception the firmware does
 * not handle itself is routed to the RTKit fault entry.  Unused device
 * interrupts share one empty handler.
 */
#include "btp/btp.h"
#include "btp/rtkit.h"
#include "btp/clock.h"
#include "btp/mac.h"
#include "btp/rx.h"
#include "btp/tx.h"
#include "btp/dpd.h"
#include "btp/loopback.h"
#include "btp/trace.h"
#include "btp/vectors.h"

typedef void (*vector_t)(void);

#define VECTOR(fn)  ((vector_t)(fn))

/* @provenance RVA 0x0C80 IRQ0_Handler */
void IRQ0_Handler(void)  { mac_isr(); }
/* @provenance RVA 0x0C84 IRQ1_Handler */
void IRQ1_Handler(void)  { (void)tx_setup_isr(); }
/* @provenance RVA 0x0C88 IRQ2_Handler */
void IRQ2_Handler(void)  { tx_done_isr(); }
/* @provenance RVA 0x0C8C IRQ3_Handler */
void IRQ3_Handler(void)  { (void)rx_setup_isr(); }
/* @provenance RVA 0x0C90 IRQ4_Handler */
void IRQ4_Handler(void)  { rx_done_isr(); }
/* @provenance RVA 0x0CD8 IRQ6_Handler */
void IRQ6_Handler(void)  { aux_timer_isr(); }
/* @provenance RVA 0x0CB4 IRQ12_Handler */
void IRQ12_Handler(void) { loopback_corr0_done_isr(); }
/* @provenance RVA 0x0CB8 IRQ15_Handler */
void IRQ15_Handler(void) { loopback_corr1_done_isr(); }

/* @provenance RVA 0x0CBC IRQ16_Handler */
void IRQ16_Handler(void)
{
    dbg_printf(">> ISR: AFE_EL_CORR_DONE\n");
    dbg_printf("<< ISR: AFE_EL_CORR_DONE\n");
}

/* @provenance RVA 0x0C94 IRQ19_Handler */
void IRQ19_Handler(void) { dpd_capture_done_isr(); }

/* @provenance RVA 0x0C98 IRQ20_Handler */
void IRQ20_Handler(void)
{
    dbg_printf(">> ISR: DPD_RAM_WR_AVAIL\n");
    dbg_printf("<< ISR: DPD_RAM_WR_AVAIL\n");
}

/* Shared empty handler for IRQ7-11, 13, 14, 17, 18 and 21-24 (one 2-byte stub in the image). */
void IRQ_Unused_Handler(void) { }

#if defined(__arm__) && !defined(BTP_HOST)
#define BTP_VECTORS __attribute__((section(".vectors"), used))
#else
#define BTP_VECTORS __attribute__((used))
#endif

BTP_VECTORS
const vector_t btp_vector_table[48] = {
    [0]  = 0,                           /* initial SP: stacks set up in software */
    [1]  = VECTOR(Reset_Handler),
    [2]  = VECTOR(Fault_Handler),       /* NMI */
    [3]  = VECTOR(Fault_Handler),       /* HardFault */
    [4]  = VECTOR(Fault_Handler),       /* MemManage */
    [5]  = VECTOR(Fault_Handler),       /* BusFault */
    [6]  = VECTOR(Fault_Handler),       /* UsageFault */
    [7]  = VECTOR(Fault_Handler),
    [8]  = VECTOR(Fault_Handler),
    [9]  = VECTOR(Fault_Handler),
    [10] = VECTOR(Fault_Handler),
    [11] = VECTOR(SVC_Handler),
    [12] = VECTOR(Fault_Handler),       /* DebugMonitor */
    [13] = VECTOR(Fault_Handler),
    [14] = VECTOR(SVC_Handler),         /* PendSV */
    [15] = VECTOR(systick_isr),
    [16 + IRQ_MAC_IPC]        = VECTOR(IRQ0_Handler),
    [16 + IRQ_TX_SETUP]       = VECTOR(IRQ1_Handler),
    [16 + IRQ_TX_DONE]        = VECTOR(IRQ2_Handler),
    [16 + IRQ_RX_SETUP]       = VECTOR(IRQ3_Handler),
    [16 + IRQ_RX_DONE]        = VECTOR(IRQ4_Handler),
    [16 + IRQ_CLOCK_WAKEUP]   = VECTOR(clk_wakeup_isr),
    [16 + IRQ_AUX_TIMER]      = VECTOR(IRQ6_Handler),
    [16 + 7]  = VECTOR(IRQ_Unused_Handler),
    [16 + 8]  = VECTOR(IRQ_Unused_Handler),
    [16 + 9]  = VECTOR(IRQ_Unused_Handler),
    [16 + 10] = VECTOR(IRQ_Unused_Handler),
    [16 + 11] = VECTOR(IRQ_Unused_Handler),
    [16 + IRQ_LB_CORR0_DONE]  = VECTOR(IRQ12_Handler),
    [16 + 13] = VECTOR(IRQ_Unused_Handler),
    [16 + 14] = VECTOR(IRQ_Unused_Handler),
    [16 + IRQ_LB_CORR1_DONE]  = VECTOR(IRQ15_Handler),
    [16 + IRQ_AFE_EL_CORR]    = VECTOR(IRQ16_Handler),
    [16 + 17] = VECTOR(IRQ_Unused_Handler),
    [16 + 18] = VECTOR(IRQ_Unused_Handler),
    [16 + IRQ_DPD_CAPTURE]    = VECTOR(IRQ19_Handler),
    [16 + IRQ_DPD_RAM_WR]     = VECTOR(IRQ20_Handler),
    [16 + 21] = VECTOR(IRQ_Unused_Handler),
    [16 + 22] = VECTOR(IRQ_Unused_Handler),
    [16 + 23] = VECTOR(IRQ_Unused_Handler),
    [16 + 24] = VECTOR(IRQ_Unused_Handler),
    /* 41..47 are zero */
};
