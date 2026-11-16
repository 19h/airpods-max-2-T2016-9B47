/*
 * clock.h -- PHY clock controller, SysTick, the sequencer timestamp counter
 * and interrupt bring-up.
 */
#ifndef BTP_CLOCK_H
#define BTP_CLOCK_H

#include "btp.h"

/* Interrupt lines (NVIC numbers). */
enum btp_irq {
    IRQ_MAC_IPC         = 0,
    IRQ_TX_SETUP        = 1,
    IRQ_TX_DONE         = 2,
    IRQ_RX_SETUP        = 3,
    IRQ_RX_DONE         = 4,
    IRQ_CLOCK_WAKEUP    = 5,
    IRQ_AUX_TIMER       = 6,
    IRQ_LB_CORR0_DONE   = 12,
    IRQ_LB_CORR1_DONE   = 15,
    IRQ_AFE_EL_CORR     = 16,
    IRQ_DPD_CAPTURE     = 19,
    IRQ_DPD_RAM_WR      = 20,
};

struct clock_state {
    void   (*wakeup_callback)(void);    /* one-shot, run from the wake-up IRQ */
    uint32_t systick_overflows;
    uint32_t systick_reload;
};
extern struct clock_state clk;

/* Clock controller. */
void clk_enable(void);
int  clk_delay_ticks(int ticks);
void clk_enter_panic_mode(void);
void clk_set_mux_divider(uint8_t divider);
void clk_set_prescaler(uint8_t prescaler);
void clk_wakeup_isr(void);
void aux_timer_isr(void);

/* SysTick, used as a free-running down counter for short delays. */
void     systick_init(uint32_t reload, bool interrupt, bool core_clock);
void     systick_isr(void);
uint32_t systick_now(void);
void     systick_delay(uint32_t cycles);

/* Elapsed SysTick cycles between two down-counter samples. */
static inline uint32_t systick_elapsed(uint32_t now, uint32_t start)
{
    return start < now ? start - now + clk.systick_reload : start - now;
}

/* 48-bit sequencer timestamp counter. */
uint64_t seq_timer_now(void);
uint64_t seq_timer_tx_trigger(void);
uint64_t seq_timer_rx_trigger(void);
void     seq_timer_delay(uint64_t ticks);
void     seq_timer_wait_until(uint64_t deadline);

/* Interrupt bring-up at the end of btp_main()'s init sequence. */
void btp_irq_init(void);

#endif /* BTP_CLOCK_H */
