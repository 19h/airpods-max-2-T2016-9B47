#include "host_bus.h"
#include "acib/aci.h"
#include "acib/libc.h"
#include "acib/memmap.h"
#include "acib/phy_ipc.h"
#include "acib/regs.h"
#include "acib/rtkit.h"
#include "acib/shared.h"
#include <stdio.h>
#include <string.h>

static uint32_t s_checks_run;
static uint32_t s_checks_failed;

#define CHECK(cond) do { \
    s_checks_run++; \
    if (!(cond)) { \
        s_checks_failed++; \
        fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define EXPECT_PANIC(stmt) do { \
    g_host_panic.armed = true; \
    g_host_panic.triggered = false; \
    if (setjmp(g_host_panic.env) == 0) { \
        stmt; \
    } \
    g_host_panic.armed = false; \
    CHECK(g_host_panic.triggered); \
} while (0)

/* --- 1. Boot, MPU 7-Region Setup, CPU Clock Calibration & Board Props --- */
static void test_boot_mpu_and_clocks(void)
{
    acib_host_reset();
    g_host_spy.timer_step_per_read  = 25u;          /* 25 us step (>= 20 us window) */
    g_host_spy.cyccnt_step_per_read = 25u * 192u;   /* 4800 cycles per 25 us -> 192 MHz */

    rtk_early_hw_init();

    /* Verify MPU enabled on 0xE000ED94 */
    CHECK((acib_host_get_mmio32(ACIB_MPU_CTRL_ADDR) & ACIB_MPU_CTRL_ENABLE) != 0u);

    /* Inspect MMIO write trace to confirm Region 5 maps 0x4D280000 (MAC_MAILBOX_BASE) */
    bool saw_region5_rbar = false;
    bool saw_region5_rasr = false;
    for (uint32_t i = 0; i < g_host_mmio_write_count; i++) {
        if (g_host_mmio_writes[i].addr == ACIB_MPU_RBAR_ADDR &&
            (g_host_mmio_writes[i].val & 0xFFFFF000u) == ACIB_PHY_MBOX_BASE) {
            saw_region5_rbar = true;
        }
        if (g_host_mmio_writes[i].addr == ACIB_MPU_RASR_ADDR &&
            (g_host_mmio_writes[i].val & 0x3Fu) == ((0x0Bu << 1) | 1u)) {
            saw_region5_rasr = true;
        }
    }
    CHECK(saw_region5_rbar);
    CHECK(saw_region5_rasr);

    /* Verify CPU frequency calibration from 1 MHz timer + DWT CYCCNT */
    acib_clock_timing_init();
    CHECK(g_acib_clock_ctx.calibrated_cpu_hz == 192000000u);
    CHECK(g_acib_clock_ctx.cycles_per_us == 192u);
    CHECK(g_acib_clock_ctx.low_freq_mode == 0u);
    CHECK(g_acib_clock_ctx.fallback_cpu_hz == 160000000u);

    /* When 1 MHz timer is frozen, acib_get_calibrated_cpu_hz falls back cleanly */
    g_host_spy.timer_step_per_read = 0u;
    CHECK(acib_get_calibrated_cpu_hz() == 160000000u);

    /* Verify RTKit board property tags loaded from real ROM_RODATA (0xF966F..0xF9693) */
    acib_load_rtkit_board_props();
    uint32_t expected_btm1 = acib_read_unaligned_le32(acib_addr_ptr(0x000F966Fu));
    uint32_t expected_btm2 = acib_read_unaligned_le32(acib_addr_ptr(0x000F967Bu));
    CHECK(g_acib_board_props.btm1 == expected_btm1);
    CHECK(g_acib_board_props.btm2 == expected_btm2);

    /* FPGA platform ID check (0x520DC0D8 low16 == 0xFC00) */
    acib_host_set_mmio32(ACIB_SOC_PLAT_ID_ADDR, 0xABCDFC00u);
    CHECK(acib_soc_is_fpga_emulation() == 1u);
    acib_host_set_mmio32(ACIB_SOC_PLAT_ID_ADDR, 0xABCD0500u);
    CHECK(acib_soc_is_fpga_emulation() == 0u);

    /* Stack watermark fill ("RTKSTACK") & high-water scan */
    uint32_t stk[8];
    memset(stk, 0, sizeof(stk));
    rtk_stack_watermark_fill(stk, sizeof(stk));
    CHECK(stk[0] == ACIB_STACK_WATERMARK_LO);
    CHECK(stk[1] == ACIB_STACK_WATERMARK_HI);
    CHECK(rtk_stack_watermark_check(stk, sizeof(stk)) == 0u);
    stk[4] = 0xDEADBEEFu;
    CHECK(rtk_stack_watermark_check(stk, sizeof(stk)) == 16u);
}

/* --- 2. Exception Frame Extraction, LR Unwind & Panic Helpers --- */
static void test_exception_and_unwind(void)
{
    acib_host_reset();

    /* Stack canary failure panics with "stk_fail" */
    EXPECT_PANIC(__stack_chk_fail());
    CHECK(g_host_panic.fmt != NULL && strcmp(g_host_panic.fmt, "stk_fail") == 0);
    CHECK(g_host_panic.halt_reason == 4u);

    /* Encoded panic code+line helper (sub_725FC) */
    EXPECT_PANIC(acib_panic_code_line((486u << 12) | 0x2Au, 0x99u));
    CHECK(g_host_panic.arg0 == 0x99u);
    CHECK(g_host_panic.arg1 == 0x2Au);
    CHECK(g_host_panic.arg2 == 486u);

    /* Fault frame extractor: standard (EXC_RETURN bit4=1) vs FPU-extended (bit4=0) */
    uint8_t raw_ctx[0xB0];
    struct acib_fault_summary sum;
    memset(raw_ctx, 0, sizeof(raw_ctx));
    acib_write_unaligned_le32(raw_ctx + 0x10u, 0x11112222u);
    acib_write_unaligned_le32(raw_ctx + 0x14u, 0x33334444u);
    acib_write_unaligned_le32(raw_ctx + 0x18u, 0xFFFFFFFDu); /* bit4 = 1 */
    acib_write_unaligned_le32(raw_ctx + 0x1Cu, 0x1001BC00u);
    acib_write_unaligned_le32(raw_ctx + 0x54u, 0x0007C821u); /* LR */
    acib_write_unaligned_le32(raw_ctx + 0x58u, 0x00084EB8u); /* PC */
    acib_write_unaligned_le32(raw_ctx + 0x5Cu, 0x61000000u); /* xPSR */

    rtk_fault_extract_frame(raw_ctx, &sum);
    CHECK(sum.r0_or_code == 0x11112222u);
    CHECK(sum.fault_sub == 0x33334444u);
    CHECK(sum.fault_sub_dup == 0x33334444u);
    CHECK(sum.lr == 0x0007C821u);
    CHECK(sum.pc == 0x00084EB8u);
    CHECK(sum.sp == 0x1001BC00u);
    CHECK(sum.xpsr == 0x61000000u);

    /* Thumb-2 LR unwind: BLX Rm (0x4788 = BLX r1) vs 32-bit BL (-4) */
    acib_write_unaligned_le16(acib_addr_ptr(0x00010002u), 0x4788u);
    CHECK(rtk_unwind_adjust_lr(0x00010005u) == 0x00010002u);
    acib_write_unaligned_le16(acib_addr_ptr(0x00010012u), 0xF7FFu);
    CHECK(rtk_unwind_adjust_lr(0x00010015u) == 0x00010010u);
    /* Out-of-ROM LR returns even_lr - 3 */
    CHECK(rtk_unwind_adjust_lr(0x20000011u) == 0x2000000Du);
}

static uint32_t s_test_irq_hits;
static uint32_t test_irq_cb(uintptr_t ctx)
{
    s_test_irq_hits += (uint32_t)ctx;
    return 0u;
}

/* --- 3. IRQ Masking, NVIC Controller & Priority-Bitmap Scheduler --- */
static void test_irq_nvic_and_scheduler(void)
{
    acib_host_reset();

    /* Priority group configuration: prio_bits < 4 rejected; prio_bits = 12 -> mask = 0x40 */
    CHECK(rtk_irq_set_prio_group(3u) == 0x80000001u);
    CHECK(rtk_irq_set_prio_group(12u) == 0u);
    CHECK(g_rtk_irq_prio_group == 12u);
    CHECK(g_rtk_irq_mask_basepri == 0x40u);

    /* BASEPRI mask/restore & enabled check */
    g_acib_host_cpu.primask = 0u;
    g_acib_host_cpu.basepri = 0u;
    CHECK(rtk_irq_are_enabled() == 1u);
    uint32_t saved_bp = rtk_irq_mask_save();
    CHECK(saved_bp == 0u);
    CHECK(g_acib_host_cpu.basepri == 0x40u);
    CHECK(rtk_irq_are_enabled() == 0u);
    /* BASEPRI_MAX semantics: higher-priority mask (0x20 < 0x40) is not lowered */
    g_acib_host_cpu.basepri = 0x20u;
    CHECK(rtk_irq_mask_save() == 0x20u);
    CHECK(g_acib_host_cpu.basepri == 0x20u);
    rtk_irq_mask_restore(saved_bp);
    CHECK(rtk_irq_are_enabled() == 1u);

    /* NVIC IRQ registration, enable/disable, pending check & dispatch (vec 37 = ext IRQ 21) */
    *(uint32_t *)acib_addr_ptr(0x100002ECu) = 128u;
    s_test_irq_hits = 0u;
    CHECK(rtk_nvic_method_7(NULL, 37u, test_irq_cb, 7u) == 0u);
    CHECK(rtk_nvic_method_5(NULL, 37u, 1) == 0u); /* returns previous enabled state (0) */
    CHECK(rtk_nvic_method_6(NULL, 37u) == 0u);    /* set pending in ISPR0 */
    CHECK(IRQ16_Handler(NULL, 37u) == true);      /* ISPR0 bit 21 pending */
    CHECK(rtk_nvic_method_8(NULL, 37u, 8u) == 0u);
    CHECK(acib_host_mmio_read8(ACIB_NVIC_IPR0_ADDR + 21u) == 0x40u);
    g_acib_host_cpu.ipsr = 37u;
    rtk_nvic_dispatch_irq();
    CHECK(s_test_irq_hits == 7u);
    CHECK(rtk_nvic_method_5(NULL, 37u, 0) == 1u); /* returns previous enabled state (1) */
    acib_host_set_mmio32(ACIB_NVIC_ISPR0_ADDR, 0u);
    CHECK(IRQ16_Handler(NULL, 37u) == false);

    /* Priority-bitmap scheduler context switch (rtk_sched_switch @ 0x0e5908) */
    uint8_t thread_low[0x40];
    uint8_t thread_high[0x40];
    memset(thread_low, 0, sizeof(thread_low));
    memset(thread_high, 0, sizeof(thread_high));

    void *old_sp_slot = NULL;
    uint32_t new_sp_dummy = 0x2222u;
    void *new_sp_slot = &new_sp_dummy;
    uint32_t saved_sp_dummy = 0x1111u;

    thread_low[0x16u]  = 1u; /* preemptible */
    thread_high[0x16u] = 1u;
    *(void ***)(thread_low + 0x28u)  = &old_sp_slot;
    *(void ***)(thread_high + 0x28u) = &new_sp_slot;

    *(void **)acib_addr_ptr(0x10002568u) = thread_low;
    g_rtk_sched_need_resched = 1u;
    /* Priority 29 -> bit (1 << 29), clz(0x20000000) == 2 -> top_prio = 29 */
    g_rtk_ready_mask = (1u << 29);
    g_rtk_ready_queues[2u * 29u] = (uintptr_t)thread_high;

    void *next_sp = rtk_sched_switch(&saved_sp_dummy);
    CHECK(next_sp == &new_sp_dummy);
    CHECK(old_sp_slot == &saved_sp_dummy);
    CHECK(*(void **)acib_addr_ptr(0x10002568u) == thread_high);
    CHECK(g_rtk_sched_need_resched == 0u);
}

/* --- 4. Libc String/Memory & 64-bit AEABI Shift Helpers --- */
static void test_libc_and_aeabi(void)
{
    uint8_t buf[32];
    memset(buf, 0xAA, sizeof(buf));
    for (uint32_t i = 0; i < 16u; i++) {
        buf[i] = (uint8_t)('A' + i);
    }

    CHECK(acib_memchr(buf, 'F', 16u) == &buf[5]);
    CHECK(acib_memchr(buf, 'Z', 16u) == NULL);

    /* Overlapping forward & backward memmove */
    memmove(buf + 2, buf, 12u);
    CHECK(acib_memcmp(buf + 2, "ABCDEFGHIJKL", 12u) == 0);
    memmove(buf, buf + 2, 12u);
    CHECK(acib_memcmp(buf, "ABCDEFGHIJKL", 12u) == 0);

    CHECK(acib_strlen("AirPods3,6") == 10u);
    CHECK(acib_strncmp("RTKit-3514", "RTKit-3599", 6u) == 0);
    CHECK(acib_strncmp("RTKit-3514", "RTKit-3599", 9u) < 0);

    CHECK(acib_aeabi_llsl(0x0123456789ABCDEFULL, 0u)  == 0x0123456789ABCDEFULL);
    CHECK(acib_aeabi_llsl(0x0123456789ABCDEFULL, 12u) == (0x0123456789ABCDEFULL << 12));
    CHECK(acib_aeabi_llsl(0x0123456789ABCDEFULL, 40u) == (0x0123456789ABCDEFULL << 40));
    CHECK(acib_aeabi_llsl(0x0123456789ABCDEFULL, 64u) == 0ULL);
    CHECK(acib_aeabi_llsl(0x0123456789ABCDEFULL, 95u) == 0ULL);
    CHECK(acib_aeabi_llsr(0xFEDCBA9876543210ULL, 16u) == (0xFEDCBA9876543210ULL >> 16));
    CHECK(acib_aeabi_llsr(0xFEDCBA9876543210ULL, 44u) == (0xFEDCBA9876543210ULL >> 44));
    CHECK(acib_aeabi_llsr(0xFEDCBA9876543210ULL, 64u) == 0ULL);
    CHECK(acib_aeabi_llsr(0xFEDCBA9876543210ULL, 95u) == 0ULL);
}

/* --- 5. MAC (acib) <-> W3 PHY (phyb) Shared Mailbox, Dekker Lock, FRS & Crash --- */
static void test_mac_phy_ipc_contract(void)
{
    acib_host_reset();

    /* 5a. B0 PHY init handshake at 0x4D280000 (req) / 0x4D280400 (resp) */
    g_host_spy.auto_ack_sync_cmd = true;
    struct acib_phy_mbox_resp *resp = (struct acib_phy_mbox_resp *)acib_host_mbox_ptr(0x400u);
    resp->fw_version[0] = 0x0021u;
    resp->fw_version[1] = 0x005Bu;
    resp->fw_version[2] = 0x0003u;
    resp->fw_version[3] = 0x0001u;

    for (uint32_t i = 0; i < sizeof(g_phy_table_shadow); i++) {
        g_phy_table_shadow[i] = (uint8_t)(i ^ 0x5Au);
    }

    phy_ipc_init();

    CHECK(g_phy_ipc_state.req_addr == ACIB_PHY_MBOX_BASE);
    CHECK(g_phy_ipc_state.resp_addr == ACIB_PHY_MBOX_RESP_BASE);
    CHECK(g_host_spy.phy_ver_calls == 1u);
    CHECK(g_host_spy.phy_ver[0] == 0x0021u && g_host_spy.phy_ver[1] == 0x005Bu);

    /* Verify handshake payload {3, 0x5B, 1, 0} & Dekker-synced 288B table @ +0x0C4 */
    struct acib_phy_mbox_req *req = (struct acib_phy_mbox_req *)acib_host_mbox_ptr(0x000u);
    CHECK(req->args_u16[0] == 3u);
    CHECK(req->args_u16[1] == 0x5Bu);
    CHECK(req->args_u16[2] == 1u);
    CHECK(req->args_u16[3] == 0u);
    CHECK(req->mac_want_lock == 0u);
    CHECK(acib_memcmp(req->shared_link_table, g_phy_table_shadow, 0x120u) == 0);

    /* 5b. Legacy PHY response window fallback (0x4D2801E6) if +0x1EE mutated */
    acib_host_reset();
    g_host_spy.auto_ack_sync_cmd = true;
    g_host_spy.mutate_legacy_probe_on_cmd = true;
    phy_ipc_init();
    CHECK(g_phy_ipc_state.resp_addr == ACIB_PHY_MBOX_LEGACY_RESP);
    CHECK(g_host_spy.phy_ver[0] == 0x0102u && g_host_spy.phy_ver[3] == 0x0708u);

    /* 5c. Switch back to B0 layout & test Fast RSSI Scan (FRS) + 24-bit LE pair packing */
    acib_host_reset();
    g_host_spy.auto_ack_sync_cmd = true;
    phy_ipc_init();
    resp = (struct acib_phy_mbox_resp *)acib_host_mbox_ptr(0x400u);
    req  = (struct acib_phy_mbox_req *)acib_host_mbox_ptr(0x000u);

    for (uint32_t i = 0; i < 126u; i++) {
        resp->frs_rssi_q[i]      = (int16_t)(-80 + (int)(i & 0x3Fu));
        resp->frs_aux_metrics[i] = (uint8_t)(i + 1u);
    }
    g_host_spy.frs_auto_complete_status = 0x00u;
    g_host_spy.timer_step_per_read      = 250u;

    uint8_t  packed_rssi[189];
    uint16_t elapsed_us = 0u;
    memset(packed_rssi, 0, sizeof(packed_rssi));

    int frs_rc = phy_ipc_frs_run_scan(2u, 4u, 8u, 5u, 0u, NULL, packed_rssi, &elapsed_us);
    CHECK(frs_rc == 0);
    CHECK(req->frs_mode == 2u);
    CHECK(req->frs_param == 4u);
    CHECK(req->frs_dwell_ticks == 8u);
    CHECK(req->frs_band_flags == 5u);
    CHECK(elapsed_us == 250u);

    /* Unpack first 24-bit LE pair (ch0 in bits[11:0], ch1 in bits[23:12]) */
    uint32_t pair24 = (uint32_t)packed_rssi[0] |
                      ((uint32_t)packed_rssi[1] << 8) |
                      ((uint32_t)packed_rssi[2] << 16);
    uint16_t s0_12 = (uint16_t)(pair24 & 0x0FFFu);
    uint16_t s1_12 = (uint16_t)((pair24 >> 12) & 0x0FFFu);
    CHECK(s0_12 == ((uint16_t)resp->frs_rssi_q[0] & 0x0FFFu));
    CHECK(s1_12 == ((uint16_t)resp->frs_rssi_q[1] & 0x0FFFu));

    /* 5d. FRS scan timeout triggers abort doorbell (0x4D154C68 = 2) & returns 0x18 */
    g_host_spy.frs_timeout_mode    = true;
    g_host_spy.timer_step_per_read = 50000u;
    frs_rc = phy_ipc_frs_run_scan(1u, 1u, 2u, 1u, 0u, NULL, packed_rssi, &elapsed_us);
    CHECK(frs_rc == 0x18);
    CHECK(acib_host_get_mmio32(ACIB_PHY_IPC_FROM_MAC_DB_ADDR) == ACIB_PHY_DB_FRS_ABORT);

    /* 5e. Coex grant doorbell (bit 2 -> 0x4D154C68 = 4, unaligned u32 @ +0x1F9) */
    uint32_t coex_word = 0xA55A1234u;
    phy_ipc_coex_ring_doorbell(&coex_word);
    CHECK(acib_read_unaligned_le32(&req->coex_grant_word) == 0xA55A1234u);
    CHECK(acib_host_get_mmio32(ACIB_PHY_IPC_FROM_MAC_DB_ADDR) == ACIB_PHY_DB_COEX_UPDATE);

    /* 5f. PHY->MAC IRQ handler (sub_7CD5C): bit0 PHY crash dump @ 0x4D2805C1..0x4D2805D4 */
    acib_write_unaligned_le32((uint8_t *)resp + 0x1C1u, 0xDEAD0001u);
    acib_write_unaligned_le32((uint8_t *)resp + 0x1C5u, 0xDEAD0002u);
    acib_write_unaligned_le32((uint8_t *)resp + 0x1C9u, 0xDEAD0003u);
    acib_write_unaligned_le32((uint8_t *)resp + 0x1CDu, 0xDEAD0004u);
    acib_write_unaligned_le32((uint8_t *)resp + 0x1D1u, 0xDEAD0005u);
    acib_host_set_mmio32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR, ACIB_PHY_STATUS_CRASH);

    EXPECT_PANIC(phy_ipc_irq_handler());
    /* W1C cleared status bit 0, unpacked 5 crash words, set panic reason 0x5D & halted */
    CHECK(acib_host_get_mmio32(ACIB_PHY_IPC_TO_MAC_STATUS_ADDR) == 0u);
    CHECK(g_host_spy.crash_dump_calls == 1u);
    CHECK(g_host_spy.crash_dump_words[0] == 0xDEAD0001u);
    CHECK(g_host_spy.crash_dump_words[4] == 0xDEAD0005u);
    CHECK(g_acib_panic_reason == 0x5Du);

    /* 5g. SEQ_RF_CTRL (0x4D154038) shared register read & bit29 mask (sub_97920) */
    acib_host_set_mmio32(ACIB_PHY_SEQ_RF_CTRL_ADDR, 0x20000155u);
    CHECK(phy_seq_rf_ctrl_apply_tx_mode(1u, 7u) == 0);
    CHECK(g_host_spy.last_rf_reg_id == 0x0Au);
    CHECK(g_host_spy.last_rf_reg_val == 0x00000155u);
}

