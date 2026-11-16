/*
 * main.c -- PHY bring-up.
 *
 * btp_main() is entered from the RTKit boot path with interrupts masked.  It
 * initialises the clocks, loads the calibration and brings up the receive and
 * transmit paths, then parks the core in WFI; everything after that happens in
 * interrupt handlers.
 */
#include "btp/btp.h"
#include "btp/rtkit.h"
#include "btp/clock.h"
#include "btp/trace.h"
#include "btp/status.h"
#include "btp/nvv.h"
#include "btp/syscfg.h"
#include "btp/txpwr.h"
#include "btp/rx.h"
#include "btp/tx.h"
#include "btp/frs.h"
#include "btp/mac.h"

/* @provenance RVA 0x58F0 btp_main */
void btp_main(void)
{
    bool warm_boot = warm_boot_detected();
    uint32_t idle_arg = 0;

    btp_early_hw_init();
    dbg_printf("\n[BTP]: (");
    dbg_printf("%s)\n", BTP_BUILD_STRING);

    phy_hw_init(warm_boot);
    btp_irq_init();
    rtk_platform_late_init();
    rx_init();
    frs_correlator_init();
    tx_init(warm_boot);

    dbg_printf("PHY init done rad_rev=%d\n", radio_revision_get());

    if (warm_boot)
        mac_send_warm_boot_ack();
    else
        warm_boot_set_state(1);

    for (;;)
        idle_arg = rtk_platform_idle(idle_arg);
}

/* @provenance RVA 0x5964 phy_reset_tx_and_radio_state */
int phy_reset_tx_and_radio(void)
{
    REG_CLEAR_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_ALT_RATE);
    REG_CLEAR_BITS(SEQ_TX_CFG, SEQ_TX_CFG_ALT_RATE);
    REG_CLEAR_BITS(TXDIG_CTRL, TXDIG_CTRL_PACKET_EN);
    REG_CLEAR_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_ALT_PATH);

    REG_CLEAR_BITS(RADIO_RESET, RADIO_RESET_TX_DIGITAL);
    REG_SET_BITS(RADIO_RESET, RADIO_RESET_RELEASE);
    return 0;
}

/* Pulse the PHY block resets. */
static void phy_pulse_resets(void)
{
    REG_SET_BITS(SYS_RESET_CLK_EN, SYS_RESET_ALL_MASK);
    clk_delay_ticks(1);
    REG_SET_BITS(SYS_RESET_ASSERT, SYS_RESET_ALL_MASK);
    clk_delay_ticks(1);
    REG_CLEAR_BITS(SYS_RESET_DEASSERT, SYS_RESET_ALL_MASK);
    clk_delay_ticks(1);
}

/* @provenance RVA 0x59A8 phy_hw_init */
/* @provenance RVA 0x5A64 _phy_outlined_assert_reset */
/* @provenance RVA 0x5A70 _phy_outlined_deassert_reset */
uint32_t phy_hw_init(bool warm_boot)
{
    uint32_t clk0;

    if (warm_boot) {
        modem_restore_registers();
        phy_pulse_resets();
    } else {
        modem_init();
        syscfg_init();
        txpwr_init();
        phy_pulse_resets();

        /* Both synthesiser banks out of power-down. */
        REG_CLEAR_BITS(RADIO_SYNTH_CTRL0(RADIO_BANK_B), RADIO_SYNTH_CTRL0_PD);
        REG_CLEAR_BITS(RADIO_SYNTH_CTRL1(RADIO_BANK_B), RADIO_SYNTH_CTRL1_PD);
        REG_CLEAR_BITS(RADIO_SYNTH_CTRL0(RADIO_BANK_A), RADIO_SYNTH_CTRL0_PD);
        REG_CLEAR_BITS(RADIO_SYNTH_CTRL1(RADIO_BANK_A), RADIO_SYNTH_CTRL1_PD);
        phy_reset_tx_and_radio();
    }

    REG_WRITE_FIELD(SEQ_TX_CFG, SEQ_TX_CFG_RAMP, SEQ_TX_CFG_RAMP_DEFAULT);
    REG_SET_BITS(SEQ_TX_CFG, SEQ_TX_CFG_RAMP_EN);

    REG_SET_BITS(SYS_CLK_CTRL2, SYS_CLK_CTRL2_MODEM_EN);
    REG_SET_BITS(SYS_CLK_CTRL2, SYS_CLK_CTRL2_PHY_EN);
    REG_SET_BITS(SYS_CLK_CTRL1, SYS_CLK_CTRL_EN);
    clk0 = SYS_CLK_CTRL0 | SYS_CLK_CTRL_EN;
    SYS_CLK_CTRL0 = clk0;
    return clk0;
}

/* @provenance RVA 0x5A7C phy_configure_synth_band_mux */
uint32_t phy_set_synth_band(bool high_band, bool tx_mode, unsigned band)
{
    uint32_t ctrl;

    REG_WRITE_MASKED(SEQ_RF_CTRL, SEQ_RF_CTRL_TX_MODE, tx_mode ? SEQ_RF_CTRL_TX_MODE : 0);
    REG_CLEAR_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_RX_MODE);
    REG_WRITE_MASKED(SEQ_RF_CTRL, SEQ_RF_CTRL_HIGH_BAND,
                     high_band ? SEQ_RF_CTRL_HIGH_BAND : 0);

    /* Two-bit synthesiser band mux. */
    switch (band) {
    case 0:
    case 3:
        REG_CLEAR_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_SYNTH_BAND_HI);
        ctrl = SEQ_RF_CTRL & ~SEQ_RF_CTRL_SYNTH_BAND_LO;
        break;
    case 1:
        REG_CLEAR_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_SYNTH_BAND_HI);
        ctrl = SEQ_RF_CTRL | SEQ_RF_CTRL_SYNTH_BAND_LO;
        break;
    case 2:
        REG_SET_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_SYNTH_BAND_HI);
        ctrl = SEQ_RF_CTRL & ~SEQ_RF_CTRL_SYNTH_BAND_LO;
        break;
    default:
        REG_SET_BITS(SEQ_RF_CTRL, SEQ_RF_CTRL_SYNTH_BAND_HI);
        ctrl = SEQ_RF_CTRL | SEQ_RF_CTRL_SYNTH_BAND_LO;
        break;
    }
    SEQ_RF_CTRL = ctrl;
    return ctrl;
}

/* @provenance RVA 0x5AE0 btp_early_hw_init */
int btp_early_hw_init(void)
{
    struct trace_event ev;

    systick_init(0xFFFFFF, false, false);
    trace_begin(&ev, TRACE_TYPE_INIT, TRACE_INIT_EARLY_HW, 0, systick_now());
    trace_log(TRACE_SITE_EARLY_HW_INIT_ENTER, &ev);

    radio_revision_detect();
    clk_enable();
    clk_set_prescaler(7);
    clk_set_mux_divider(7);

    ev.start_ts = 0;
    ev.end_ts = systick_now();
    trace_log(TRACE_SITE_EARLY_HW_INIT_EXIT, &ev);
    return 0;
}
