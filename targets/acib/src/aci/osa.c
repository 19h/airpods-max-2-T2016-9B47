#include "acib/aci.h"
#include "acib/bus.h"
#include "acib/libc.h"
#include "acib/rtkit.h"

/*
 * Operating System Abstraction (OSA) mailbox routing, buffer owner tagging,
 * task status inspection, and crash-log event snapshot helpers.
 */

#define ACIB_ROM_OSA_RELAY_TBL_ADDR   0x000F5434u

/* @provenance RVA 0x072570 sub_72570 */
void osa_buf_tag_owner(uint8_t *buf_body)
{
    uint32_t hdr = acib_read_unaligned_le32(buf_body - 4);
    hdr = (hdr & ~(7u << 28)) | (3u << 28);
    acib_write_unaligned_le32(buf_body - 4, hdr);
}

/* @provenance RVA 0x072580 sub_72580 */
uint32_t osa_crash_copy_recent_events(uint8_t *dst, uint32_t max_bytes)
{
    uint8_t last_idx = g_bt_ctrl_ctx[0x6ADu];
    if (last_idx == 0xFFu) {
        return 0u;
    }

    dst[0x38u] = last_idx;
    if (max_bytes < 28u) {
        return 1u;
    }

    uint8_t prev2 = (uint8_t)(last_idx - 2u);
    uint8_t prev1 = (uint8_t)(last_idx - 1u);
    uint32_t written = 1u;

    if (prev2 <= 0x3Bu) {
        memcpy_wrapper(dst, &g_bt_ctrl_ctx[28u * (uint32_t)prev1], 28u);
        written = 29u;
    }
    if (prev1 <= 0x3Bu) {
        memcpy_wrapper(dst + 28u, &g_bt_ctrl_ctx[28u * (uint32_t)last_idx], 28u);
        written += 28u;
    }
    return written;
}

/* @provenance RVA 0x0727b4 sub_727B4 */
void osa_task_clear_pending_flag(uint32_t task_id)
{
    g_osa_task_table[(16u * task_id) + 12u] = 0u;
}

/* @provenance RVA 0x072940 osa_mbx_get_relay_dest */
uint8_t osa_mbx_get_relay_dest(uint32_t msg_id)
{
    if (msg_id < 0xB000u) {
        const uint8_t *relay_tbl = (const uint8_t *)acib_addr_ptr(ACIB_ROM_OSA_RELAY_TBL_ADDR);
        return relay_tbl[msg_id >> 12];
    }
    rtk_abort_panic(
        "OSA_MBX_get_relay_destination_of_event: unknown destination for event 0x%04x",
        msg_id, 0u, 0u);
    return 0u;
}

/* @provenance RVA 0x0729ac sub_729AC */
uint16_t osa_task_get_msg_count(uint32_t task_id)
{
    return acib_read_unaligned_le16(&g_osa_task_table[(16u * task_id) + 8u]);
}
