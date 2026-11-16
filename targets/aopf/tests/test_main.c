/*
 * test_main.c — Comprehensive native host behavioral test suite exercising
 * reconstructed C11 modules of targets/aopf/.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "aopf/aopf.h"

void aopf_host_reset_env(void);
void aopf_host_set_ipsr(uint32_t exc_num);
const char *aopf_host_last_panic(void);
jmp_buf *aopf_host_arm_panic_trap(void);
void aopf_host_disarm_panic_trap(void);
int aopf_layout_checks_compiled(void);

extern const uint8_t g_rtkit_image_header[0x48];
extern const uint8_t g_rom_rodata_bank[0x13b14];
extern uint8_t g_ram_data_bank[0x1a0b0];

static void test_image_banks_and_headers(void)
{
    const aopf_uuid_header_t *hdr = (const aopf_uuid_header_t *)(const void *)g_rtkit_image_header;
    assert(hdr->magic == 0x64697575u);
    assert(hdr->version == 5u);
    assert(hdr->rtkit_ver_str_vma == AOPF_RTKIT_BUILD_STR_VMA);
    assert(hdr->build_banner_vma == AOPF_RAM_BUILD_BANNER_VMA);
    assert(hdr->metadata_tlv_vma == AOPF_METADATA_TLV_BASE);
    assert(hdr->metadata_tlv_size == AOPF_METADATA_TLV_SIZE);
    assert(hdr->text_end_vma == AOPF_ROM_RODATA_END);

    const char *rtkit_str = AOPF_VMA_PTR(const char *, AOPF_RTKIT_BUILD_STR_VMA);
    assert(aopf_strcmp(rtkit_str, "RTKit_release-3514.0.15.release") == 0);

    const char *banner_str = AOPF_VMA_PTR(const char *, AOPF_RAM_BUILD_BANNER_VMA);
    assert(aopf_strcmp(banner_str,
                       "b515daop:MAIN:DEVELOPMENT:AppleSPUFirmwareBuilder-990.0.13~5644") == 0);
}

static void test_libc_memory_and_strings(void)
{
    uint8_t buf[32];
    rtk_memset(buf, 0x5a, sizeof(buf));
    for (size_t idx = 0u; idx < sizeof(buf); idx++) {
        assert(buf[idx] == 0x5au);
    }

    const char sample[] = "AppleSPU-Laguna-AOP";
    rtk_memcpy_wrap(buf, sample, sizeof(sample));
    assert(aopf_strlen((const char *)buf) == sizeof(sample) - 1u);
    assert(aopf_memchr(buf, 'L', sizeof(sample)) == &buf[9]);
    assert(aopf_memchr(buf, 'Z', sizeof(sample)) == NULL);

    /* Overlapping memmove forward and backward */
    rtk_memmove_entry(buf + 4, buf, 10u);
    assert(aopf_strncmp((const char *)(buf + 4), "AppleSPU-L", 10u) == 0);

    /* Word-aligned vs byte-mismatch memcmp semantics */
    const uint8_t m_left[8]  = {1, 2, 3, 4, 0x10, 0x20, 0x30, 0x40};
    const uint8_t m_right[8] = {1, 2, 3, 4, 0x10, 0x20, 0x31, 0x40};
    assert(aopf_memcmp(m_left, m_right, 8u) == -1);
    assert(aopf_memcmp(m_right, m_left, 8u) == 1);
    assert(aopf_memcmp(m_left, m_left, 8u) == 0);

    char dst[12];
    size_t full = rtk_strlcpy(dst, "0123456789ABCDEF", sizeof(dst));
    assert(full == 16u);
    assert(aopf_strcmp(dst, "0123456789A") == 0);

    char cat_buf[16] = "AOP-";
    size_t cat_tot = rtk_strlcat(cat_buf, "RTKit-3514", sizeof(cat_buf));
    assert(cat_tot == 14u);
    assert(aopf_strcmp(cat_buf, "AOP-RTKit-3514") == 0);

    char stp_buf[8];
    char *end_p = rtk_stpncpy(stp_buf, "abc", sizeof(stp_buf));
    assert(end_p == stp_buf + 3);
    assert(stp_buf[3] == '\0' && stp_buf[7] == '\0');
}

