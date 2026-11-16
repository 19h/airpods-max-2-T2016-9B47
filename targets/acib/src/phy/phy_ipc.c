#include "acib/phy_ipc.h"
#include "acib/bus.h"
#include "acib/libc.h"
#include "acib/memmap.h"
#include "acib/reconstructed_p2.h"
#include "acib/regs.h"
#include "acib/rtkit.h"

/*
 * MAC (acib) <-> W3 PHY (phyb) Shared Mailbox, Doorbell, Dekker Spinlock,
 * Fast RSSI Scan (FRS), Coexistence, Crash Reader, and SEQ_RF_CTRL Driver.
 *
 * Reconstructed from acib ROM 0x07a0dc..0x07dcae, 0x084e74..0x084f14, 0x097920.
 */

#define ACIB_ROM_RF_HW_PTR_ADDR       0x000F2A9Cu
#define ACIB_ROM_FRS_CNT_TBL_ADDR     0x000F5A07u
#define ACIB_ROM_FRS_CHAN_ALLOW_ADDR  0x000F5A0Fu

#define ACIB_RAM_ACL_PHY_CFG_ADDR     0x10000642u
#define ACIB_RAM_MODEM_LUT_INIT_ADDR  0x1000171Cu
#define ACIB_RAM_FRS_CHAN_PTRS_ADDR   0x1000172Cu
#define ACIB_RAM_FRS_FLAGS_1752_ADDR  0x10001752u
#define ACIB_RAM_FRS_FLAGS_1753_ADDR  0x10001753u
#define ACIB_RAM_FRS_FLAGS_1754_ADDR  0x10001754u
#define ACIB_RAM_FRS_AUX_SHADOW_ADDR  0x100017E7u
#define ACIB_RAM_PHY_VER_WORD_ADDR    0x10001868u
#define ACIB_RAM_CHIP_TAG_9B40_ADDR   0x10009B40u

/* External pending_c helpers invoked by higher-level MAC<->PHY routines */
extern uint32_t sub_7ECD4(uint32_t ev_base, uint32_t count);
extern uint32_t sub_7ECC6(uint32_t slot_ptr, uint32_t reg_addr, uint32_t val);
extern uint32_t sub_7E9F8(uint32_t end_ptr);
extern void     sub_DF764(void);
extern void     sub_7EE58(void);
extern int      sub_74240(uint32_t log_id);
extern void     sub_74256(uint32_t log_id, uint32_t val0);
extern void     sub_74270(uint32_t log_id, uint32_t val0, uint32_t val1);
extern void     sub_742B2(uint32_t log_id, uint32_t val0, uint32_t val1, uint32_t val2, uint32_t val3);
extern void     sub_7DC92(uint32_t err_id);
extern void     sub_6DF3C(uint32_t ext_irq);
extern void     sub_6DD70(uint32_t lock_arg);
extern void     sub_6DE10(void);
extern void     sub_740A8(uint32_t log_id, uint32_t line);
extern void     sub_740FE(uint32_t log_id, uint32_t line, uint32_t val0);
extern void     sub_35354(uint8_t *out_buf, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4);
extern void     sub_EBA60(uint32_t ctx_addr, uint32_t tag_addr, const uint8_t *buf);
extern uint32_t sub_7513C(const char *msg, uint32_t code);
extern void     sub_7835C(uint32_t delay_us);
extern void     sub_7D820(uint32_t arg);
extern int      sub_CF6B4(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3,
                          uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4);
extern void     sub_96458(uint32_t reg_id, uint32_t val);
extern void     sub_964AC(uint32_t val);
extern void     sub_96434(uint32_t reg_id, uint32_t val);

static inline void phy_modem_rmw_nibble(uint32_t reg_addr, uint32_t nibble_idx, uint32_t val4)
{
    uint32_t shift = nibble_idx * 4u;
    uint32_t cur = acib_mmio_read32(reg_addr);
    cur = (cur & ~(0xFu << shift)) | ((val4 & 0xFu) << shift);
    acib_mmio_write32(reg_addr, cur);
}

/* @provenance RVA 0x07a0dc sub_7A0DC */
uint32_t phy_hw_trig_frs_start_arm(uint32_t ev_base)
{
    uint32_t words = sub_7ECD4(ev_base, 11u);
    uint32_t ptr = ev_base + 4u * words;
    uint32_t added = sub_7ECC6(ptr, ACIB_PHY_IPC_FROM_MAC_DB_ADDR, ACIB_PHY_DB_FRS_START);
    return sub_7E9F8(ptr + 4u * added);
}

