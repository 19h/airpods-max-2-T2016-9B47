/*
 * host_image.c -- host-side stand-in for the loader.
 *
 * On the target the pointer cells to the shared regions are initialised data.
 * On the host those regions live inside the bus windows, so the cells are
 * re-pointed here after every bus reset.  Module state is ordinary C data and
 * is simply cleared.
 */
#include "btp/btp.h"
#include "btp/rtkit.h"
#include "btp/mac.h"
#include "btp/rx.h"
#include "btp/tx.h"
#include "btp/txpwr.h"
#include "btp/dpd.h"
#include "btp/frs.h"
#include "btp/nvv.h"
#include "btp/status.h"
#include "btp/loopback.h"
#include "btp/syscfg.h"
#include "btp/clock.h"

#include <string.h>

void btp_host_image_reset(void);

void btp_host_image_reset(void)
{
    btp_bus_reset();

    mac_mbox = BTP_PTR(MAC_MAILBOX_BASE);
    trace_ring = BTP_PTR(TRACE_RING_BASE);
    crash_record = BTP_PTR(CRASH_RECORD_BASE);
    phy_status = BTP_PTR(PHY_STATUS_BASE);

    memset(&clk, 0, sizeof clk);
    memset(&mac, 0, sizeof mac);
    memset(&rx_ovr, 0, sizeof rx_ovr);
    memset(&rx, 0, sizeof rx);
    memset(&tx_ovr, 0, sizeof tx_ovr);
    memset(&tx, 0, sizeof tx);
    memset(&txpwr_mode, 0, sizeof txpwr_mode);
    memset(&txpwr, 0, sizeof txpwr);
    memset(&dpd, 0, sizeof dpd);
    memset(&frs, 0, sizeof frs);
    memset(&nvv, 0, sizeof nvv);
    memset(&nvv_presence, 0, sizeof nvv_presence);
    memset(&diag, 0, sizeof diag);
    memset(&lb, 0, sizeof lb);
    memset(&lb_saved, 0, sizeof lb_saved);
    memset(&syscfg, 0, sizeof syscfg);
    memset(&rtk, 0, sizeof rtk);
    memset(&rtk_mpu, 0, sizeof rtk_mpu);
    memset(&rtk_irq, 0, sizeof rtk_irq);
    warm_boot_magic = 0;
    radio_revision = 0;
}
