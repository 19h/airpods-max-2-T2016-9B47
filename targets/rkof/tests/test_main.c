#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host_compat.h"

extern int rkof_layout_checks_compiled(void);

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        g_tests_run++;                                                         \
        if (!(cond)) {                                                         \
            g_tests_failed++;                                                  \
            fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, (msg));  \
        }                                                                      \
    } while (0)

static uint8_t *load_raw_file(const char *path, size_t *len_out)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long sz = ftell(fp);
    if (sz <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }
    if (fread(buf, 1u, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    *len_out = (size_t)sz;
    return buf;
}

static void test_boot_and_remap(const uint8_t *raw, size_t raw_len)
{
    rkof_host_bus_reset(raw, raw_len);
    MMIO32_WRITE(RKOF_REMAP_CTRL_ADDR, RKOF_REMAP_CTRL_ENABLE_BIT | 0x5u);
    MMIO32_WRITE(RKOF_REMAP_STATUS_ADDR, 0u);

    rtkit_entry();

    const uint32_t *rmd1 = RKOF_PTR(const uint32_t *, RKOF_RODATA_TLV_RMD1_ADDR);
    const uint32_t *rmc1 = RKOF_PTR(const uint32_t *, RKOF_RODATA_TLV_RMC1_ADDR);
    const uint32_t *rmfi = RKOF_PTR(const uint32_t *, RKOF_RODATA_TLV_RMFI_ADDR);
    const uint32_t *rtsi = RKOF_PTR(const uint32_t *, RKOF_RODATA_TLV_RTSI_ADDR);

    CHECK(MMIO32_READ(RKOF_REMAP_DATA_RMD1_ADDR) == rmd1[2], "RMD1 programmed from TLV");
    CHECK(MMIO32_READ(RKOF_REMAP_CODE_RMC1_ADDR) == rmc1[2], "RMC1 programmed from TLV");
    CHECK(MMIO32_READ(RKOF_REMAP_RMFI_ADDR) == rmfi[2], "RMFI programmed from TLV");
    CHECK(MMIO32_READ(RKOF_SCRATCH_RTSI_ADDR) == rtsi[2], "RTSI programmed to 0x8000F0");
    CHECK((MMIO32_READ(RKOF_REMAP_CTRL_ADDR) & RKOF_REMAP_CTRL_ENABLE_BIT) != 0u, "Remap re-enabled");

    /* Already-remapped fast path should skip register writes */
    MMIO32_WRITE(RKOF_REMAP_STATUS_ADDR, RKOF_REMAP_STATUS_DONE_BIT);
    MMIO32_WRITE(RKOF_REMAP_RMFI_ADDR, 0xDEADBEEFu);
    rtkit_entry();
    CHECK(MMIO32_READ(RKOF_REMAP_RMFI_ADDR) == 0xDEADBEEFu, "Remap status bit skips reprogramming");

    /* Stack canary init & stack poison fill */
    *RKOF_PTR(uint32_t *, RKOF_DATA_STACK_CHK_GUARD_ADDR) = 0u;
    __stack_chk_init();
    CHECK(*RKOF_PTR(uint32_t *, RKOF_DATA_STACK_CHK_GUARD_ADDR) == 0x00000AFFu, "STKG canary 0xAFF");

    uint32_t stack_words[6] = { 0 };
    sub_175D4(stack_words, 20u);
    CHECK(stack_words[0] == 0x534B5452u && stack_words[1] == 0x4B434154u, "RTKS/TACK poison pair 0");
    CHECK(stack_words[2] == 0x534B5452u && stack_words[3] == 0x4B434154u, "RTKS/TACK poison pair 1");
    CHECK(stack_words[4] == 0u, "Trailing <8 bytes untouched");
}

static void test_libc_and_aeabi(const uint8_t *raw, size_t raw_len)
{
    rkof_host_bus_reset(raw, raw_len);

    const char *sample = "AirPods3,6-rkof";
    CHECK(sub_CE20(sample) == 15u, "sub_CE20 strlen");
    CHECK(sub_CD48(sample, '-') == sample + 10, "sub_CD48 strchr hit");
    CHECK(sub_CD48(sample, '\0') == sample + 15, "sub_CD48 NUL hit");
    CHECK(sub_CD48(sample, 'Z') == NULL, "sub_CD48 miss");
    CHECK(sub_CA68(sample, '3', 15u) == sample + 7, "sub_CA68 memchr");
    CHECK(rkof_strcmp("phyb", "phyb") == 0, "strcmp equal");
    CHECK(rkof_strcmp("phyb", "phya") == 1, "strcmp diff");
    CHECK(rkof_strncmp("up_extra", "up_other", 3u) == 0, "strncmp prefix");

    /* Aligned 4+ byte memcmp returns +/-1 whereas short memcmp returns byte diff */
    _Alignas(4) uint8_t buf_l[8] = { 1, 2, 3, 10, 5, 6, 7, 8 };
    _Alignas(4) uint8_t buf_r[8] = { 1, 2, 3, 20, 5, 6, 7, 8 };
    CHECK(rkof_memcmp(buf_l, buf_r, 8u) == -1, "memcmp aligned word returns -1");
    CHECK(rkof_memcmp(buf_r, buf_l, 8u) == 1, "memcmp aligned word returns +1");
    CHECK(rkof_memcmp(buf_l + 2, buf_r + 2, 2u) == -10, "memcmp <4 returns exact byte diff");

    uint8_t fill_buf[19];
    rkof_memset(fill_buf, 0xA5, sizeof(fill_buf));
    CHECK(rtkit_mem_is_filled(fill_buf + 1, 0xA5u, 17u) == 1u, "rtkit_mem_is_filled true");
    fill_buf[12] = 0xA4u;
    CHECK(rtkit_mem_is_filled(fill_buf + 1, 0xA5u, 17u) == 0u, "rtkit_mem_is_filled false");

    /* Overlap-safe memcpy / memcpy_thunk / j_memcpy (backward & forward overlap) */
    uint8_t ov_bwd[17] = "0123456789abcdef";
    CHECK(rkof_memcpy(ov_bwd + 3, ov_bwd, 12u) == ov_bwd + 3, "rkof_memcpy backward ret");
    CHECK(memcmp(ov_bwd, "0120123456789abf", 16u) == 0, "rkof_memcpy backward overlap");
    uint8_t ov_fwd[17] = "0123456789abcdef";
    CHECK(memcpy_thunk(ov_fwd, ov_fwd + 3, 12u) == ov_fwd, "memcpy_thunk forward ret");
    CHECK(memcmp(ov_fwd, "3456789abcdecdef", 16u) == 0, "memcpy_thunk forward overlap");
    uint8_t ov_same[9] = "rkofphyb";
    CHECK(j_memcpy(ov_same, ov_same, 8u) == ov_same && memcmp(ov_same, "rkofphyb", 8u) == 0, "j_memcpy dst==src");

    char fmt_out[32];
    int n = rkof_snprintf(fmt_out, sizeof(fmt_out), "tag=0x%x", 0x4E56u);
    CHECK(n == 10 && strcmp(fmt_out, "tag=0x4e56") == 0, "rkof_snprintf formatting");

    /* AEABI 64-bit shifts (including shift = 0, 1, 31, 32, 33, 63, 64, 95, 256), divmod, and IEEE-754 */
    const uint64_t sh_in = 0x123456789ABCDEF1ULL;
    CHECK(sub_D23C(sh_in, 0u) == sh_in, "sub_D23C lsl 0");
    CHECK(sub_D23C(sh_in, 1u) == (sh_in << 1u), "sub_D23C lsl 1");
    CHECK(sub_D23C(sh_in, 8u) == 0x3456789ABCDEF100ULL, "sub_D23C lsl 8");
    CHECK(sub_D23C(sh_in, 31u) == (sh_in << 31u), "sub_D23C lsl 31");
    CHECK(sub_D23C(sh_in, 32u) == 0x9ABCDEF100000000ULL, "sub_D23C lsl 32");
    CHECK(sub_D23C(sh_in, 33u) == (0x9ABCDEF100000000ULL << 1u), "sub_D23C lsl 33");
    CHECK(sub_D23C(0x0000000089ABCDEFULL, 36u) == 0x9ABCDEF000000000ULL, "sub_D23C lsl 36");
    CHECK(sub_D23C(sh_in, 63u) == 0x8000000000000000ULL, "sub_D23C lsl 63");
    CHECK(sub_D23C(sh_in, 64u) == 0ULL, "sub_D23C lsl 64 -> 0");
    CHECK(sub_D23C(sh_in, 95u) == 0ULL, "sub_D23C lsl 95 -> 0");
    CHECK(sub_D23C(sh_in, 256u) == sh_in, "sub_D23C lsl 256 -> Rm[7:0]==0");

    CHECK(sub_D9F8(sh_in, 0u) == sh_in, "sub_D9F8 lsr 0");
    CHECK(sub_D9F8(sh_in, 1u) == (sh_in >> 1u), "sub_D9F8 lsr 1");
    CHECK(sub_D9F8(0x123456789ABCDEF0ULL, 12u) == 0x000123456789ABCDULL, "sub_D9F8 lsr 12");
    CHECK(sub_D9F8(sh_in, 31u) == (sh_in >> 31u), "sub_D9F8 lsr 31");
    CHECK(sub_D9F8(sh_in, 32u) == 0x0000000012345678ULL, "sub_D9F8 lsr 32");
    CHECK(sub_D9F8(sh_in, 33u) == 0x00000000091A2B3CULL, "sub_D9F8 lsr 33");
    CHECK(sub_D9F8(0x123456789ABCDEF0ULL, 40u) == 0x0000000000123456ULL, "sub_D9F8 lsr 40");
    CHECK(sub_D9F8(0x823456789ABCDEF1ULL, 63u) == 1ULL, "sub_D9F8 lsr 63");
    CHECK(sub_D9F8(sh_in, 64u) == 0ULL, "sub_D9F8 lsr 64 -> 0");
    CHECK(sub_D9F8(sh_in, 95u) == 0ULL, "sub_D9F8 lsr 95 -> 0");
    CHECK(sub_D9F8(sh_in, 256u) == sh_in, "sub_D9F8 lsr 256 -> Rm[7:0]==0");

    uint64_t rem = 0u;
    uint64_t quot = sub_DD76(1000000000007ULL, 1000003ULL, &rem);
    CHECK(quot == (1000000000007ULL / 1000003ULL), "sub_DD76 quotient");
    CHECK(rem == (1000000000007ULL % 1000003ULL), "sub_DD76 remainder");
    CHECK(sub_D5D0(-987654321098LL, 12345LL) == (-987654321098LL / 12345LL), "sub_D5D0 sdiv64");

    CHECK(fabs(sub_D610(-3.5f) - (-3.5)) < 1e-12, "sub_D610 f2d normal");
    CHECK(sub_D680(-42999.875) == -42999, "sub_D680 d2iz");
    CHECK(sub_D6C2(-1048576.0f) == -1048576LL, "sub_D6C2 f2lz");
    CHECK(sub_D704(3000000000.0) == 3000000000u, "sub_D704 d2uiz");
    CHECK(sub_D736(16777216.0f) == 16777216ULL, "sub_D736 f2ulz");
    CHECK(sub_D7BC(-12345678LL) == (float)(-12345678LL), "sub_D7BC l2f");
}

static void test_irq_systick_and_exception(const uint8_t *raw, size_t raw_len)
{
    rkof_host_bus_reset(raw, raw_len);
    *RKOF_PTR(uint32_t *, STATE_IRQ_PRIO_BASE) = 0x80u;
    *RKOF_PTR(uint32_t *, RKOF_DATA_MAX_EXT_IRQS_ADDR) = 64u;

    g_rkof_host_cpu.basepri = 0u;
    g_rkof_host_cpu.primask = 0u;
    CHECK(sub_16F44() == 1u, "IRQ initially enabled");
    uint32_t prev = rtkit_enter_critical();
    CHECK(prev == 0u && g_rkof_host_cpu.basepri == 0x80u, "rtkit_enter_critical masks basepri");
    CHECK(sub_16F44() == 0u, "IRQ masked under critical section");
    sub_16F20(prev);
    CHECK(sub_16F44() == 1u, "IRQ restored");

    uint32_t spin = 0u;
    CHECK(sub_175F0(&spin) == 0u && spin == 1u, "spinlock acquire");
    CHECK(sub_175F0(&spin) == 0x20002u, "spinlock contention returns 0x20002");
    sub_1760C(&spin);
    CHECK(spin == 0u && g_rkof_host_cpu.basepri == 0u, "spinlock release");

    CHECK(sub_170C8(NULL, 2u, 0x1111u, 0x2222u) == 0u, "NMI slot 0 register");
    CHECK(sub_170C8(NULL, 16u, 0x3333u, 0x4444u) == 0u, "EXT0 slot 2 register");
    CHECK(sub_170C8(NULL, 10u, 0x5555u, 0x6666u) == 0x80000001u, "Invalid exc_num rejected");

    CHECK(sub_1710C(NULL, 2u, 4u) == 0x8000000Bu, "NMI prio returns 0x8000000B");
    CHECK(sub_1710C(NULL, 15u, 8u) == 0u, "SysTick prio set");
    CHECK((MMIO32_READ(SCB_SHPR3_ADDR) >> 24u) == 0x40u, "SysTick SHPR3 byte == 0x40");

    /* SysTick conversions & programming */
    struct rkof_systick_dev st;
    memset(&st, 0, sizeof(st));
    st.freq_hz = 24000000u;
    CHECK(sub_18A59C(&st, 0u, 100ULL) == 2400ULL, "100us @ 24MHz == 2400 ticks");
    CHECK(sub_18A568(&st, 0u, 2400ULL) == 100ULL, "2400 ticks @ 24MHz == 100us");
    CHECK(sub_18A4D0(&st, 0u, 5000ULL) == 0u, "oneshot period configure");
    CHECK(MMIO32_READ(SYST_RVR_ADDR) == 4999u, "SYST_RVR == period - 1");
    MMIO32_WRITE(SYST_CVR_ADDR, 1234u);
    uint64_t rem_ticks = 0u;
    CHECK(sub_18A510(&st, &rem_ticks) == 0u && rem_ticks == 1234ULL, "sub_18A510 remaining");

    /* Crash magic & panic trap */
    sub_17BD4(0x42504859u);
    const char *magic_str = RKOF_PTR(const char *, STATE_CRASH_MAGIC_BASE + 0x44u);
    CHECK(strcmp(magic_str, "BPHY") == 0, "sub_17BD4 stores big-endian FourCC BPHY");

    g_rkof_panic_trap.armed = 1;
    if (setjmp(g_rkof_panic_trap.env) == 0) {
        sub_FDE74();
    }
    g_rkof_panic_trap.armed = 0;
    CHECK(g_rkof_panic_trap.triggered == 1, "sub_FDE74 assertion trapped");
    CHECK(strstr(g_rkof_panic_trap.last_msg, "modem_inits.c:157") != NULL, "Assertion message has file:line");
}

static void test_coproc_and_phy(const uint8_t *raw, size_t raw_len)
{
    rkof_host_bus_reset(raw, raw_len);

    /* sub_17F5C coprocessor lookup including uxth quirk on 0x00010002 */
    CHECK(sub_17F5C(2u) == RKOF_PTR(void *, RKOF_DATA_COPROC_MASK02_ADDR), "mask 2");
    CHECK(sub_17F5C(0x00010002u) == RKOF_PTR(void *, RKOF_DATA_COPROC_MASK02_ADDR), "uxth mask 0x10002");
    CHECK(sub_17F5C(0x00010004u) == NULL, "32-bit mask 0x10004 returns NULL");
    CHECK(sub_17F5C(0x20u) == RKOF_PTR(void *, STATE_COPROC_DYN_BASE), "mask 0x20");

    /* sub_15B78 & sub_15BAC carveout grant setup */
    struct rkof_carveout_info *cv = RKOF_PTR(struct rkof_carveout_info *, STATE_CARVEOUT_BASE);
    cv->region_a_base = 0x4D280000u;
    cv->region_a_size = 0x14000u;
    cv->shared_vma = 0x4D280000u;
    cv->shared_meta = 0x1234u;
    uintptr_t fake_mgr_buf[36] = { 0 };
    *RKOF_PTR(uintptr_t *, RKOF_DATA_BTP_MGR_PTR_ADDR + 0x10u) = (uintptr_t)fake_mgr_buf;

    CHECK(sub_15B78() == 0, "sub_15B78 ok");
    const struct rkof_coproc_grant *grant =
        RKOF_PTR(const struct rkof_coproc_grant *, RKOF_DATA_COPROC_GRANT_A_ADDR);
    CHECK(grant->grant_vma == 0x4D280000u && grant->grant_meta == 0x1234u, "coproc grant populated");
    CHECK(sub_15BAC() == 0, "sub_15BAC ok");

    /* Coproc refcount acquire/release */
    CHECK(sub_15C1C() == 0u && sub_15C1C() == 0u, "refcount 0->1->2");
    CHECK(sub_15C1C() == 0x8000000Bu, "refcount >2 rejected");
    CHECK(sub_15C4C() == 0u && sub_15C4C() == 0u, "refcount 2->1->0");
    CHECK(sub_15C4C() == 0x8000000Bu, "refcount underflow rejected");

    /* Modem reset, pairs, coalesced RMW triples, & warm-restore table at 0x4D291000 */
    sub_FDD90(1);
    CHECK((MMIO32_READ(RKOF_W3_MODEM_RESET_ADDR) & 0x50u) == 0x50u, "modem reset bits 4,6 set");
    CHECK((MMIO32_READ(RKOF_W3_PHY_TOP_CTRL_ADDR) & 0x90020u) == 0x90020u, "PHY top reset bits set");
    sub_FDD90(0);
    CHECK((MMIO32_READ(RKOF_W3_MODEM_RESET_ADDR) & 0x70u) == 0u, "modem reset bits cleared");

    const uint32_t pairs[4] = {
        0x4D154100u, 0x11223344u,
        0x4D140020u, 0x55667788u, /* Outside 0x4D154000..0x4D155000: not recorded in warm_regs */
    };
    sub_FDDDC(pairs, sizeof(pairs));

    const uint32_t triples[6] = {
        0x4D154200u, 0x000000FFu, 0x000000AAu,
        0x4D154200u, 0x0000FF00u, 0x0000BB00u, /* Coalesced with previous RMW! */
    };
    sub_FDDF8(triples, sizeof(triples));

    const struct phy_status_region *smem =
        RKOF_PTR(const struct phy_status_region *, RKOF_PHY_STATUS_SRAM_BASE);
    CHECK(smem->slpr_reg_num == 2u, "warm_regs recorded 1 in-range pair + 1 coalesced triple");
    CHECK(smem->warm_regs[0].reg_addr == 0x4D154100u && smem->warm_regs[0].reg_val == 0x11223344u, "warm_regs[0]");
    CHECK(smem->warm_regs[1].reg_addr == 0x4D154200u && smem->warm_regs[1].reg_val == 0x0000BBAAu, "warm_regs[1] coalesced");

    /* PHY trace coredump (sub_FDC38) & crash error capture (sub_FDCB4) */
    uint8_t *trace_sram = RKOF_PTR(uint8_t *, RKOF_PHY_TRACE_RING_BASE);
    for (uint32_t idx = 0u; idx < 32u; idx++) {
        trace_sram[idx] = (uint8_t)(0x40u + idx);
    }
    struct phy_crash_record *crash =
        RKOF_PTR(struct phy_crash_record *, RKOF_PHY_CRASH_RECORD_BASE);
    crash->error_code = 0xE0010042u;
    crash->detail1_ptr = 0x4D2805C1u;
    crash->detail1_len = 12u;
    uint8_t *detail_sram = RKOF_PTR(uint8_t *, 0x4D2805C1u);
    memcpy(detail_sram, "PHYB_PANIC!!", 12u);

    rkof_host_set_mock_pwr_state(0u);
    uint8_t coredump_out[512] = { 0 };
    uint32_t cd_bytes = sub_FDC38(NULL, coredump_out, 16u);
    CHECK(cd_bytes == (16u + 12u + 0x9Cu), "sub_FDC38 total byte count");
    CHECK(*(uint32_t *)(void *)(coredump_out + 16u) == 0u, "sub_FDC38 saved original pwr state");
    CHECK(memcmp(coredump_out + 16u + 0x9Cu, "PHYB_PANIC!!", 12u) == 0, "sub_FDC38 detail payload");
    CHECK(rkof_host_get_mock_pwr_state() == 0u, "sub_FDC38 restored power state");

    uint32_t crash_words[8] = { 0 };
    uint32_t err_bytes = sub_FDCB4(NULL, crash_words, sizeof(crash_words));
    CHECK(err_bytes == 20u, "sub_FDCB4 returns detail_len + 8");
    CHECK(crash_words[0] == 0xE0010042u && crash_words[1] == 12u, "sub_FDCB4 header words");
    CHECK(memcmp(&crash_words[2], "PHYB_PANIC!!", 12u) == 0, "sub_FDCB4 detail copy");

    /* Overflow early-out path marks coredump state +0x5D without restoring power */
    *RKOF_PTR(uint32_t *, RKOF_DATA_COREDUMP_STATE_ADDR + 0x54u) = 1u;
    rkof_host_set_mock_pwr_state(1u);
    CHECK(sub_FDCB4(NULL, crash_words, 10u) == 0u, "sub_FDCB4 overflow returns 0");
    CHECK(*RKOF_PTR(uint8_t *, RKOF_DATA_COREDUMP_STATE_ADDR + 0x5Du) == 1u, "sub_1887C4 overflow flag set");
    CHECK(rkof_host_get_mock_pwr_state() == 2u, "sub_FDCB4 early-out leaves power awake (2)");

    /* WNVV CRC8 & ring helpers (including 0xBC endianness switch) */
    uint8_t ring_backing[8] = { 0xAAu, 0xBCu, 0x12u, 0x34u, 0x00u, 0x77u, 0x34u, 0x12u };
    struct rkof_byte_ring ring = { 0u, 8u, 5u, 0u, ring_backing };
    CHECK(sub_FE59E(&ring) == 0x1234u, "sub_FE59E 0xBC big-endian u16");
    ring.rd = 4u;
    CHECK(sub_FE59E(&ring) == 0x1234u, "sub_FE59E non-0xBC little-endian u16");
    CHECK(sub_FE7CE(&ring) == 6u, "sub_FE7CE ring free space");
    uint8_t crc8 = sub_FE564(0xFFu, (const uint8_t *)"W3PHY", 5u);
    CHECK(crc8 != 0xFFu, "sub_FE564 CRC8 table lookup");

    /* phy_shell_cmd subcommands */
    const char *argv_wake[] = { "phy", "wake", NULL };
    const char *argv_sleep[] = { "phy", "sleep", NULL };
    const char *argv_uart[] = { "phy", "uart", NULL };
    const char *argv_crash[] = { "phy", "crash", "2", NULL };
    CHECK(phy_shell_cmd(NULL, argv_wake) == 0, "phy wake");
    CHECK((MMIO32_READ(RKOF_PMGR_PHY_CLK20_ADDR) & 0x0Fu) == 0x0Fu, "phy wake enabled CLK20");
    CHECK(phy_shell_cmd(NULL, argv_sleep) == 0, "phy sleep");
    CHECK((MMIO32_READ(RKOF_PMGR_PHY_CLK20_ADDR) & 0x0Fu) == 0u, "phy sleep disabled CLK20");
    CHECK(phy_shell_cmd(NULL, argv_uart) == 0, "phy uart");
    CHECK((MMIO32_READ(RKOF_UART_PINMUX_AUX_ADDR + 4u) & 0x20000u) != 0u, "phy uart pinmux");
    CHECK(phy_shell_cmd(NULL, argv_crash) == 0, "phy crash 2");
}

static void test_wnvv_and_nv_synthesis(const uint8_t *raw, size_t raw_len)
{
    rkof_host_bus_reset(raw, raw_len);

    /* Build a synthetic WNVV container in SRAM and parse via sub_FE2B0 */
    uint8_t container[64];
    memset(container, 0, sizeof(container));
    /* Record 0: 1-byte tag 0x12 = 0xAB -> little-endian u16 = 0x12AB */
    container[8] = 0xABu;
    container[9] = 0x12u;
    /* Record 1: short tag 0x0105, len=4 (enc_len=3 -> 0x8000 | (3<<11) | 0x0105 = 0x9905) */
    uint16_t short_hdr = (uint16_t)(0x8000u | (3u << 11u) | 0x0105u);
    memcpy(container + 10, &short_hdr, 2u);
    uint32_t short_val = 0xCAFEBABEu;
    memcpy(container + 12, &short_val, 4u);

    uint32_t sram_off = 0x20050000u;
    memcpy(rkof_host_translate_ptr(sram_off), container, 16u);
    *RKOF_PTR(uintptr_t *, STATE_CORE_PTRS_BASE + 8u) = 0x200CD6E4u;

    sub_FE2B0(NULL, 0u, sram_off, 16u, NULL);

    uint16_t out_len = 0u;
    const uint8_t *p12 = sub_FE504(0x12u, &out_len);
    CHECK(p12 != NULL && out_len == 1u && p12[0] == 0xABu, "WNVV 1-byte tag 0x12");

    const uint8_t *p105 = sub_FE504(0x0105u, &out_len);
    CHECK(p105 != NULL && out_len == 4u && *(const uint32_t *)(const void *)p105 == 0xCAFEBABEu,
          "WNVV short tag 0x0105");
}

int main(int argc, char **argv)
{
    const char *raw_path = (argc > 1) ? argv[1] : "../../../rkof";
    size_t raw_len = 0u;
    uint8_t *raw = load_raw_file(raw_path, &raw_len);
    CHECK(raw != NULL && raw_len == 2159008u, "Loaded raw rkof firmware (2,159,008 bytes)");
    CHECK(rkof_layout_checks_compiled() == 1, "Compile-time _Static_assert layout proof");

    if (raw != NULL) {
        test_boot_and_remap(raw, raw_len);
        test_libc_and_aeabi(raw, raw_len);
        test_irq_systick_and_exception(raw, raw_len);
        test_coproc_and_phy(raw, raw_len);
        test_wnvv_and_nv_synthesis(raw, raw_len);
        free(raw);
    }

    printf("rkof host behavioral suite: %d/%d checks passed\n",
           g_tests_run - g_tests_failed, g_tests_run);
    return (g_tests_failed == 0) ? 0 : 1;
}
