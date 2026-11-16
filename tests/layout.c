/*
 * layout.c -- compile-time proof of the recovered data model.
 *
 * Every constant below is an address or offset read off the image.  A mismatch
 * is a build failure, not a runtime surprise.
 */
#include "btp/btp.h"
#include "btp/rtkit.h"
#include "btp/clock.h"
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
#include "btp/trace.h"

#define CHECK(cond) _Static_assert((cond), #cond)
#define SPAN(from, to) ((to) - (from))

/* ---- shared memory ---------------------------------------------------------- */
CHECK(sizeof(struct nvv_blob_header) == 8);
CHECK(offsetof(struct mac_request, command) == 0x002);
CHECK(offsetof(struct mac_request, length) == 0x005);
CHECK(offsetof(struct mac_request, u.sub.id) == 0x008);
CHECK(offsetof(struct mac_request, u.sub.length) == 0x00A);
CHECK(offsetof(struct mac_request, u.sub.p) == 0x00C);
CHECK(offsetof(struct mac_request, u.sub.p.ovr.value) == 0x00E);
CHECK(offsetof(struct mac_request, u.sub.p.syscfg.data) == 0x010);
CHECK(offsetof(struct mac_request, u.band_table.band) == 0x02C);
CHECK(offsetof(struct mac_request, frs) == MAC_REQUEST_FRS_OFFSET);
CHECK(offsetof(struct mac_request, frs.channel_bitmap) == 0x1E6);
CHECK(offsetof(struct mac_request, frs.half_span) == 0x1F6);
CHECK(offsetof(struct mac_request, frs.coex_enable) == 0x1F9);
CHECK(offsetof(struct mac_mailbox, resp) == 0x400);
CHECK(offsetof(struct mac_response, status) == 0x004);
CHECK(offsetof(struct mac_response, length) == 0x005);
CHECK(offsetof(struct mac_response, payload) == 0x008);
CHECK(offsetof(struct mac_mailbox, frs_rssi) == 0x4C0);
CHECK(offsetof(struct mac_mailbox, frs_status) == 0x5C0);
CHECK(offsetof(struct mac_mailbox, crash) == 0x5C1);
CHECK(offsetof(struct mac_mailbox, frs_gain) == 0x5FE);

CHECK(sizeof(struct telemetry) == 0x2C);
CHECK(sizeof(struct version_info) == 0x7F);
CHECK(offsetof(struct phy_status_region, warm_restore) == 0x0008);
CHECK(offsetof(struct phy_status_region, telemetry) == 0x2008);
CHECK(offsetof(struct phy_status_region, version) == 0x2034);

CHECK(sizeof(struct trace_record) == 28);
CHECK(sizeof(struct trace_ring) == 0x200);
CHECK(offsetof(struct trace_ring, reserved) == 0x1FC);
CHECK(sizeof(struct crash_record) == 0x4C);
CHECK(offsetof(struct crash_record, error_code) == 0x40);
CHECK(sizeof(struct crash_descriptor) == 20);

/* ---- module state blocks (sizes match the gaps between their addresses) ---- */
CHECK(sizeof(struct clock_state) == SPAN(STATE_CLOCK_BASE, STATE_MAC_BASE));
CHECK(sizeof(struct mac_state) == SPAN(STATE_MAC_BASE, STATE_RX_OVERRIDES_BASE));
CHECK(sizeof(struct rx_overrides) == SPAN(STATE_RX_OVERRIDES_BASE, STATE_TX_OVERRIDES_BASE));
CHECK(sizeof(struct tx_overrides) == SPAN(STATE_TX_OVERRIDES_BASE, STATE_TXPWR_MODE_BASE));
CHECK(sizeof(struct txpwr_mode) == SPAN(STATE_TXPWR_MODE_BASE, STATE_LOOPBACK_BASE));
CHECK(sizeof(struct loopback_state) == SPAN(STATE_LOOPBACK_BASE, STATE_WARM_BOOT_BASE));
CHECK(sizeof(warm_boot_magic_t) == SPAN(STATE_WARM_BOOT_BASE, STATE_FORMAT_BASE));
CHECK(_Alignof(warm_boot_magic_t) == 4);
CHECK(FMT_SLOT_COUNT * FMT_SLOT_SIZE + 4 == SPAN(STATE_FORMAT_BASE, STATE_NVV_PRESENCE_BASE));
CHECK(sizeof(struct nvv_presence) == SPAN(STATE_NVV_PRESENCE_BASE, STATE_RTK_BASE));
CHECK(sizeof(struct rtk_state) == 0x20);
CHECK(offsetof(struct rtk_state, resched_pending) == 0x4D289A48 - STATE_RTK_BASE);
CHECK(offsetof(struct rtk_state, fault_count) == 0x4D289A4C - STATE_RTK_BASE);
CHECK(offsetof(struct rtk_state, exception_class) == 0x4D289A54 - STATE_RTK_BASE);