/* @provenance RVA 0x07a104 sub_7A104 */
uint32_t phy_hw_trig_frs_abort_arm(uint32_t ev_base)
{
    uint32_t words = sub_7ECD4(ev_base, 12u);
    uint32_t ptr = ev_base + 4u * words;
    uint32_t added = sub_7ECC6(ptr, ACIB_PHY_IPC_FROM_MAC_DB_ADDR, ACIB_PHY_DB_FRS_ABORT);
    return sub_7E9F8(ptr + 4u * added);
}

/* @provenance RVA 0x084e74 sub_84E74 */
void phy_ipc_on_cmd_ack_clear_flags(void)
{
    g_phy_ack_ctx[0x0Au] = 1u;
    uint32_t hw_base = acib_read_unaligned_le32(acib_addr_ptr(ACIB_ROM_RF_HW_PTR_ADDR));
    uint32_t r0 = acib_mmio_read32(hw_base);
    acib_mmio_write32(hw_base, r0 & ~0x4000u);
    uint32_t r28 = acib_mmio_read32(hw_base + 0x28u);
    acib_mmio_write32(hw_base + 0x28u, r28 & ~0x4000u);
}

/* @provenance RVA 0x084eb8 sub_84EB8 */
int phy_ipc_sync_shared_table_dekker(uint32_t cb_arg)
{
    uint8_t *req_base = (uint8_t *)acib_addr_ptr(g_phy_ipc_state.req_addr);
    volatile uint16_t *mac_lock = (volatile uint16_t *)(void *)(req_base + 0x0C0u);
    volatile uint16_t *phy_lock = (volatile uint16_t *)(void *)(req_base + 0x0C2u);

    *mac_lock = 1u;
    while (*phy_lock != 0u) {
        *mac_lock = 0u;
        while (*phy_lock != 0u) {
        }
        *mac_lock = 1u;
    }

    memcpy_wrapper(req_base + 0x0C4u, g_phy_table_shadow, sizeof(g_phy_table_shadow));
    *mac_lock = 0u;

    return sub_CF6B4(0u, 0u, 2u, 0u, 0u, 1u, 0x00084F21u, cb_arg, 0u);
}

/* @provenance RVA 0x07ca30 sub_7CA30 */
void phy_ipc_is_ready(uint32_t *out_magic)
{
    *out_magic = 0x17462216u;
}

/* @provenance RVA 0x07ca64 sub_7CA64 */
void phy_ipc_cmd_set_op2(void)
{
    uint32_t val = acib_mmio_read32(ACIB_BT_MODEM_CFG_218_ADDR);
    acib_mmio_write32(ACIB_BT_MODEM_CFG_218_ADDR, val | 1u);
}

/* @provenance RVA 0x07ca78 sub_7CA78 */
void phy_ipc_cmd_set_op3_u16(uint32_t enable)
{
    uint32_t val = acib_mmio_read32(ACIB_BT_MODEM_CFG_274_ADDR);
    val = (val & ~1u) | (enable & 1u);
    acib_mmio_write32(ACIB_BT_MODEM_CFG_274_ADDR, val);
}

/* @provenance RVA 0x07ca8c sub_7CA8C */
int phy_ipc_noop(void)
{
    return 1;
}

/* @provenance RVA 0x07ca90 sub_7CA90 */
int phy_ipc_cmd_set_op6_mode(uint8_t *out_b0, uint8_t *out_b1, uint32_t *out_word)
{
    uint32_t sts = acib_mmio_read32(ACIB_BT_MODEM_CAP_STS_ADDR);
    if ((sts & 0x20000u) != 0u) {
        return 0;
    }
    uint32_t cap = acib_mmio_read32(ACIB_BT_MODEM_CAP_VAL_ADDR) & 0x7FFFFFFFu;
    *out_word = cap;
    *out_b0 = (uint8_t)(cap & 0xFFu);
    *out_b1 = (uint8_t)((cap >> 8) & 7u);
    return 1;
}

/* @provenance RVA 0x07cabc sub_7CABC */
bool phy_ipc_wait_and_ack_cmd(int wait_flag, uint32_t timeout_us)
{
    bool ok;
    if (wait_flag != 0) {
        uint32_t start_cyc = acib_dwt_cyccnt_read();
        uint32_t budget_cyc = g_acib_clock_ctx.cycles_per_us * timeout_us;
        for (;;) {
            uint32_t sts = acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR);
            if ((sts & ACIB_PHY_STATUS_CMD_ACK) != 0u) {
                ok = true;
                break;
            }
            if ((acib_dwt_cyccnt_read() - start_cyc) > budget_cyc) {
                ok = false;
                break;
            }
        }
    } else {
        uint32_t sts = acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR);
        ok = ((sts & ACIB_PHY_STATUS_CMD_ACK) != 0u);
    }

    uint32_t ack = (acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & 0xFFFF0000u) |
                   ACIB_PHY_STATUS_CMD_ACK;
    acib_mmio_write32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR, ack);
    while ((acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & ACIB_PHY_STATUS_CMD_ACK) != 0u) {
    }
    sub_6DF3C(8u);
    return ok;
}

