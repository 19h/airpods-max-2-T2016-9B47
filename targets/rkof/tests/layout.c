#include <stddef.h>
#include "rkof/rkof.h"

/*
 * Compile-time struct layout & memory-map invariants for both ARMv7E-M (32-bit)
 * and host test builds.
 */

_Static_assert(sizeof(struct rkof_uuid_header) == 0x48u, "rkof_uuid_header size");
_Static_assert(offsetof(struct rkof_uuid_header, uuid) == 0x10u, "uuid offset");
_Static_assert(offsetof(struct rkof_uuid_header, meta_tlv_vma) == 0x28u, "meta_tlv_vma offset");
_Static_assert(offsetof(struct rkof_uuid_header, meta_tlv_size) == 0x2Cu, "meta_tlv_size offset");
_Static_assert(offsetof(struct rkof_uuid_header, rtkit_build_vma_dup) == 0x40u, "dup build offset");

_Static_assert(sizeof(struct rkof_tlv_record) == 0x08u, "rkof_tlv_record size");
_Static_assert(sizeof(struct rkof_fwsg_segment) == 0x20u, "rkof_fwsg_segment size");
_Static_assert(sizeof(struct rkof_fwsg_trailer) == 0x20u, "rkof_fwsg_trailer size");

/* W3 MAC <-> PHY <-> RKOF Shared SRAM Contracts */
_Static_assert(offsetof(struct phy_mac_mailbox, dekker_flag_mac) == 0x0C0u, "Dekker MAC flag");
_Static_assert(offsetof(struct phy_mac_mailbox, dekker_flag_phy) == 0x0C2u, "Dekker PHY flag");
_Static_assert(offsetof(struct phy_mac_mailbox, txpwr_table) == 0x0C4u, "MAC_REQUEST_TXPWR_TABLE_OFFSET");
_Static_assert(offsetof(struct phy_mac_mailbox, frs_scan_window) == 0x1E4u, "FRS scan window");
_Static_assert(offsetof(struct phy_mac_mailbox, resp_payload) == 0x400u, "PHY resp slot");
_Static_assert(offsetof(struct phy_mac_mailbox, crash_inline_copy) == 0x5C1u, "MAC crash copy");
_Static_assert(sizeof(struct phy_mac_mailbox) == 0x5D5u, "phy_mac_mailbox total");

_Static_assert(sizeof(struct phy_trace_ring) == 0x200u, "phy_trace_ring size");
_Static_assert(offsetof(struct phy_trace_ring, write_idx) == 0x1FCu, "trace write_idx");

_Static_assert(sizeof(struct phy_crash_record) == 0x98u, "phy_crash_record copy size");
_Static_assert(offsetof(struct phy_crash_record, error_code) == 0x40u, "crash error_code");
_Static_assert(offsetof(struct phy_crash_record, detail1_ptr) == 0x44u, "crash detail1_ptr");
_Static_assert(offsetof(struct phy_crash_record, detail1_len) == 0x48u, "crash detail1_len");

_Static_assert(sizeof(struct phy_nvv_container_hdr) == 0x08u, "phy_nvv_container_hdr size");
_Static_assert(sizeof(struct phy_warm_restore_pair) == 0x08u, "phy_warm_restore_pair size");
_Static_assert(offsetof(struct phy_status_region, slpr_state) == 0x00u, "slpr_state");
_Static_assert(offsetof(struct phy_status_region, slpr_reg_num) == 0x04u, "slpr_reg_num");
_Static_assert(offsetof(struct phy_status_region, warm_regs) == 0x08u, "warm_regs");
_Static_assert(sizeof(struct phy_status_region) == 0x20B3u, "phy_status_region exact clear size");

_Static_assert(sizeof(struct rkof_carveout_info) == 0x20u, "rkof_carveout_info size");
_Static_assert(sizeof(struct rkof_coproc_grant) == 0x10u, "rkof_coproc_grant size");

_Static_assert(offsetof(struct rkof_systick_dev, freq_hz) == 0x18u, "systick freq_hz");
_Static_assert(offsetof(struct rkof_systick_dev, ext_clk_flag) == 0x1Cu, "systick ext_clk");
_Static_assert(offsetof(struct rkof_systick_dev, ticks_per_100ns) == 0x28u, "systick 100ns");
_Static_assert(offsetof(struct rkof_systick_dev, ticks_per_us) == 0x2Cu, "systick us");
_Static_assert(offsetof(struct rkof_systick_dev, rem_ticks_lo) == 0x30u, "systick rem_lo");
_Static_assert(offsetof(struct rkof_systick_dev, rem_ticks_hi) == 0x34u, "systick rem_hi");
_Static_assert(offsetof(struct rkof_systick_dev, oneshot_mode) == 0x38u, "systick oneshot");

#if !defined(RKOF_HOST_BUILD)
_Static_assert(sizeof(void *) == 4u, "ARMv7E-M pointer size");
_Static_assert(sizeof(struct rkof_scanmedia_ctx) == 0x34u, "ARM rkof_scanmedia_ctx size");
_Static_assert(sizeof(struct rkof_wnvv_node) == 0x0Cu, "ARM rkof_wnvv_node size");
_Static_assert(sizeof(struct rkof_byte_ring) == 0x14u, "ARM rkof_byte_ring size");
#endif

/* Export dummy symbol so layout.o is non-empty */
int rkof_layout_checks_compiled(void);
int rkof_layout_checks_compiled(void)
{
    return 1;
}
