#include "acib/acib.h"
#include "acib/aci.h"
#include "acib/memmap.h"
#include "acib/phy_ipc.h"
#include "acib/regs.h"
#include "acib/rtkit.h"
#include "acib/shared.h"
#include "acib/vectors.h"
#include <stddef.h>
#include <stdint.h>

/*
 * Compile-time ABI, memory map, and MAC<->PHY shared contract assertions.
 * Compiled on both freestanding ARMv7E-M (-target armv7em-none-eabi) and
 * native host (cc -DACIB_HOST_TEST=1).
 */

/* Segment boundary invariants from fwsg v1 + acib.i64 */
_Static_assert(ACIB_ROM_VECTORS_BASE    == 0x00000000u, "ROM_VECTORS base");
_Static_assert(ACIB_ROM_VECTORS_SIZE    == 0x000000C0u, "ROM_VECTORS size");
_Static_assert(ACIB_RTKIT_HEADER_BASE   == 0x000000C0u, "RTKit uuid header base");
_Static_assert(ACIB_RTKIT_HEADER_SIZE   == 0x00000048u, "RTKit uuid header size");
_Static_assert(ACIB_RESET_HANDLER_RVA   == 0x00000108u, "Reset_Handler RVA");
_Static_assert(ACIB_SECONDARY_VTOR_BASE == 0x00000400u, "Secondary VTOR base");
_Static_assert(ACIB_SECONDARY_VTOR_SIZE == 0x00000240u, "Secondary VTOR size (144 entries)");
_Static_assert(ACIB_ROM_RODATA_BASE     == 0x000EFEA8u, "ROM_RODATA base");
_Static_assert(ACIB_ROM_RODATA_END      == 0x000F984Cu, "ROM_RODATA end");
_Static_assert(ACIB_ROM_RODATA_SIZE     == 0x000099A4u, "ROM_RODATA size");
_Static_assert(ACIB_RAM_DATA_BASE       == 0x10000000u, "RAM_DATA base");
_Static_assert(ACIB_RAM_DATA_END        == 0x1000AC1Eu, "RAM_DATA end");
_Static_assert(ACIB_RAM_BSS_BASE        == 0x1000AC1Eu, "RAM_BSS base");
_Static_assert(ACIB_RAM_BSS_END         == 0x1002D2C4u, "RAM_BSS end");
_Static_assert(ACIB_RAM_BSS_SIZE        == 0x000226A6u, "RAM_BSS size");

/* MAC (acib) <-> PHY (phyb) shared SRAM request window @ 0x4D280000 */
_Static_assert(ACIB_PHY_MBOX_BASE        == 0x4D280000u, "MAC_MAILBOX_BASE");
_Static_assert(ACIB_PHY_MBOX_RESP_BASE   == 0x4D280400u, "B0 PHY response base");
_Static_assert(ACIB_PHY_MBOX_LEGACY_RESP == 0x4D2801E6u, "Legacy PHY response base");
_Static_assert(sizeof(struct acib_phy_mbox_req) == 0x200u, "acib_phy_mbox_req size");
_Static_assert(offsetof(struct acib_phy_mbox_req, flags0)            == 0x000u, "req +0x000");
_Static_assert(offsetof(struct acib_phy_mbox_req, opcode)            == 0x002u, "req +0x002");
_Static_assert(offsetof(struct acib_phy_mbox_req, args_u16)          == 0x008u, "req +0x008");
_Static_assert(offsetof(struct acib_phy_mbox_req, mac_want_lock)     == 0x0C0u, "req +0x0C0 Dekker MAC");
_Static_assert(offsetof(struct acib_phy_mbox_req, phy_want_lock)     == 0x0C2u, "req +0x0C2 Dekker PHY");
_Static_assert(offsetof(struct acib_phy_mbox_req, shared_link_table) == 0x0C4u, "req +0x0C4 288B table");
_Static_assert(offsetof(struct acib_phy_mbox_req, frs_mode)          == 0x1E4u, "req +0x1E4 FRS mode");
_Static_assert(offsetof(struct acib_phy_mbox_req, frs_param)         == 0x1E5u, "req +0x1E5 FRS param");
_Static_assert(offsetof(struct acib_phy_mbox_req, frs_chan_bitmap)   == 0x1E6u, "req +0x1E6 FRS bitmap");
_Static_assert(offsetof(struct acib_phy_mbox_req, frs_dwell_ticks)   == 0x1F6u, "req +0x1F6 FRS dwell");
_Static_assert(offsetof(struct acib_phy_mbox_req, frs_band_flags)    == 0x1F7u, "req +0x1F7 FRS band");
_Static_assert(offsetof(struct acib_phy_mbox_req, frs_ctrl_flags)    == 0x1F8u, "req +0x1F8 FRS ctrl");
_Static_assert(offsetof(struct acib_phy_mbox_req, coex_grant_word)   == 0x1F9u, "req +0x1F9 unaligned coex");