/* @provenance RVA 0x07cb4c sub_7CB4C */
bool phy_ipc_ring_cmd_doorbell(int wait_flag, uint32_t timeout_us)
{
    uint32_t db = acib_mmio_read32(ACIB_PHY_IPC_FROM_MAC_DB_ADDR);
    acib_mmio_write32(ACIB_PHY_IPC_FROM_MAC_DB_ADDR, (db & 0xFFFF0000u) | ACIB_PHY_DB_CMD_REQ);
    if (wait_flag == 0) {
        return true;
    }
    return phy_ipc_wait_and_ack_cmd(1, timeout_us);
}

/* @provenance RVA 0x07cb20 sub_7CB20 */
int phy_ipc_exec_cmd(uint16_t opcode, int wait_flag, uint32_t timeout_us)
{
    uint8_t *req_base = (uint8_t *)acib_addr_ptr(g_phy_ipc_state.req_addr);
    acib_write_unaligned_le16(req_base + 2u, opcode);
    if (phy_ipc_ring_cmd_doorbell(wait_flag, timeout_us)) {
        return 1;
    }
    sub_7DC92(0x01048008u);
    return 0;
}

/* @provenance RVA 0x07cb70 sub_7CB70 */
void *phy_ipc_clear_cmd_status_flag(const void *src, size_t n)
{
    return memcpy_wrapper(acib_addr_ptr(g_phy_ipc_state.req_addr), src, n);
}

/* @provenance RVA 0x07cb80 sub_7CB80 */
int phy_ipc_query_op3_ready(uint32_t lock_arg)
{
    int ready = 0;
    sub_6DD70(lock_arg);
    if (phy_ipc_exec_cmd(ACIB_PHY_CMD_QUERY_READY, 1, 10000u) != 0) {
        const uint8_t *resp = (const uint8_t *)acib_addr_ptr(g_phy_ipc_state.resp_addr);
        if (resp[8] == 0u) {
            sub_740A8(0x00048009u, 0x81C9u);
        } else {
            ready = 1;
        }
    }
    sub_6DE10();
    return ready;
}

/* @provenance RVA 0x07cbc0 sub_7CBC0 */
int phy_ipc_complete_async_cmd(uint32_t timeout_us, uint32_t slot)
{
    if (!phy_ipc_wait_and_ack_cmd(1, timeout_us)) {
        sub_740FE(0x0004800Au, 0x81CBu, timeout_us);
        return 0;
    }
    if (phy_mdm_fn_07cc08(0, (int32_t)slot) == 0 || phy_mdm_fn_07cc8c(0, (int32_t)slot) == 0) {
        sub_7DC92(0x0204800Bu);
    }
    return 1;
}