CHECK(sizeof(struct diag_state) == SPAN(STATE_DIAG_BASE, STATE_FRS_BASE));
CHECK(offsetof(struct diag_state, rssi_baseline) == 0x4D28A2CC - STATE_DIAG_BASE);
CHECK(offsetof(struct diag_state, buffers) == 0x4D28A2D0 - STATE_DIAG_BASE);
CHECK(sizeof(struct diag_buffer) == 16);

CHECK(sizeof(struct frs_state) == SPAN(STATE_FRS_BASE, STATE_RX_BASE));
CHECK(offsetof(struct frs_state, fixed_gain_word) == 0x4D28A302 - STATE_FRS_BASE);
CHECK(offsetof(struct frs_state, gain_rows) == 0x4D28A400 - STATE_FRS_BASE);
CHECK(offsetof(struct frs_state, channel_list) == 0x4D28A5C8 - STATE_FRS_BASE);
CHECK(offsetof(struct frs_state, pass_count) == 0x4D28A5D8 - STATE_FRS_BASE);
CHECK(offsetof(struct frs_state, rssi_rows) == 0x4D28A5DA - STATE_FRS_BASE);
CHECK(offsetof(struct frs_state, rssi_rows[1]) == 0x4D28A70A - STATE_FRS_BASE);
CHECK(offsetof(struct frs_state, saved_regs) == 0x4D28A96C - STATE_FRS_BASE);
CHECK(offsetof(struct frs_state, half_span) == 0x4D28A9C4 - STATE_FRS_BASE);

CHECK(sizeof(struct rx_state) == SPAN(STATE_RX_BASE, STATE_DPD_BASE));
CHECK(offsetof(struct rx_state, cfo_sum) == 0x4D28A9D4 - STATE_RX_BASE);
CHECK(offsetof(struct rx_state, packet_count) == 0x4D28A9E4 - STATE_RX_BASE);
CHECK(offsetof(struct rx_state, rssi_histogram) == 0x4D28A9E8 - STATE_RX_BASE);
CHECK(offsetof(struct rx_state, default_pkt_params) == 0x4D28AA0C - STATE_RX_BASE);
CHECK(offsetof(struct rx_state, coex_saved_mix) == 0x4D28AA2C - STATE_RX_BASE);
CHECK(offsetof(struct rx_state, maxgain) == 0x4D28AA40 - STATE_RX_BASE);
CHECK(offsetof(struct rx_state, saved_mode0) == 0x4D28AA80 - STATE_RX_BASE);

CHECK(sizeof(struct dpd_state) == SPAN(STATE_DPD_BASE, STATE_TX_BASE));
CHECK(offsetof(struct dpd_state, capture) == 0x4D28AAB0 - STATE_DPD_BASE);
CHECK(offsetof(struct dpd_state, mode) == 0x4D28B670 - STATE_DPD_BASE);

CHECK(sizeof(struct tx_state) == 8);
CHECK(offsetof(struct tx_state, saved_pa_bias_stage1) == 0x4D28B678 - STATE_TX_BASE);