static void test_shifts_crc_and_printf(void)
{
    assert(rtk_u64_shl(0x123456789abcdef0ull, 0u) == 0x123456789abcdef0ull);
    assert(rtk_u64_shl(0x123456789abcdef0ull, 12u) == 0x456789abcdef0000ull);
    assert(rtk_u64_shl(0x0000000089abcdefull, 36u) == 0x9abcdef000000000ull);
    assert(rtk_u64_shl(0x123456789abcdef0ull, 64u) == 0ull);
    assert(rtk_u64_shl(0x123456789abcdef0ull, 95u) == 0ull);
    assert(rtk_u64_shl(0x123456789abcdef0ull, 256u) == 0x123456789abcdef0ull);
    assert(rtk_u64_shr(0x123456789abcdef0ull, 8u) == 0x00123456789abcdeull);
    assert(rtk_u64_shr(0x123456789abcdef0ull, 40u) == 0x0000000000123456ull);
    assert(rtk_u64_shr(0x123456789abcdef0ull, 64u) == 0ull);
    assert(rtk_u64_shr(0x123456789abcdef0ull, 95u) == 0ull);
    assert(rtk_u64_shr(0x123456789abcdef0ull, 256u) == 0x123456789abcdef0ull);
    assert(rtk_u64_div_u32(1000000000000ull, 1000000u) == 1000000ull);

    assert(rtk_abs_i32(-42) == 42 && rtk_abs_i32(42) == 42);
    assert(rtk_abs_i64(-9876543210ll) == 9876543210ll);
    assert(rtk_is_ascii_printable('A') == 1);
    assert(rtk_is_ascii_printable(0x1f) == 0);
    assert(rtk_is_ascii_printable(0x7f) == 0);

    const uint8_t crc_in[] = "123456789";
    assert(rtk_crc8_buf(0x00u, crc_in, 9u) == 0xf4u); /* CRC-8/SMBUS (poly 0x07) */

    uint8_t crash76[0x4c];
    for (size_t idx = 0u; idx < sizeof(crash76); idx++) {
        crash76[idx] = (uint8_t)idx;
    }
    uint32_t c32 = rtk_crc32_crash76(crash76);
    assert(c32 != 0u && c32 == rtk_crc32_crash76(crash76));

    char fmt_out[64];
    int n_fmt = rtk_snprintf(fmt_out, sizeof(fmt_out),
                             "%s:%d:0x%04X:%llu", "aop", -17, 0xbeefu, 123456789012ull);
    assert(n_fmt > 0);
    assert(aopf_strcmp(fmt_out, "aop:-17:0xBEEF:123456789012") == 0);

    int n_rtk = rtk_snprintf(fmt_out, sizeof(fmt_out), "%s|%o|%#x|%q", NULL, 0);
    assert(n_rtk == 16);
    assert(aopf_strcmp(fmt_out, "<null>|%o|0x0|%q") == 0);

    char tiny[5];
    int unclamped = rtk_snprintf(tiny, sizeof(tiny), "abcdefg");
    assert(unclamped == 7);
    assert(aopf_strcmp(tiny, "abcd") == 0);
}

static int g_once_counter;
static void once_cb(void) { g_once_counter++; }

