/*
 * clock.c -- PHY clock controller, SysTick and the sequencer timestamp
 * counter, plus interrupt bring-up.
 *
 * SysTick is programmed as a free-running 24-bit down counter without an
 * interrupt; the short busy-waits in the RX/FRS paths are measured against it.
 * The 48-bit sequencer timestamp is the time base shared with the MAC.
 */
#include "btp/btp.h"
#include "btp/clock.h"
#include "btp/rtkit.h"
#include "btp/trace.h"

struct clock_state clk BTP_STATE(clock);

/* ---- clock controller ----------------------------------------------------- */

/* @provenance RVA 0x0264 btp_clock_enable */
void clk_enable(void)
{
    REG_SET_BITS(CLK_CTRL, CLK_CTRL_EN);
}

/* Spin for `ticks` + 1 ticks of the controller's free-running counter. */
/* @provenance RVA 0x0274 btp_clock_delay_ticks */
int clk_delay_ticks(int ticks)
{
    int deadline = (int)CLK_TICK + ticks + 1;

    while ((int)(deadline - CLK_TICK) > 0)
        ;
    return deadline;
}

/* @provenance RVA 0x028C IRQ5_Handler */
void clk_wakeup_isr(void)
{
    void (*callback)(void);

    REG_WRITE_MASKED(CLK_WAKE_IRQ, CLK_WAKE_IRQ_ACK | CLK_WAKE_IRQ_EN, CLK_WAKE_IRQ_ACK);
    nvic_disable_irq(IRQ_CLOCK_WAKEUP);

    callback = clk.wakeup_callback;
    if (callback) {
        clk.wakeup_callback = NULL;
        callback();
    }
}

/* Keep the clock running unconditionally so a panic can be reported. */
/* @provenance RVA 0x02C0 btp_clock_set_panic_mode */
void clk_enter_panic_mode(void)
{
    uint32_t ctrl = CLK_CTRL;

    CLK_CTRL = (ctrl & ~(CLK_CTRL_FORCE_ON | CLK_CTRL_AUTO_GATE)) + CLK_CTRL_FORCE_ON;
}

/* @provenance RVA 0x02D8 btp_clock_set_mux_divider */
void clk_set_mux_divider(uint8_t divider)
{
    REG_SET_BITS(CLK_CTRL, CLK_CTRL_MUX_UPDATE);
    clk_delay_ticks(0);
    CLK_CTRL = (CLK_CTRL & ~(RFIELD_MASK(CLK_CTRL_MUX_DIV) | CLK_CTRL_MUX_UPDATE))
               | FIELD_PREP(CLK_CTRL_MUX_DIV, divider);
}

/* @provenance RVA 0x0304 btp_clock_set_prescaler */
void clk_set_prescaler(uint8_t prescaler)
{
    REG_WRITE_FIELD(CLK_CTRL, CLK_CTRL_PRESCALER, prescaler);
}

/* @provenance RVA 0x0420 btp_aux_timer_isr */
void aux_timer_isr(void)
{
    if (AUX_TIMER_IRQ & AUX_TIMER_IRQ_EXPIRED)
        AUX_TIMER_IRQ = AUX_TIMER_IRQ_EXPIRED;
}

/* ---- SysTick -------------------------------------------------------------- */

/* @provenance RVA 0x0434 SysTick_Handler */
void systick_isr(void)
{
    clk.systick_overflows++;
}

/* @provenance RVA 0x0444 systick_init */
void systick_init(uint32_t reload, bool interrupt, bool core_clock)
{
    uint32_t csr = SYST_CSR_ENABLE;

    SYST_CSR = 0;
    clk.systick_reload = (reload & 0xFFFFFF) - 1;
    SYST_RVR = (reload & 0xFFFFFF) - 1;
    SYST_CVR = 0;
    if (interrupt)
        csr |= SYST_CSR_TICKINT;
    if (core_clock)
        csr |= SYST_CSR_CLKSOURCE;
    SYST_CSR = csr;
}

/* @provenance RVA 0x0474 systick_get_current_value */
uint32_t systick_now(void)
{
    return SYST_CVR;
}

/* @provenance RVA 0x04A0 systick_wait_cycles */
static void systick_wait(uint32_t cycles)
{
    uint32_t start = SYST_CVR;

    while (systick_elapsed(SYST_CVR, start) < cycles)
        ;
}

/* @provenance RVA 0x0498 systick_delay_cycles */
void systick_delay(uint32_t cycles)
{
    if (cycles > clk.systick_reload)
        cycles = clk.systick_reload;
    systick_wait(cycles);
}

/* ---- sequencer timestamp counter -------------------------------------------- */

static inline uint64_t seq_timer_read(volatile uint32_t *lo, volatile uint32_t *hi)
{
    return (uint64_t)*lo | ((uint64_t)(uint16_t)*hi << 32);
}

/* @provenance RVA 0x4AE0 btp_timer_get_current_timestamp */
uint64_t seq_timer_now(void)
{
    return seq_timer_read(&SEQ_TIMESTAMP_LO, &SEQ_TIMESTAMP_HI);
}

/* @provenance RVA 0x4AEC btp_timer_get_tx_trigger_timestamp */
uint64_t seq_timer_tx_trigger(void)
{
    return seq_timer_read(&SEQ_TX_TRIGGER_TS_LO, &SEQ_TX_TRIGGER_TS_HI);
}

/* @provenance RVA 0x4AF8 btp_timer_get_rx_trigger_timestamp */
uint64_t seq_timer_rx_trigger(void)
{
    return seq_timer_read(&SEQ_RX_TRIGGER_TS_LO, &SEQ_RX_TRIGGER_TS_HI);
}

/* @provenance RVA 0x4B04 btp_timer_delay_ticks */
void seq_timer_delay(uint64_t ticks)
{
    if (ticks) {
        uint64_t deadline = seq_timer_now() + ticks + 1;

        while (seq_timer_now() < deadline)
            ;
    }
}

/* @provenance RVA 0x4B3E btp_timer_wait_until_timestamp */
void seq_timer_wait_until(uint64_t deadline)
{
    while ((int64_t)(seq_timer_now() - deadline) < 0)
        ;
}

/* ---- interrupt bring-up ------------------------------------------------------- */

/* @provenance RVA 0x0CDC btp_irq_init_and_enable */
void btp_irq_init(void)
{
    struct trace_event ev;

    trace_begin(&ev, TRACE_TYPE_INIT, TRACE_INIT_IRQ, 0, systick_now());
    trace_log(TRACE_SITE_IRQ_INIT_ENTER, &ev);

    nvic_enable_irq(IRQ_MAC_IPC);
    nvic_enable_irq(IRQ_TX_SETUP);
    nvic_enable_irq(IRQ_TX_DONE);
    nvic_enable_irq(IRQ_RX_SETUP);
    nvic_enable_irq(IRQ_RX_DONE);
    nvic_enable_irq(IRQ_LB_CORR0_DONE);
    nvic_enable_irq(IRQ_LB_CORR1_DONE);
    nvic_enable_irq(IRQ_AFE_EL_CORR);
    irq_enable();

    ev.start_ts = 0;
    ev.end_ts = systick_now();
    trace_log(TRACE_SITE_IRQ_INIT_EXIT, &ev);
}
