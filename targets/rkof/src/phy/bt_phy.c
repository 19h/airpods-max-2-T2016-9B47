#include "rkof/libc.h"
#include "rkof/memmap.h"
#include "rkof/phy.h"
#include "rkof/regs.h"
#include "rkof/rtkit.h"
#include "rkof/shared.h"
#include "rkof/wnvv.h"

extern void sub_141B0(void *log_pack);
extern void sub_189C4(void *log_pack, const char *arg0);
extern void sub_1A646(uintptr_t log_token, const void *log_pack);
extern void sub_1A5F4(uintptr_t log_token, const void *log_pack);
extern uint32_t sub_187A64(const char *str, char **endptr, int base);
extern void sub_23B78(uint32_t dst_id, uint32_t op, uint32_t subop, uint32_t len, const void *payload);
extern int sub_1880F8(uint32_t tag_fourcc, uint32_t flags, uintptr_t cb_fn, uint32_t ctx);

/* @provenance RVA 0xfdc04 sub_FDC04 */
void sub_FDC04(uint8_t *log_pack)
{
    *(uint32_t *)(void *)(log_pack + 0x14u) = 1u;
    *(uint32_t *)(void *)(log_pack + 0x10u) = 0xD3E016BDu;
    *(uint16_t *)(void *)(log_pack + 0x0Eu) = 0x0800u;
    *(uint32_t *)(void *)(log_pack + 0x0Au) = 0x001EA44Fu;
    *(uint16_t *)(void *)(log_pack + 0x08u) = 0x0420u;
    *(uint32_t *)(void *)(log_pack + 0x04u) = 0x001EA44Cu;
    *(uint32_t *)(void *)(log_pack + 0x00u) = 0x04200302u;
}

/* @provenance RVA 0xfdd1c sub_FDD1C */
int sub_FDD1C(uint32_t *power_state_out)
{
    uintptr_t pwr_obj = *RKOF_PTR(const uintptr_t *, RKOF_DATA_PWR_VTABLE_PTR_ADDR);
    const uintptr_t *vtable = RKOF_PTR(const uintptr_t *, pwr_obj);
    uintptr_t fn_addr = vtable[0x14u / sizeof(uintptr_t)];
    if (fn_addr == 0u) {
        rtkit_assert_fail("RTKitOS.c", 0x1916u, "rtk_power_get_domain");
    }
    typedef int (*pwr_get_fn)(uintptr_t obj, uint32_t domain_id, uint32_t *state_out);
    pwr_get_fn fn = (pwr_get_fn)fn_addr;
    return fn(RKOF_DATA_PWR_VTABLE_PTR_ADDR, 0x6Eu, power_state_out);
}

/* @provenance RVA 0xfdd48 sub_FDD48 */
int sub_FDD48(uint32_t power_state)
{
    uintptr_t pwr_obj = *RKOF_PTR(const uintptr_t *, RKOF_DATA_PWR_VTABLE_PTR_ADDR);
    const uintptr_t *vtable = RKOF_PTR(const uintptr_t *, pwr_obj);
    uintptr_t fn_addr = vtable[0x0Cu / sizeof(uintptr_t)];
    if (fn_addr == 0u) {
        rtkit_assert_fail("RTKitOS.c", 0x18FCu, "rtk_power_set_domain");
    }
    typedef int (*pwr_set_fn)(uintptr_t obj, uint32_t domain_id, uint32_t state);
    pwr_set_fn fn = (pwr_set_fn)fn_addr;
    return fn(RKOF_DATA_PWR_VTABLE_PTR_ADDR, 0x6Eu, power_state);
}