static void test_heap_cxxabi_and_fastlocks(void)
{
    aopf_host_reset_env();
    *AOPF_VMA_PTR(uint32_t *, 0x20819844u) = 0x20820000u; /* current thread token */

    /* Initialize a local heap arena in simulated SRAM */
    static uint8_t arena_mem[4096] __attribute__((aligned(8)));
    rtk_heap_ctx_t heap_ctx;
    memset(&heap_ctx, 0, sizeof(heap_ctx));

    rtk_heap_block_t *initial = (rtk_heap_block_t *)(void *)arena_mem;
    uint32_t usable = 2048u;
    memset(arena_mem, 0, sizeof(arena_mem));
    rtk_heap_init_free_hdr(initial, usable);
    rtk_heap_insert_block(&heap_ctx, initial);
    *AOPF_VMA_PTR(rtk_heap_ctx_t **, AOPF_HEAP_CTX_PTR_VMA) = &heap_ctx;

    void *blk_first = rtk_malloc(64u);
    assert(blk_first != NULL);
    void *blk_zero = rtk_calloc(8u, 16u);
    assert(blk_zero != NULL);
    for (size_t idx = 0u; idx < 128u; idx++) {
        assert(((const uint8_t *)blk_zero)[idx] == 0u);
    }
    void *blk_aligned = rtk_memalign(64u, 80u);
    assert(blk_aligned != NULL && (((uintptr_t)blk_aligned & 63u) == 0u));
    rtk_free(blk_aligned);
    rtk_free(blk_zero);
    rtk_free(blk_first);

    /* C++ static local guard */
    uint8_t guard[4] = {0, 0, 0, 0};
    assert(rtk_cxa_guard_acquire(guard) == 1);
    rtk_cxa_guard_release(guard);
    assert(rtk_cxa_guard_acquire(guard) == 0);

    /* rtk_call_once */
    rtk_once_t once_state = {0, {0, 0, 0}, 0u};
    g_once_counter = 0;
    (void)rtk_call_once(&once_state, once_cb);
    (void)rtk_call_once(&once_state, once_cb);
    assert(g_once_counter == 1);
}

static int g_irq_fired;
static void dummy_isr(void *ctx)
{
    g_irq_fired = (int)(uintptr_t)ctx;
}