/* @provenance RVA 0x07c820 sub_7C820 */
void phy_ipc_init(void)
{
    g_phy_ipc_state.req_addr = ACIB_PHY_MBOX_REQ_BASE;
    *(uint16_t *)acib_addr_ptr(ACIB_RAM_CHIP_TAG_9B40_ADDR) = 0x1B21u;
    g_phy_ipc_state.resp_addr = ACIB_PHY_MBOX_RESP_BASE;

    sub_DF764();
    acib_mmio_write32(ACIB_BT_MODEM_LUT0_ADDR, 0x0B0B0B0Bu);
    acib_mmio_write32(ACIB_BT_MODEM_LUT1_ADDR, 0x2B3C150Fu);
    acib_mmio_write32(ACIB_BT_MODEM_LUT2_ADDR, 0x003C3C3Cu);

    uint8_t *req = (uint8_t *)acib_addr_ptr(g_phy_ipc_state.req_addr);
    acib_write_unaligned_le16(req + 0x0C0u, 0u);
    acib_write_unaligned_le16(req + 0x0C2u, 0u);

    uint32_t saved_probe[2];
    saved_probe[0] = acib_read_unaligned_le32(req + 0x1EEu);
    saved_probe[1] = acib_read_unaligned_le32(req + 0x1F2u);

    acib_write_unaligned_le16(req + 0x008u, 3u);
    acib_write_unaligned_le16(req + 0x00Au, 0x005Bu);
    acib_write_unaligned_le16(req + 0x00Cu, 1u);
    acib_write_unaligned_le16(req + 0x00Eu, 0u);

    if (phy_ipc_exec_cmd(ACIB_PHY_CMD_HANDSHAKE_INIT, 1, 10000u) != 0) {
        const uint8_t *ver_ptr = req + 0x1EEu;
        if (acib_memcmp(saved_probe, req + 0x1EEu, 8u) != 0) {
            sub_74270(0x00048013u, g_phy_ipc_state.resp_addr, ACIB_PHY_MBOX_LEGACY_RESP);
            g_phy_ipc_state.resp_addr = ACIB_PHY_MBOX_LEGACY_RESP;
        } else {
            ver_ptr = (const uint8_t *)acib_addr_ptr(g_phy_ipc_state.resp_addr) + 8u;
        }

        uint16_t ver_major = acib_read_unaligned_le16(ver_ptr + 0u);
        uint16_t ver_minor = acib_read_unaligned_le16(ver_ptr + 2u);
        uint16_t ver_patch = acib_read_unaligned_le16(ver_ptr + 4u);
        uint16_t ver_build = acib_read_unaligned_le16(ver_ptr + 6u);

        (void)phy_mdm_fn_07e084((int32_t)ver_major, (int32_t)ver_minor,
                                (int32_t)ver_patch, (int32_t)ver_build);
        phy_ipc_sync_shared_table_dekker(1u);

        const uint32_t *lut = (const uint32_t *)acib_addr_ptr(ACIB_RAM_MODEM_LUT_INIT_ADDR);
        acib_mmio_write32(ACIB_BT_MODEM_LUT0_ADDR, lut[0]);
        acib_mmio_write32(ACIB_BT_MODEM_LUT1_ADDR, lut[1]);
        acib_mmio_write32(ACIB_BT_MODEM_LUT2_ADDR, lut[2]);
        acib_mmio_write32(ACIB_BT_MODEM_LUT3_ADDR, lut[3]);

        *(uint32_t *)acib_addr_ptr(ACIB_RAM_PHY_VER_WORD_ADDR) = ver_major;
        sub_742B2(0x00048014u, ver_major, ver_minor, ver_patch, ver_build);
    } else {
        sub_74240(0x00048015u);
    }

    static const uint8_t gain0_nibbles[8] = { 6u, 7u, 0u, 0xFu, 0xFu, 0xFu, 0xFu, 0xFu };
    static const uint8_t gain1_nibbles[8] = { 8u, 9u, 0xBu, 0xAu, 0xCu, 0xDu, 0xFu, 0xFu };
    for (uint32_t i = 0u; i < 8u; i++) {
        phy_modem_rmw_nibble(ACIB_BT_MODEM_RX_GAIN0_ADDR, i, gain0_nibbles[i]);
    }
    for (uint32_t i = 0u; i < 8u; i++) {
        phy_modem_rmw_nibble(ACIB_BT_MODEM_RX_GAIN1_ADDR, i, gain1_nibbles[i]);
    }

    uint32_t mhz = g_acib_clock_ctx.ref_clk_hz / 1000000u;
    uint32_t clk_div = acib_mmio_read32(ACIB_BT_MODEM_CLK_DIV_ADDR);
    acib_mmio_write32(ACIB_BT_MODEM_CLK_DIV_ADDR, (clk_div & 0xFFFFFF00u) | (mhz & 0xFFu));

    memset(g_phy_ipc_state.cfg_bytes, 0, 0x0Eu);

    uint32_t c944 = acib_mmio_read32(ACIB_BT_MODEM_CTRL_944_ADDR);
    acib_mmio_write32(ACIB_BT_MODEM_CTRL_944_ADDR, (c944 & 0xFFFF0000u) | 0x0020u);

    g_phy_ipc_state.cfg_bytes[0x07u] = 0x92u;
    g_phy_ipc_state.cfg_bytes[0x0Cu] = 0x01u;
    acib_write_unaligned_le32(&g_phy_ipc_state.cfg_bytes[0x08u], 0x04030200u);
    acib_write_unaligned_le32(&g_phy_ipc_state.cfg_bytes[0x02u], 0xBA080A0Fu);

    c944 = acib_mmio_read32(ACIB_BT_MODEM_CTRL_944_ADDR);
    acib_mmio_write32(ACIB_BT_MODEM_CTRL_944_ADDR, c944 | 0x80000000u);

    sub_7EE58();
    uint32_t irq_en = acib_mmio_read32(ACIB_BT_MODEM_IRQ_EN_ADDR);
    acib_mmio_write32(ACIB_BT_MODEM_IRQ_EN_ADDR, irq_en | 1u);
}