/* @provenance RVA 0xfdc38 sub_FDC38 */
uint32_t sub_FDC38(void *unused_ctx, uint8_t *dst_buf, uint32_t max_trace_bytes)
{
    (void)unused_ctx;
    uint32_t saved_pwr = 0u;
    (void)sub_FDD1C(&saved_pwr);
    if (saved_pwr != 2u) {
        (void)sub_FDD48(2u);
    }

    uint32_t trace_bytes = (max_trace_bytes > RKOF_PHY_TRACE_CAPTURE_MAX)
        ? RKOF_PHY_TRACE_CAPTURE_MAX
        : max_trace_bytes;

    memcpy_thunk(dst_buf, RKOF_PTR(const void *, RKOF_PHY_TRACE_RING_BASE), trace_bytes);
    *(uint32_t *)(void *)(dst_buf + trace_bytes) = saved_pwr;

    const struct phy_crash_record *crash = RKOF_PTR(
        const struct phy_crash_record *,
        RKOF_PHY_CRASH_RECORD_BASE
    );
    memcpy_thunk(dst_buf + trace_bytes + 4u, crash, RKOF_PHY_CRASH_RECORD_COPY_SIZE);

    uint32_t detail_len = crash->detail1_len;
    memcpy_thunk(
        dst_buf + trace_bytes + 0x9Cu,
        RKOF_PTR(const void *, crash->detail1_ptr),
        detail_len
    );

    if (saved_pwr != 2u) {
        (void)sub_FDD48(saved_pwr);
    }
    return trace_bytes + detail_len + 0x9Cu;
}

/* @provenance RVA 0xfdcb4 sub_FDCB4 */
uint32_t sub_FDCB4(void *unused_ctx, uint32_t *dst_words, uint32_t max_bytes)
{
    (void)unused_ctx;
    uint32_t saved_pwr = 0u;
    (void)sub_FDD1C(&saved_pwr);
    if (saved_pwr != 2u) {
        (void)sub_FDD48(2u);
    }

    const struct phy_crash_record *crash = RKOF_PTR(
        const struct phy_crash_record *,
        RKOF_PHY_CRASH_RECORD_BASE
    );
    uint32_t detail_len = crash->detail1_len;

    /* Firmware early-out at 0xFDCE0..0xFDCE6 marks overflow without restoring power */
    if (max_bytes < detail_len + 8u) {
        sub_1887C4();
        return 0u;
    }

    dst_words[0] = crash->error_code;
    if (crash->detail1_ptr != 0u) {
        memcpy_thunk(&dst_words[2], RKOF_PTR(const void *, crash->detail1_ptr), detail_len);
    } else {
        rkof_memset(&dst_words[2], 0, detail_len);
    }
    dst_words[1] = detail_len;

    if (saved_pwr != 2u) {
        (void)sub_FDD48(saved_pwr);
    }
    return (detail_len == 0u) ? 0u : (detail_len + 8u);
}

static void pmgr_clk_disable_wait(uintptr_t addr)
{
    mmio32_clear_bits(addr, 0x0Fu);
    while ((MMIO32_READ(addr) & 0xF0u) != 0u) {
    }
}

static void pmgr_clk_enable_wait(uintptr_t addr)
{
    mmio32_set_bits(addr, 0x0Fu);
    while ((MMIO32_READ(addr) & 0xF0u) != 0xF0u) {
    }
}