static void test_boot_nvic_and_drivers(void)
{
    aopf_host_reset_env();
    bus_write32(DWT_CYCCNT_ADDR, 0x13572468u);
    rtk_start();

    assert(bus_read32(SCB_VTOR_ADDR) == AOPF_VTOR_TABLE_BASE);
    assert((bus_read32(SCB_CPACR_ADDR) & 0x00f00000u) == 0x00f00000u);
    assert(*AOPF_VMA_PTR(uint32_t *, AOPF_STACK_CHK_GUARD_VMA) != 0u);

    const uint32_t *msp_bot = AOPF_VMA_PTR(const uint32_t *, AOPF_MSP_STACK_BOTTOM);
    assert(rtk_stack_unused_bytes(msp_bot, AOPF_MSP_STACK_SIZE) == 0u);

    /* NVIC registration, enable, priority, pending, dispatch */
    rtk_nvic_dev_t nvic = {0};
    assert(nvic_register_irq(&nvic, 32u, dummy_isr, (void *)(uintptr_t)77) == RTK_OK);
    assert(nvic_set_irq_enabled(&nvic, 32u, 1) == 0);
    assert(nvic_set_irq_enabled(&nvic, 32u, 1) == 1);
    assert(nvic_set_irq_priority(&nvic, 32u, 8u) == RTK_OK);
    assert(nvic_trigger_irq(&nvic, 32u) == RTK_OK);
    assert(nvic_is_irq_pending(&nvic, 32u) == 1);
    aopf_host_set_ipsr(32u);
    g_irq_fired = 0;
    nvic_dispatch_active_irq(&nvic);
    assert(g_irq_fired == 77);

    /* SysTick arm / cancel / conversion */
    rtk_systick_dev_t syst = {0};
    syst.cpu_clk_hz = 24u;
    assert(SysTick_armTicks(&syst, 5000ull) == RTK_OK);
    assert(bus_read32(SYSTICK_RVR_ADDR) == 5000u);
    bus_write32(SYSTICK_CVR_ADDR, 1234u);
    uint64_t rem = 0ull;
    assert(SysTick_cancel(&syst, &rem) == RTK_OK && rem == 1234ull);

    /* Timebase 48-bit monotonic read + epoch delta */
    rtk_timebase_dev_t tb = {0};
    static const rtk_timebase_vtable_t host_tb_vt = {
        .ioctl_fn       = Timebase_ioctl,
        .get_ticks      = Timebase_getTicks,
        .get_ticks_wrap = Timebase_getTicksWrap,
        .sync_offset    = Timebase_syncOffset,
        .us_to_ticks    = Timebase_usToTicks,
        .ticks_to_us    = Timebase_ticksToUs,
    };
    tb.hdr.vt = &host_tb_vt;
    tb.hdr.mmio_base = AOPF_MMIO_TIMEBASE_BASE;
    tb.freq_hz = 1000000u;
    tb.lo_reg_off = AOP_TIMEBASE_LO_OFF;
    tb.hi_reg_off = AOP_TIMEBASE_HI_OFF;
    tb.fast_1mhz_flag = 1u;

    bus_write32(AOPF_MMIO_TIMEBASE_BASE + AOP_TIMEBASE_LO_OFF, 0x80001000u);
    bus_write32(AOPF_MMIO_TIMEBASE_BASE + AOP_TIMEBASE_HI_OFF, 0x00000005u);
    assert(Timebase_readRaw48(&tb) == 0x000580001000ull);
    assert(rtk_timer_get_us(&tb) == 0x000580001000ull);

    /* Watchdog bark/bite timeouts */
    rtk_wdt_dev_t wdt = {0};
    static const rtk_wdt_vtable_t host_wdt_vt = {
        .ioctl_fn    = WDT_ioctl,
        .set_running = WDT_setRunning,
        .kick        = WDT_kick,
    };
    wdt.hdr.vt = &host_wdt_vt;
    wdt.hdr.mmio_base = AOPF_MMIO_AON_WDT_BASE;
    wdt.allow_bite_no_cb = 1u;
    bus_write32(AOPF_MMIO_AON_WDT_BASE, 0x8002u);
    assert(WDT_setBarkTimeoutUs(&wdt, 500000ull) == RTK_OK);
    assert(WDT_getBarkTimeoutUs(&wdt) == 500000ull);
    assert(WDT_setBiteTimeoutUs(&wdt, 375000ull) == RTK_OK);
    assert(WDT_getBiteTimeoutUs(&wdt) == 375000ull);

    /* Fuse readWords & AKF Mailbox popOne */
    rtk_fuse_dev_t fuse = {0};
    fuse.hdr.mmio_base = AOPF_MMIO_FUSE_OTP_BASE;
    bus_write32(AOPF_MMIO_FUSE_OTP_BASE + AOP_FUSE_STATUS_OFF, AOP_FUSE_READY_MAGIC);
    assert(Fuse_ioctl(&fuse, 1u, NULL, NULL) == RTK_OK);
    bus_write32(AOPF_MMIO_FUSE_OTP_BASE + AOP_FUSE_DATA_OFF, 0xcafef00du);
    uint32_t otp_out = 0u;
    uint32_t otp_cnt = 1u;
    /* Clear bit 30 so status is ready */
    bus_write32(AOPF_MMIO_FUSE_OTP_BASE + AOP_FUSE_STATUS_OFF, 0x80000461u);
    assert(Fuse_readWords(&fuse, 0x00020001u, &otp_out, &otp_cnt) == RTK_OK);
    assert(otp_out == 0xcafef00du && otp_cnt == 1u);

    rtk_mbox_dev_t mbox = {0};
    mbox.hdr.mmio_base = AOPF_MMIO_MBOX_AP_BASE;
    mbox.hdr.claimed_flags = 1u;
    mbox.err_reg_off = 0x00u;
    mbox.rx_fifo_off = 0x80u;
    bus_write32(AOPF_MMIO_MBOX_AP_BASE + 0x80u, 0x11111111u);
    bus_write32(AOPF_MMIO_MBOX_AP_BASE + 0x84u, 0x22222222u);
    bus_write32(AOPF_MMIO_MBOX_AP_BASE + 0x88u, 0x33333333u);
    bus_write32(AOPF_MMIO_MBOX_AP_BASE + 0x8cu, 0x44444444u);
    rtk_mbox_msg128_t rx_msg = {{0}};
    assert(Mbox_tryReadOne(&mbox, &rx_msg) == RTK_OK);
    assert(rx_msg.words[0] == 0x11111111u && rx_msg.words[3] == 0x44444444u);

    /* AKF Mailbox RX-empty bit 17 (0x00020000) + Mbox_armRxEmptyIrq */
    bus_write32(AOPF_MMIO_MBOX_AP_BASE + 0x00u, AKF_MBOX_RX_EMPTY_BIT);
    mbox.rx_empty_arm_en = 1u;
    mbox.rx_defer_count = 0u;
    mbox.rx_int_en_flags = 1u;
    mbox.rx_int_bit_shift = 5u;
    mbox.rx_int_set_mmio = AOPF_MMIO_MBOX_AP_BASE + 0x20u;
    assert(Mbox_popOne(&mbox, &rx_msg) == RTK_ERR_NO_MMIO);
    assert(bus_read32(AOPF_MMIO_MBOX_AP_BASE + 0x20u) == (1u << 5));

    /* Non-blocking spinlock acquire/release */
    volatile uint32_t slock = 0u;
    assert(rtk_spinlock_acquire(&slock) == RTK_OK && slock != 0u);
    assert(rtk_spinlock_acquire(&slock) == 0x00020002u);
    rtk_spinlock_release(&slock);
    assert(slock == 0u);
}