/* @provenance RVA 0x07cd5c sub_7CD5C */
uint32_t phy_ipc_irq_handler(void)
{
    uint32_t sts = acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR);
    if ((sts & ACIB_PHY_STATUS_CMD_ACK) != 0u) {
        phy_ipc_on_cmd_ack_clear_flags();
        uint32_t ack = (acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & 0xFFFF0000u) |
                       ACIB_PHY_STATUS_CMD_ACK;
        acib_mmio_write32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR, ack);
        while ((acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & ACIB_PHY_STATUS_CMD_ACK) != 0u) {
        }
        sub_6DF3C(8u);
        return 0u;
    }

    sts = acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR);
    if ((sts & ACIB_PHY_STATUS_CRASH) != 0u) {
        const uint8_t *resp = (const uint8_t *)acib_addr_ptr(g_phy_ipc_state.resp_addr);
        uint32_t w0 = acib_read_unaligned_le32(resp + 0x1C1u);
        uint32_t w1 = acib_read_unaligned_le32(resp + 0x1C5u);
        uint32_t w2 = acib_read_unaligned_le32(resp + 0x1C9u);
        uint32_t w3 = acib_read_unaligned_le32(resp + 0x1CDu);
        uint32_t w4 = acib_read_unaligned_le32(resp + 0x1D1u);

        uint8_t formatted[32];
        sub_74256(0x0004800Du, w0);
        sub_742B2(0x0004800Eu, w1, w2, w3, w4);
        sub_35354(formatted, w0, w1, w2, w3, w4);
        sub_EBA60(0x100217CCu, 0x000FAC18u, formatted);

        uint32_t ack = (acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & 0xFFFF0000u) |
                       ACIB_PHY_STATUS_CRASH;
        acib_mmio_write32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR, ack);
        while ((acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & ACIB_PHY_STATUS_CRASH) != 0u) {
        }
        sub_6DF3C(8u);
        g_acib_panic_reason = 0x5Du;
        uint32_t rc = sub_7513C(NULL, 0u);
        if (rc != 0u) {
            rtk_abort_panic(NULL, 0u, 0u, 0u);
        }
        return rc;
    }

    sts = acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR);
    if ((sts & ACIB_PHY_STATUS_WARM_BOOT_ACK) != 0u) {
        acib_mmio_write32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR,
                          (sts & 0xFFFF0000u) | ACIB_PHY_STATUS_WARM_BOOT_ACK);
        while ((acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & ACIB_PHY_STATUS_WARM_BOOT_ACK) != 0u) {
        }
        sub_6DF3C(8u);
        return 0u;
    }

    if ((sts & ACIB_PHY_STATUS_FRS_DONE) != 0u) {
        acib_mmio_write32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR,
                          (sts & 0xFFFF0000u) | ACIB_PHY_STATUS_FRS_DONE);
        while ((acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & ACIB_PHY_STATUS_FRS_DONE) != 0u) {
        }
        sub_6DF3C(8u);
        return 0u;
    }

    if ((sts & 0xFFFFu) != 0u) {
        uint32_t unexpected = acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & 0xFFFFu;
        sub_74256(0x00048010u, unexpected);
        uint32_t clr = acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) | 0xFFFFu;
        acib_mmio_write32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR, clr);
        return clr;
    }
    return 0u;
}

/* @provenance RVA 0x07d234 sub_7D234 */
uint32_t phy_aci_if_enable_gates(void)
{
    uint32_t g0 = acib_mmio_read32(ACIB_ACI_IF_GATE_CTRL0_ADDR);
    acib_mmio_write32(ACIB_ACI_IF_GATE_CTRL0_ADDR, g0 | 0x300u);
    uint32_t g1 = acib_mmio_read32(ACIB_ACI_IF_GATE_CTRL1_ADDR);
    acib_mmio_write32(ACIB_ACI_IF_GATE_CTRL1_ADDR, g1 | 0x300u);
    return ACIB_ACI_IF_BASE;
}

/* @provenance RVA 0x07d258 sub_7D258 */
int phy_ipc_get_cached_version(void)
{
    return phy_ipc_exec_cmd(ACIB_PHY_CMD_SLEEP_ASYNC, 0, 0u);
}