/* @provenance RVA 0xfe120 phy_shell_cmd */
int phy_shell_cmd(void *shell_ctx, const char *const *argv)
{
    (void)shell_ctx;
    if (rkof_strncmp(argv[1], "sleep", 5u) == 0) {
        uint32_t mux = MMIO32_READ(RKOF_PMGR_PHY_MUX_ADDR) & 0xF0000000u;
        MMIO32_WRITE(RKOF_PMGR_PHY_MUX_ADDR, mux);
        pmgr_clk_disable_wait(RKOF_PMGR_PHY_CLK20_ADDR);
        pmgr_clk_disable_wait(RKOF_PMGR_PHY_CLK10_ADDR);
        pmgr_clk_disable_wait(RKOF_PMGR_PHY_CLK18_ADDR);
        pmgr_clk_disable_wait(RKOF_PMGR_PHY_CLK0_ADDR);
        pmgr_clk_disable_wait(RKOF_PMGR_PHY_CLK8_ADDR);
        return 0;
    }

    if (rkof_strncmp(argv[1], "uart", 4u) == 0) {
        mmio32_rmw_mask(RKOF_UART_PINMUX_AUX_ADDR, 0x00000070u, 4u << 4u);
        mmio32_rmw_mask(RKOF_UART_PINMUX_AUX_ADDR, 0x0000F000u, 1u << 12u);
        mmio32_rmw_mask(RKOF_UART_PINMUX_AUX_ADDR + 4u, 0x00000070u, 4u << 4u);
        mmio32_rmw_mask(RKOF_UART_PINMUX_AUX_ADDR + 4u, 0x0000F000u, 1u << 12u);
        mmio32_rmw_mask(RKOF_UART_PINMUX_AUX_ADDR + 4u, 0x00000070u, 4u << 4u);
        mmio32_set_bits(RKOF_UART_PINMUX_AUX_ADDR + 4u, 0x00020000u);
        return 0;
    }

    if (rkof_strncmp(argv[1], "wake", 4u) == 0) {
        pmgr_clk_enable_wait(RKOF_PMGR_PHY_CLK8_ADDR);
        pmgr_clk_enable_wait(RKOF_PMGR_PHY_CLK0_ADDR);
        pmgr_clk_enable_wait(RKOF_PMGR_PHY_CLK18_ADDR);
        pmgr_clk_enable_wait(RKOF_PMGR_PHY_CLK10_ADDR);
        pmgr_clk_enable_wait(RKOF_PMGR_PHY_CLK20_ADDR);
        return 0;
    }

    if (rkof_strncmp(argv[1], "crash", 5u) == 0) {
        uint32_t mode = sub_187A64(argv[2], NULL, 10);
        if ((uint8_t)mode <= 2u) {
            uint8_t msg[8] = { 0 };
            *(uint32_t *)(void *)msg = 0x0001001Du;
            msg[4] = (uint8_t)mode;
            sub_23B78(0u, 1u, 0u, 5u, msg);
            return 0;
        }
    }

    uint8_t log_pack[16] = { 0 };
    sub_189C4(log_pack, argv[0]);
    sub_1A5F4(0x200E6CC2u, log_pack);
    return -1;
}

struct wnvv_replay_spec {
    uint16_t tag_id;
    uint16_t is_triples;
};