static uint32_t simple_prop_hash(uint32_t key, uint32_t nbuckets)
{
    return key % nbuckets;
}

static void test_aop_app_subsystems(void)
{
    aopf_host_reset_env();

    /* Audio TLV log buffer */
    uint8_t raw_log[32];
    memset(raw_log, 0, sizeof(raw_log));
    aop_aud_log_buf_t log_buf = { .data = raw_log, .capacity = sizeof(raw_log), .length = 0u };
    (void)aud_log_append_u32(&log_buf, 0xdeadbeefu);
    (void)aud_log_append_str(&log_buf, "vt");
    (void)aud_log_append_end(&log_buf);
    assert(log_buf.length == 5u + 4u + 1u);
    assert(raw_log[0] == 0u && raw_log[5] == 2u && raw_log[9] == 3u);

    /* Property hash table */
    aop_prop_node_t *buckets[4] = {0};
    aop_prop_node_t pool[4];
    memset(pool, 0, sizeof(pool));
    aop_prop_arena_t arena = {
        .base_addr   = (uintptr_t)pool,
        .node_count  = 4u,
        .node_stride = (uint32_t)sizeof(aop_prop_node_t),
        .index_mask  = 0x0003u,
    };
    aop_prop_table_t tbl = {
        .hash_fn     = simple_prop_hash,
        .num_buckets = 4u,
        .buckets     = buckets,
        .free_remain = 4u,
        .alloc_pool  = pool,
        .chain_arena = &arena,
    };
    aop_prop_node_t *p_one = PropertyTable_findOrAlloc(&tbl, 0x101u);
    aop_prop_node_t *p_two = PropertyTable_findOrAlloc(&tbl, 0x105u); /* same bucket mod 4 */
    assert(p_one != NULL && p_two != NULL && p_one != p_two);
    assert(Property_setInlineOrPtr(p_one, 0x4242u) == RTK_OK);
    assert(PropertyTable_lookup(&tbl, 0x101u) == p_one);
    assert(p_one->val_or_ptr == 0x4242u);

    /* Battery build string & SOCR LFSR mixer */
    const char *bni = Battery_getBuildString();
    assert(bni != NULL && aopf_strncmp(bni, "BNI:AudioProductsBattery-900.106~38", 35u) == 0);
    uint64_t socr = aop_socr_lfsr_mix(0x0123456789abcdefull);
    assert(socr == aop_socr_lfsr_mix(0x0123456789abcdefull));

    /* Panic trap verification with verbatim firmware format strings */
    if (setjmp(*aopf_host_arm_panic_trap()) == 0) {
        laguna_assert("laguna_service_t2016.c", 314u, 0);
        assert(!"unreachable");
    }
    aopf_host_disarm_panic_trap();
    assert(strstr(aopf_host_last_panic(), "laguna_service_t2016.c : 314") != NULL);
}

int main(void)
{
    assert(aopf_layout_checks_compiled() == 1);
    test_image_banks_and_headers();
    test_libc_memory_and_strings();
    test_shifts_crc_and_printf();
    test_heap_cxxabi_and_fastlocks();
    test_boot_nvic_and_drivers();
    test_aop_app_subsystems();
    printf("[aopf host_tests] PASS: all structural & behavioral checks succeeded.\n");
    return 0;
}