/* MAC (acib) <-> PHY (phyb) shared SRAM response window @ 0x4D280400 */
_Static_assert(sizeof(struct acib_phy_mbox_resp) == 0x280u, "acib_phy_mbox_resp size");
_Static_assert(offsetof(struct acib_phy_mbox_resp, fw_version)      == 0x008u, "resp +0x008 version");
_Static_assert(offsetof(struct acib_phy_mbox_resp, frs_rssi_q)      == 0x0C0u, "resp +0x0C0 RSSI[126]");
_Static_assert(offsetof(struct acib_phy_mbox_resp, frs_status)      == 0x1C0u, "resp +0x1C0 FRS status");
_Static_assert(offsetof(struct acib_phy_mbox_resp, crash_words)     == 0x1C1u, "resp +0x1C1 20B crash");
_Static_assert(offsetof(struct acib_phy_mbox_resp, cal_telemetry)   == 0x1D5u, "resp +0x1D5 41B cal");
_Static_assert(offsetof(struct acib_phy_mbox_resp, frs_aux_metrics) == 0x1FEu, "resp +0x1FE aux[126]");

/* Shared W3 PHY MMIO registers overlapping phyb */
_Static_assert(ACIB_PHY_SEQ_RF_CTRL_ADDR       == 0x4D154038u, "SEQ_RF_CTRL @ 0x4d154038");
_Static_assert(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR == 0x4D154C64u, "PHY_IPC_TO_MAC_STS @ 0x4d154c64");
_Static_assert(ACIB_PHY_IPC_FROM_MAC_DB_ADDR   == 0x4D154C68u, "PHY_IPC_FROM_MAC_DB @ 0x4d154c68");

/* Fixed-width RTKit & subsystem state structs */
_Static_assert(sizeof(struct acib_rtkit_uuid_header) == 0x48u, "uuid v5 header == 72B");
_Static_assert(offsetof(struct acib_rtkit_uuid_header, image_uuid)   == 0x10u, "uuid offset");
_Static_assert(offsetof(struct acib_rtkit_uuid_header, metadata_rva) == 0x28u, "meta_tlv_vma offset");
_Static_assert(offsetof(struct acib_rtkit_uuid_header, rom_end_rva)  == 0x30u, "rom_end_rva offset");
_Static_assert(sizeof(struct acib_fault_summary) == 0x38u, "fault summary == 56B");
_Static_assert(sizeof(struct acib_clock_ctx)     == 0x44u, "clock ctx == 68B");
_Static_assert(sizeof(struct acib_board_props)   == 0x10u, "board props == 16B");
_Static_assert(sizeof(struct acib_phy_ipc_state) == 0x3Cu, "phy_ipc_state == 60B");

/* 32-bit ARM target pointer-sized ABI checks */
#if UINTPTR_MAX == 0xFFFFFFFFu
_Static_assert(sizeof(struct acib_vector_table) == 0xC0u, "48-entry vector table == 192B");
_Static_assert(offsetof(struct acib_vector_table, systick)      == 0x3Cu, "SysTick vector #15");
_Static_assert(offsetof(struct acib_vector_table, meta_word_17) == 0x44u, "meta word #17");
_Static_assert(offsetof(struct acib_vector_table, proto_ver_18) == 0x48u, "proto ver #18");
_Static_assert(sizeof(struct acib_tm_buf_cb) == 0x08u, "tm_buf_cb == 8B on ARM32");
#endif

int acib_layout_proof_anchor(void);
int acib_layout_proof_anchor(void)
{
    return (int)sizeof(struct acib_phy_mbox_req);
}