/* @provenance RVA 0xfd7c0 bt_phy_otp_init */
int bt_phy_otp_init(void)
{
    uint8_t log_pack[32] = { 0 };
    sub_FDC04(log_pack);
    sub_1A646(0x200E6C44u, log_pack);

    uint32_t pll = MMIO32_READ(RKOF_RF_PMGR_PLL_CFG_ADDR);
    pll = (pll & ~0x01FFFFFFu) | 0x019374BCu;
    MMIO32_WRITE(RKOF_RF_PMGR_PLL_CFG_ADDR, pll);

    MMIO32_WRITE(RKOF_PMGR_OTP_PWR_ADDR, 2u);
    sub_18A134(100ULL);
    MMIO32_WRITE(RKOF_PMGR_OTP_PWR_ADDR, 3u);
    sub_18A134(1000ULL);

    uint32_t *otp_state = RKOF_PTR(uint32_t *, STATE_BT_PHY_OTP_BASE);
    otp_state[0x38u / 4u] = 0u;
    otp_state[0x34u / 4u] = 0u;
    otp_state[0x30u / 4u] = 0u;

    struct rkof_scanmedia_ctx scan;
    rkof_memset(&scan, 0, sizeof(scan));
    scan.media_id = 0u;
    scan.start_off = 0x20003328u;
    scan.scan_len = 0x2230u;
    scan.reader = NULL;
    scan.callback = sub_FE2B0;
    sub_FDFD0(&scan);

    int pwr_rc = sub_FDE94(2u);
    if (pwr_rc != 0) {
        rtkit_assert_fail_hex64("bt_phy.c", 22u, "bt_phy_otp_init", (uint32_t)pwr_rc, 0ULL);
    }

    rkof_memset(&scan, 0, sizeof(scan));
    scan.media_id = 1u;
    scan.start_off = 0u;
    scan.scan_len = 0x1F00u;
    scan.reader = sub_FDEC0;
    scan.callback = sub_FE2B0;
    sub_FDFD0(&scan);

    pwr_rc = sub_FDE94(0u);
    if (pwr_rc != 0) {
        rtkit_assert_fail_hex64("bt_phy.c", 16u, "bt_phy_otp_init", (uint32_t)pwr_rc, 0ULL);
    }

    bt_phy_mmio_pre_otp();
    rkof_memset(RKOF_PTR(void *, RKOF_PHY_STATUS_SRAM_BASE), 0, RKOF_PHY_STATUS_CLEAR_SIZE);
    sub_FDD90(1);

    uint16_t tag_len = 0u;
    const uint8_t *blk_copy = sub_FE504(0x080Cu, &tag_len);
    if (blk_copy != NULL) {
        uint16_t rem = tag_len;
        const uint32_t *words = (const uint32_t *)(const void *)blk_copy;
        while (rem != 0u) {
            uint32_t dst_addr = words[0];
            uint32_t copy_bytes = words[1];
            words += 2;
            uint32_t *dst_words = RKOF_PTR(uint32_t *, dst_addr);
            for (uint32_t off = 0u; off < copy_bytes; off += 4u) {
                *dst_words++ = *words++;
            }
            rem = (uint16_t)(rem - copy_bytes - 8u);
        }
    }

    static const struct wnvv_replay_spec replay_tags[16] = {
        { 0x080Bu, 0u }, { 0x0821u, 1u }, { 0x0834u, 0u }, { 0x0836u, 1u },
        { 0x1166u, 0u }, { 0x1167u, 1u }, { 0x080Du, 0u }, { 0x0822u, 1u },
        { 0x118Eu, 0u }, { 0x118Fu, 1u }, { 0x0C00u, 1u }, { 0x0833u, 0u },
        { 0x0835u, 1u }, { 0x0C01u, 1u }, { 0x116Du, 0u }, { 0x116Eu, 1u },
    };

    for (uint32_t idx = 0u; idx < 16u; idx++) {
        const uint8_t *payload = sub_FE504(replay_tags[idx].tag_id, &tag_len);
        if (payload != NULL) {
            const uint32_t *words = (const uint32_t *)(const void *)payload;
            if (replay_tags[idx].is_triples != 0u) {
                sub_FDDF8(words, tag_len);
            } else {
                sub_FDDDC(words, tag_len);
            }
        }
    }

    const uint8_t *c02_payload = sub_FE504(0x0C02u, &tag_len);
    if (c02_payload != NULL) {
        sub_FDDF8((const uint32_t *)(const void *)c02_payload, tag_len);
    }

    uint8_t *nv_base = RKOF_PTR(uint8_t *, RKOF_PHY_NV_CAL_SRAM_BASE);
    uint32_t nv_off = 0u;
    while (nv_off < RKOF_PHY_NV_CAL_SRAM_SIZE) {
        const struct phy_nvv_container_hdr *hdr =
            (const struct phy_nvv_container_hdr *)(const void *)(nv_base + nv_off);
        if (hdr->magic != 0x4E56u) {
            break;
        }
        nv_off += (uint32_t)hdr->payload_len + 8u;
    }

    const struct rkof_nvv_mdmi_spec *mdmi_specs = RKOF_PTR(
        const struct rkof_nvv_mdmi_spec *,
        RKOF_RODATA_NVV_MDMI_TABLE_ADDR
    );
    const uint8_t **cal_ptrs = RKOF_PTR(const uint8_t **, STATE_PHY_CAL_PTRS_BASE);
    uint16_t cal_lens[29] = { 0 };

    for (uint32_t idx = 0u; idx < 29u; idx++) {
        uint16_t found_len = 0u;
        const uint8_t *found_ptr = sub_FE504(mdmi_specs[idx].tag_id, &found_len);
        cal_ptrs[idx] = found_ptr;
        if (found_ptr != NULL) {
            cal_lens[idx] = found_len;
            if (mdmi_specs[idx].expected_len != 0u && found_len != mdmi_specs[idx].expected_len) {
                cal_ptrs[idx] = NULL;
            }
        }
    }

    struct phy_nvv_container_hdr *out_hdr =
        (struct phy_nvv_container_hdr *)(void *)(nv_base + nv_off);
    out_hdr->payload_len = 0u;
    out_hdr->magic = 0x4E56u;
    out_hdr->flags = 0u;
    out_hdr->crc16 = 0u;

    uint8_t *out_cursor = nv_base + nv_off + 8u;
    for (uint32_t idx = 0u; idx < 29u; idx++) {
        const uint8_t *src_ptr = cal_ptrs[idx];
        if (src_ptr == NULL) {
            continue;
        }
        uint16_t item_len = cal_lens[idx];
        uint32_t projected = (nv_off + 8u) + (uint32_t)out_hdr->payload_len + (uint32_t)item_len;
        if (projected > RKOF_PHY_NV_CAL_LIMIT) {
            break;
        }

        uint16_t tag_id = mdmi_specs[idx].tag_id;
        if (tag_id <= 0x7Fu) {
            uint16_t packed = (uint16_t)(((uint16_t)tag_id << 8u) | (uint16_t)src_ptr[0]);
            *(uint16_t *)(void *)out_cursor = packed;
            out_cursor += 2u;
            out_hdr->payload_len = (uint16_t)(out_hdr->payload_len + 2u);
        } else if ((tag_id >> 11u) == 0u) {
            uint16_t packed = (uint16_t)(0x8000u | tag_id | (uint16_t)(item_len << 11u));
            *(uint16_t *)(void *)out_cursor = packed;
            memcpy_thunk(out_cursor + 2u, src_ptr, item_len);
            uint16_t rec_sz = (uint16_t)(item_len + 2u);
            out_cursor += rec_sz;
            out_hdr->payload_len = (uint16_t)(out_hdr->payload_len + rec_sz);
        } else {
            *(uint16_t *)(void *)out_cursor = (uint16_t)(0xC000u | tag_id);
            *(uint16_t *)(void *)(out_cursor + 2u) = item_len;
            memcpy_thunk(out_cursor + 4u, src_ptr, item_len);
            uint16_t rec_sz = (uint16_t)(item_len + 4u);
            out_cursor += rec_sz;
            out_hdr->payload_len = (uint16_t)(out_hdr->payload_len + rec_sz);
        }
    }

    sub_FDD90(0);
    MMIO32_WRITE(RKOF_PHY_STATUS_SRAM_BASE, 0u);
    bt_phy_mmio_init_seq();

    mmio32_rmw_mask(RKOF_UART_PINMUX_TX_ADDR, 0x00000070u, 1u << 4u);
    mmio32_rmw_mask(RKOF_UART_PINMUX_RX_ADDR, 0x00000070u, 1u << 4u);

#if defined(RKOF_HOST_BUILD)
    uintptr_t bphy_cb = (uintptr_t)sub_FDC38;
    uintptr_t mgrc_cb = (uintptr_t)sub_FDCB4;
#else
    uintptr_t bphy_cb = (uintptr_t)sub_FDC38;
    uintptr_t mgrc_cb = (uintptr_t)sub_FDCB4;
#endif

    if (sub_1880F8(0x42504859u, 0x100u, bphy_cb, 0u) != 0) {
        rtkit_assert_fail("bt_phy.c", 120u, "bt_phy_otp_init");
    }
    if (sub_1880F8(0x4372674Du, 0x401u, mgrc_cb, 0u) != 0) {
        rtkit_assert_fail("bt_phy.c", 126u, "bt_phy_otp_init");
    }

    rkof_memset(log_pack, 0, 16u);
    sub_141B0(log_pack);
    sub_1A646(0x200E6C6Au, log_pack);

    void *shell_ctx = (void *)(*RKOF_PTR(const uintptr_t *, STATE_CORE_PTRS_BASE + 4u));
    uint32_t sh_rc = rtkit_shell_register_cmd(shell_ctx, "phy", "phy", phy_shell_cmd);
    if (sh_rc != 0u) {
        rtkit_assert_fail_hex64("bt_phy.c", 66u, "bt_phy_otp_init", 0ULL, (uint64_t)sh_rc);
    }
    return 0;
}