/* @provenance RVA 0x07d264 sub_7D264 */
int phy_ipc_wait_warm_boot_ack(uint32_t timeout_us)
{
    uint32_t start_cyc = acib_dwt_cyccnt_read();
    uint32_t budget_cyc = g_acib_clock_ctx.cycles_per_us * timeout_us;

    while ((acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & ACIB_PHY_STATUS_WARM_BOOT_ACK) == 0u) {
        if ((acib_dwt_cyccnt_read() - start_cyc) > budget_cyc) {
            sub_740A8(0x0004801Eu, 0x81CDu);
            return 0;
        }
    }

    uint32_t ack = (acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & 0xFFFF0000u) |
                   ACIB_PHY_STATUS_WARM_BOOT_ACK;
    acib_mmio_write32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR, ack);
    while ((acib_mmio_read32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) & ACIB_PHY_STATUS_WARM_BOOT_ACK) != 0u) {
    }
    sub_6DF3C(8u);
    return 1;
}

/* @provenance RVA 0x07d2ec sub_7D2EC */
int phy_ipc_cmd_op9(void)
{
    const uint8_t *resp = (const uint8_t *)acib_addr_ptr(g_phy_ipc_state.resp_addr);
    return (int)(resp[0x1D5u] & 1u);
}

/* @provenance RVA 0x07d300 sub_7D300 */
void *phy_ipc_cmd_op10(void *dst)
{
    const uint8_t *resp = (const uint8_t *)acib_addr_ptr(g_phy_ipc_state.resp_addr);
    return memcpy_wrapper(dst, resp + 0x1D5u, 0x29u);
}

/* @provenance RVA 0x07d314 sub_7D314 */
uint32_t phy_ipc_cmd_op11_u8(uint32_t slot)
{
    return ACIB_BT_MODEM_SLOT_A_BASE + (0x50u * slot);
}

/* @provenance RVA 0x07d328 sub_7D328 */
uint32_t phy_ipc_cmd_op12_u8(uint32_t slot)
{
    return ACIB_BT_MODEM_SLOT_B_BASE + (0x50u * slot);
}

/* @provenance RVA 0x07d33c sub_7D33C */
uint32_t phy_ipc_cmd_op13_pair(uint32_t mode_idx, uint8_t *out_count)
{
    if (mode_idx <= 7u) {
        const uint8_t *cnt_tbl = (const uint8_t *)acib_addr_ptr(ACIB_ROM_FRS_CNT_TBL_ADDR);
        const uint32_t *ptr_tbl = (const uint32_t *)acib_addr_ptr(ACIB_RAM_FRS_CHAN_PTRS_ADDR);
        *out_count = cnt_tbl[mode_idx];
        return ptr_tbl[mode_idx];
    }
    *out_count = 0u;
    return 0u;
}

/* @provenance RVA 0x07d35c sub_7D35C */
uint32_t phy_ipc_frs_prepare_scan(uint32_t mode, uint8_t param, uint8_t dwell,
                                  uint8_t band_flags, uint8_t ctrl_flag,
                                  uint32_t unused6, uint32_t unused7,
                                  const void *custom_chan_bitmap)
{
    (void)unused6;
    (void)unused7;
    if (mode > 7u || mode == 3u) {
        return mode;
    }

    uint8_t *req = (uint8_t *)acib_addr_ptr(g_phy_ipc_state.req_addr);
    req[0x1E4u] = (uint8_t)mode;

    if (custom_chan_bitmap != NULL) {
        memcpy_wrapper(req + 0x1E6u, custom_chan_bitmap, 16u);
    } else {
        const uint32_t *ptr_tbl = (const uint32_t *)acib_addr_ptr(ACIB_RAM_FRS_CHAN_PTRS_ADDR);
        const uint8_t *chan_list = (const uint8_t *)acib_addr_ptr(ptr_tbl[mode]);
        const uint8_t *cnt_tbl = (const uint8_t *)acib_addr_ptr(ACIB_ROM_FRS_CNT_TBL_ADDR);
        memset(req + 0x1E6u, 0, 16u);
        uint32_t n_chans = cnt_tbl[mode];
        for (uint32_t i = 0u; i < n_chans; i++) {
            uint8_t ch = chan_list[i];
            req[0x1E6u + (ch >> 3)] |= (uint8_t)(1u << (ch & 7u));
        }
    }

    req[0x1E5u] = param;
    req[0x1F7u] = (uint8_t)((req[0x1F7u] & 0xF8u) | (band_flags & 7u));
    req[0x1F6u] = dwell;
    req[0x1F8u] = (uint8_t)((req[0x1F8u] & 0xFEu) | (ctrl_flag & 1u));

    uint8_t *resp = (uint8_t *)acib_addr_ptr(g_phy_ipc_state.resp_addr);
    resp[0x1C0u] = 0xFFu;
    return g_phy_ipc_state.resp_addr;
}