/* --- 6. ACI Trace Manager, OSA Mailbox Relay & BT Controller Entry Points --- */
static void test_aci_trace_osa_and_bt_ctrl(void)
{
    acib_host_reset();

    /* Trace Manager Buffer Util + overflow panic */
    CHECK(tm_buffer_util_cb_init(0u) == &g_tm_default_cb);
    uint8_t storage[64];
    struct acib_tm_buf_cb cb;
    tm_buffer_util_bind(&cb, storage, sizeof(storage));
    CHECK(tm_buffer_util_has_storage(&cb) == 1u);
    CHECK(tm_buffer_util_get_free(&cb) == 64u);
    tm_buffer_util_decrement_remaining(&cb, 20u);
    CHECK(tm_buffer_util_get_used(&cb) == 20u);
    CHECK(tm_buffer_util_get_free(&cb) == 44u);
    CHECK(tm_buffer_util_write_ptr(&cb) == storage + 20u);
    EXPECT_PANIC(tm_buffer_util_decrement_remaining(&cb, 45u));

    /* Trace packet timestamped event encoder (sub_E4828, kind=5 -> 0xBA hdr4) */
    uint8_t pkt[16];
    uint8_t ev_desc[10] = { 5u, 0u, 0u, 0u, 0x78u, 0x56u, 0x34u, 0x12u, 0x17u, 0x00u };
    CHECK(tm_pkt_write_ts_event(pkt, ev_desc) == 8u);
    CHECK(pkt[0] == 0xBAu && pkt[7] == 0x20u);
    CHECK(acib_read_unaligned_le32(pkt + 1u) == 0x12345678u);

    /* OSA mailbox relay lookup against real ROM_RODATA table @ 0xF5434 */
    uint8_t expected_dest = *(const uint8_t *)acib_addr_ptr(0x000F5434u + 5u);
    CHECK(osa_mbx_get_relay_dest(0x5123u) == expected_dest);
    EXPECT_PANIC(osa_mbx_get_relay_dest(0xB000u));

    /* HCI Test Mode opcode lookup against real ROM_RODATA table @ 0xF1D98 */
    uint32_t op0 = hci_test_mode_get_opcode_from_type(0u);
    uint32_t expected_op0 = acib_read_unaligned_le32(acib_addr_ptr(0x000F1D98u));
    CHECK(op0 == expected_op0);
    EXPECT_PANIC(hci_test_mode_get_opcode_from_type(5u));

    /* BD_ADDR 6-byte pair swap (sub_AEF9E) */
    const uint8_t bd_in[6] = { 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u };
    uint8_t bd_out[6] = { 0 };
    bt_bdaddr_swap(bd_in, bd_out);
    CHECK(bd_out[0] == 0x22u && bd_out[1] == 0x11u && bd_out[2] == 0x44u &&
          bd_out[3] == 0x33u && bd_out[4] == 0x66u && bd_out[5] == 0x55u);

    /* BT timestamp diagnostic ring & BT controller thread init */
    uint32_t ts_ring_buf_addr = *(uint32_t *)acib_addr_ptr(0x10001648u);
    *(uint32_t *)acib_addr_ptr(0x1000164Cu) = 0u;
    g_host_spy.timer_step_per_read = 77u;
    bt_ts_ring_record(0x2AAu);
    uint32_t ring_w0 = *(uint32_t *)acib_addr_ptr(ts_ring_buf_addr);
    CHECK((ring_w0 >> 22) == 0x2AAu);

    bt_system_thread_init();
    CHECK(g_host_spy.spawned_threads == 2u);
    CHECK(g_bt_sys_threads_state[3] == 0x24u);
    CHECK(g_rtk_thread_ptr_table[1] != NULL);
    CHECK(g_rtk_thread_ptr_table[3] != NULL);
    EXPECT_PANIC(bt_system_thread_init());
    CHECK(g_host_panic.fmt != NULL &&
          strcmp(g_host_panic.fmt, "tcm_debug_information: init_state must be incremented consecutively!") == 0);
}

int main(void)
{
    test_boot_mpu_and_clocks();
    test_exception_and_unwind();
    test_irq_nvic_and_scheduler();
    test_libc_and_aeabi();
    test_mac_phy_ipc_contract();
    test_aci_trace_osa_and_bt_ctrl();

    printf("acib_tests: %u/%u checks passed\n",
           s_checks_run - s_checks_failed, s_checks_run);
    return (s_checks_failed == 0u) ? 0 : 1;
}