CHECK(sizeof(struct txpwr_state) == SPAN(STATE_TXPWR_BASE, STATE_SYSCFG_BASE));
CHECK(offsetof(struct txpwr_state, global_offset) == 0x4D28B820 - STATE_TXPWR_BASE);
CHECK(offsetof(struct txpwr_state, band_override_enable) == 0x4D28B824 - STATE_TXPWR_BASE);
CHECK(offsetof(struct txpwr_state, band_override_value) == 0x4D28B844 - STATE_TXPWR_BASE);
CHECK(offsetof(struct txpwr_state, maxpwr) == 0x4D28B900 - STATE_TXPWR_BASE);
CHECK(offsetof(struct txpwr_state, pa_tables) == 0x4D28C900 - STATE_TXPWR_BASE);
CHECK(offsetof(struct txpwr_state, table) == 0x4D28CA00 - STATE_TXPWR_BASE);
CHECK(offsetof(struct txpwr_state, cal_offset) == 0x4D28CB20 - STATE_TXPWR_BASE);
CHECK(sizeof(struct txpwr_channel_limit) == 4);
CHECK(sizeof(struct txpwr_entry) == 4);

CHECK(sizeof(struct syscfg_state) == SPAN(STATE_SYSCFG_BASE, STATE_LOOPBACK_SAVED_BASE));
CHECK(sizeof(struct syscfg_header) == 12);
CHECK(offsetof(struct syscfg_state, rssi_cal) == 0x4D28CF18 - STATE_SYSCFG_BASE);
CHECK(sizeof(struct loopback_saved_radio) == 0x18);
CHECK(sizeof(struct loopback_stats) == 0x34);
CHECK(sizeof(struct nvv_tables) == SPAN(STATE_NVV_TABLES_BASE, STATE_MPU_BASE));
CHECK(offsetof(struct nvv_tables, hf_coeff) == 0x4D28CF68 - STATE_NVV_TABLES_BASE);
CHECK(offsetof(struct nvv_tables, mod_info) == 0x4D28CFA8 - STATE_NVV_TABLES_BASE);
CHECK(offsetof(struct nvv_tables, rx_maxgain_cal) == 0x4D28CFB8 - STATE_NVV_TABLES_BASE);
CHECK(offsetof(struct nvv_tables, tx_maxpwr_cal) == 0x4D28CFDC - STATE_NVV_TABLES_BASE);
CHECK(offsetof(struct rtk_mpu_state, regions) == 0x10);
CHECK(sizeof(struct rtk_mpu_state) == 0x190);
CHECK(sizeof(struct rtk_irq_state) == SPAN(STATE_RTK_IRQ_BASE, RAM_END));

/* ---- exception frame and stacks ------------------------------------------------- */
CHECK(sizeof(struct rtk_exception_frame) == 0x20);
CHECK(sizeof(struct rtk_metadata_u8) == 9);
CHECK(sizeof(struct rtk_image_metadata) == 0x79);
CHECK(offsetof(struct rtk_image_metadata, stack_guard) == 0x24);
CHECK(offsetof(struct rtk_image_metadata, data_reload) == 0x60);
CHECK(offsetof(struct rtk_image_metadata, slid) == 0x69);
CHECK(offsetof(struct rtk_exception_frame, exc_return) == 0x18);
CHECK(PSP_STACK_TOP - PSP_STACK_BASE == STACK_SIZE);
CHECK(MSP_STACK_TOP - MSP_STACK_BASE == STACK_SIZE);
CHECK(PSP_STACK_TOP == MSP_STACK_BASE);

/* ---- NVV catalogue ---------------------------------------------------------------- */
CHECK(NVV_TAG_GLOBAL_PHY_INITS == 0x080B);
CHECK(NVV_TAG_FRS_IQMM_AB_MIX1ST_BW3 == 0x11D9);
CHECK(NVV_P0_RF_SEQ == 1u && NVV_P0_LBIQMM_AB_LP == 0x80000000u);
CHECK(NVV_P1_FRS_IQMM_AB_MIX1ST_BW3 == 0x800u);