/* @provenance RVA 0x07d424 sub_7D424 */
int phy_ipc_frs_validate_param(uint32_t count, uint32_t mode, uint32_t chan_code)
{
    if (count == 0u || count > 8u) {
        return 0;
    }
    if (mode <= 7u && (((1u << mode) & 0xEEu) != 0u)) {
        const uint8_t *allow_tbl = (const uint8_t *)acib_addr_ptr(ACIB_ROM_FRS_CHAN_ALLOW_ADDR);
        for (uint32_t i = 0u; i < 14u; i++) {
            if (chan_code == allow_tbl[i]) {
                return 1;
            }
        }
        sub_74256(0x0204801Fu, chan_code);
        return 0;
    }
    return 0;
}

/* @provenance RVA 0x07d470 sub_7D470 */
bool phy_ipc_frs_is_scan_complete(void)
{
    uint8_t f1752 = *(const uint8_t *)acib_addr_ptr(ACIB_RAM_FRS_FLAGS_1752_ADDR);
    uint8_t f1754 = *(const uint8_t *)acib_addr_ptr(ACIB_RAM_FRS_FLAGS_1754_ADDR);
    const uint8_t *req = (const uint8_t *)acib_addr_ptr(g_phy_ipc_state.req_addr);
    const uint8_t *resp = (const uint8_t *)acib_addr_ptr(g_phy_ipc_state.resp_addr);

    if (f1752 != 0u && (((f1754 >> req[0x1E4u]) & 1u) != 0u)) {
        return true;
    }
    return resp[0x1C0u] != 0xFFu;
}

/* @provenance RVA 0x07d9d4 sub_7D9D4 */
int phy_ipc_frs_run_scan(uint32_t mode, uint8_t param, uint8_t dwell,
                         uint8_t band_flags, uint16_t custom_mult,
                         const void *custom_chan_bitmap, uint8_t *out_packed_189b,
                         uint16_t *out_elapsed_us)
{
    for (uint32_t off = 0u; off < 0xBDu; off += 3u) {
        out_packed_189b[off + 0u] = 0xFFu;
        out_packed_189b[off + 1u] = 0xF7u;
        out_packed_189b[off + 2u] = 0x7Fu;
    }

    uint32_t factor = (uint32_t)(band_flags & 3u);
    uint32_t base_units;
    uint8_t eff_dwell = dwell;
    if (custom_chan_bitmap != NULL) {
        base_units = 50u * (uint32_t)custom_mult;
    } else {
        factor *= 50u;
        const uint8_t *cfg = (const uint8_t *)acib_addr_ptr(ACIB_RAM_ACL_PHY_CFG_ADDR);
        base_units = ((uint32_t)cfg[6u * mode + 2u] + 15u) >> 4;
        eff_dwell = 8u;
    }

    phy_ipc_frs_prepare_scan(mode, param, eff_dwell, band_flags, 0u, param, dwell, custom_chan_bitmap);

    uint32_t t_start = acib_mmio_read32(ACIB_BT_TIMER_US_ADDR);
    acib_mmio_write32(ACIB_PHY_IPC_FROM_MAC_DB_ADDR, ACIB_PHY_DB_FRS_START);

    uint32_t initial_wait = (uint16_t)(base_units * factor);
    sub_7835C(initial_wait);
    uint32_t max_poll_us = 2u * initial_wait;
    uint32_t polled_us = 0u;

    while (!phy_ipc_frs_is_scan_complete()) {
        if (polled_us >= max_poll_us) {
            sub_74240(0x00048028u);
            acib_mmio_write32(ACIB_PHY_IPC_FROM_MAC_DB_ADDR, ACIB_PHY_DB_FRS_ABORT);
            return 0x18;
        }
        sub_7835C(5u);
        polled_us += 5u;
    }

    uint32_t dt = acib_mmio_read32(ACIB_BT_TIMER_US_ADDR) - t_start;
    *out_elapsed_us = (dt < 0xFFFFu) ? (uint16_t)dt : 0xFFFFu;

    const uint8_t *resp = (const uint8_t *)acib_addr_ptr(g_phy_ipc_state.resp_addr);
    sub_74256(0x00048027u, resp[0x1C0u]);

    for (uint32_t ch = 0u; ch < 0x7Eu; ch++) {
        int16_t sample = (int16_t)acib_read_unaligned_le16(resp + 0x0C0u + 2u * ch);
        uint32_t mask12 = (sample >= 0x7F00) ? 0x07FFu : 0x0FFFu;
        uint32_t val12 = (uint32_t)sample & mask12;

        uint32_t byte_idx = (ch & ~1u) + (ch >> 1);
        uint32_t cur24 = (uint32_t)acib_read_unaligned_le16(out_packed_189b + byte_idx) |
                         ((uint32_t)out_packed_189b[byte_idx + 2u] << 16);
        if ((ch & 1u) == 0u) {
            cur24 = (cur24 & 0xFFF000u) | val12;
        } else {
            cur24 = (cur24 & 0x000FFFu) | (val12 << 12);
        }
        acib_write_unaligned_le16(out_packed_189b + byte_idx, (uint16_t)(cur24 & 0xFFFFu));
        out_packed_189b[byte_idx + 2u] = (uint8_t)((cur24 >> 16) & 0xFFu);
    }

    uint8_t f1753 = *(const uint8_t *)acib_addr_ptr(ACIB_RAM_FRS_FLAGS_1753_ADDR);
    if ((f1753 & 1u) != 0u) {
        const uint8_t *cfg = (const uint8_t *)acib_addr_ptr(ACIB_RAM_ACL_PHY_CFG_ADDR);
        sub_7D820(cfg[6u * mode]);
    }
    return 0;
}

/* @provenance RVA 0x07db44 sub_7DB44 */
uint32_t phy_ipc_coex_ring_doorbell(const uint32_t *coex_word)
{
    uint8_t *req = (uint8_t *)acib_addr_ptr(g_phy_ipc_state.req_addr);
    acib_write_unaligned_le32(req + 0x1F9u, *coex_word);
    acib_mmio_write32(ACIB_PHY_IPC_FROM_MAC_DB_ADDR, ACIB_PHY_DB_COEX_UPDATE);
    return ACIB_W3_PHY_SEQ_BASE + 0xC00u;
}

/* @provenance RVA 0x07db60 sub_7DB60 */
int phy_ipc_frs_copy_results(uint32_t start_ch, size_t count, void *dst_rssi, size_t unused_end)
{
    (void)unused_end;
    const uint8_t *resp = (const uint8_t *)acib_addr_ptr(g_phy_ipc_state.resp_addr);
    if (resp[0x1C0u] != 0u) {
        (void)sub_74240(0x00048029u);
        return 0;
    }
    memcpy_wrapper(dst_rssi, resp + 0x0C0u + (2u * start_ch), 2u * count);
    uint8_t *aux_dst = (uint8_t *)acib_addr_ptr(ACIB_RAM_FRS_AUX_SHADOW_ADDR) + start_ch;
    memcpy_wrapper(aux_dst, resp + 0x1FEu + start_ch, count);
    return 1;
}

/* @provenance RVA 0x07dbbc sub_7DBBC */
int phy_ipc_frs_read_window(int16_t ch_offset, uint32_t count, void *dst_rssi)
{
    int32_t adj = (int32_t)ch_offset - 8;
    uint32_t start_ch = (adj > 0) ? (uint32_t)adj : 0u;
    uint32_t end_ch = start_ch + count;
    uint32_t clamped = (end_ch > 0x7Eu) ? (0x7Eu - start_ch) : (uint16_t)count;
    if (phy_ipc_frs_copy_results(start_ch, clamped, dst_rssi, end_ch) != 0) {
        return 0;
    }
    return 9;
}

/* @provenance RVA 0x07dbe4 sub_7DBE4 */
uint32_t phy_ipc_coex_noop(void)
{
    return 0x100226C4u;
}

/* @provenance RVA 0x07dca8 sub_7DCA8 */
uint8_t phy_ipc_coex_get_active(void)
{
    return g_acib_board_props.dev_flag;
}

/* @provenance RVA 0x097920 sub_97920 */
int phy_seq_rf_ctrl_apply_tx_mode(uint32_t profile_idx, uint32_t tx_param)
{
    uint32_t mode_word = acib_read_unaligned_le32(&g_rf_tx_mode_ctx[(16u * profile_idx) + 8u]);
    uint32_t encoded = 3u | ((mode_word & 0x3Fu) << 4);
    acib_write_unaligned_le32(&g_rf_tx_mode_ctx[0xB0u], tx_param);

    sub_96458(0x1Du, encoded);
    sub_964AC(encoded);
    sub_96434(0x24u, 0u);

    uint32_t seq_rf = acib_mmio_read32(ACIB_PHY_SEQ_RF_CTRL_ADDR);
    sub_96434(0x0Au, seq_rf & ~0x20000000u);
    return 0;
}
