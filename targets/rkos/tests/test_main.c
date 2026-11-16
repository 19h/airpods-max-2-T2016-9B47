#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_bus.h"

extern int rkos_layout_check_anchor(void);

static int g_checks_run = 0;
static int g_checks_failed = 0;

#define CHECK(cond, desc)                                                       \
    do {                                                                        \
        g_checks_run++;                                                         \
        if (!(cond)) {                                                          \
            fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (desc));  \
            g_checks_failed++;                                                  \
        }                                                                       \
    } while (0)

static int sample_irq_handler(uint32_t ctx_va)
{
    uint32_t *counter = RKOS_VMA_PTR(uint32_t *, ctx_va);
    (*counter) += 0x10u;
    return 0x5A5Au;
}

static void test_libc_and_div64(void)
{
    rkos_host_bus_reset();

    uint8_t buf[64];
    rkos_memset(buf, 0xA5, sizeof(buf));
    CHECK(rkos_mem_is_const_byte(buf, 0xA5u, sizeof(buf)) == 1, "mem_is_const_byte match");
    buf[31] = 0xA4u;
    CHECK(rkos_mem_is_const_byte(buf, 0xA5u, sizeof(buf)) == 0, "mem_is_const_byte mismatch");

    const char *alpha = "0123456789abcdef";
    rkos_memcpy(buf, alpha, 17u);
    CHECK(rkos_strlen((const char *)buf) == 16u, "rkos_strlen");
    CHECK(rkos_strnlen((const char *)buf, 8u) == 8u, "rkos_strnlen capped");
    CHECK(rkos_strcmp((const char *)buf, alpha) == 0, "rkos_strcmp equal");
    CHECK(rkos_strncmp((const char *)buf, "01234599", 6u) == 0, "rkos_strncmp prefix");
    CHECK(rkos_strchr((const char *)buf, 'a') == (char *)(buf + 10u), "rkos_strchr found");
    CHECK(rkos_memchr(buf, 'f', 16u) == buf + 15u, "rkos_memchr found");

    /* Overlapping backward & forward memmove */
    rkos_memmove_body(buf + 2u, buf, 10u, 2u);
    CHECK(rkos_memcmp(buf + 2u, "0123456789", 10u) == 0, "rkos_memmove_body backward");
    rkos_memmove_body(buf, buf + 2u, 10u, (uintptr_t)buf - (uintptr_t)(buf + 2u));
    CHECK(rkos_memcmp(buf, "0123456789", 10u) == 0, "rkos_memmove_body forward");

    char dst[8];
    CHECK(rkos_strlcpy(dst, "hello_world", sizeof(dst)) == 11u, "rkos_strlcpy len");
    CHECK(rkos_strcmp(dst, "hello_w") == 0, "rkos_strlcpy NUL-truncation");

    char pbuf[8];
    char *pend = rkos_stpncpy(pbuf, "abc", sizeof(pbuf));
    CHECK(pend == pbuf + 3u && pbuf[3] == '\0' && pbuf[7] == '\0', "rkos_stpncpy zero-pad");

    CHECK(rkos_timingsafe_bcmp("rkosftab", "rkosftab", 8u) == 0, "timingsafe_bcmp equal");
    CHECK(rkos_timingsafe_bcmp("rkosftab", "rkosftac", 8u) != 0, "timingsafe_bcmp diff");

    uint64_t rem = 0ULL;
    uint64_t quot = rkos_udivmoddi4(0x123456789ABCDEF0ULL, 0x100003ULL, &rem);
    CHECK(quot == (0x123456789ABCDEF0ULL / 0x100003ULL), "udivmoddi4 quotient");
    CHECK(rem == (0x123456789ABCDEF0ULL % 0x100003ULL), "udivmoddi4 remainder");
    CHECK(rkos_udivdi3(1000000000000ULL, 7ULL) == (1000000000000ULL / 7ULL), "udivdi3");
    CHECK(rkos_umoddi3(1000000000000ULL, 7ULL) == (1000000000000ULL % 7ULL), "umoddi3");

    /* __memset_chk bounds trap */
    jmp_buf jb;
    if (setjmp(jb) == 0) {
        rkos_host_arm_trap(&jb);
        (void)rkos_memset_chk(buf, 0, 65u, 64u);
        rkos_host_disarm_trap();
        CHECK(0, "rkos_memset_chk should have panicked");
    } else {
        CHECK(strstr(rkos_host_last_panic_msg(), "memset") != NULL ||
              rkos_host_last_halt_code() == 4u, "rkos_memset_chk overflow trapped");
    }
}

static void test_crc32_strtoul_printf(void)
{
    rkos_host_bus_reset();

    /* IEEE 802.3 CRC-32 standard check value for "123456789" */
    uint32_t crc = rkos_crc32("123456789", 9u);
    CHECK(crc == 0xCBF43926u, "rkos_crc32(\"123456789\") == 0xCBF43926");
    CHECK(rkos_crc32("", 0u) == 0x00000000u, "rkos_crc32 empty == 0");

    char *endp = NULL;
    CHECK(rkos_strtoul("  0xDeadBeef!", &endp, 0u) == 0xDEADBEEFu && *endp == '!',
          "rkos_strtoul auto-hex");
    CHECK(rkos_strtoul("0755", &endp, 0u) == 0755u, "rkos_strtoul auto-octal");
    CHECK(rkos_strtoul("-1", &endp, 10u) == 0xFFFFFFFFu, "rkos_strtoul negative wrap");
    CHECK(rkos_strtoul("999999999999999", &endp, 10u) == 0xFFFFFFFFu, "rkos_strtoul overflow clamp");

    char out[128];
    int n = rkos_snprintf(out, sizeof(out), "tag=%s v=%d hex=%08x u64=%llx",
                          "rkos", -42, 0x1A2Bu, 0x1122334455667788ULL);
    CHECK(n > 0 && strcmp(out, "tag=rkos v=-42 hex=00001a2b u64=1122334455667788") == 0,
          "rkos_snprintf formatting");

    char tiny[6];
    int full_len = rkos_snprintf(tiny, sizeof(tiny), "0123456789");
    CHECK(full_len == 10 && strcmp(tiny, "01234") == 0, "rkos_snprintf truncation");
}

static void test_der_and_crypto(void)
{
    rkos_host_bus_reset();

    /* Place DER test vectors in host scratch VMA 0x02000000 */
    const uint32_t scratch_va = 0x02000000u;
    uint8_t *scratch = (uint8_t *)RKOS_VMA_RAW(scratch_va, 512u);

    /* 1. Valid SEQUENCE (0x30) short length 5 */
    const uint8_t der_seq[] = { 0x30u, 0x05u, 'I', 'M', '4', 'P', '!' };
    memcpy(scratch, der_seq, sizeof(der_seq));
    rkos_der_input_t slice = { scratch_va, sizeof(der_seq) };
    rkos_der_tlv_t tlv;
    uint32_t raw_val_len = 0u;
    CHECK(rkos_der_decode_ex(&slice, &tlv, &raw_val_len) == 0, "DER short SEQUENCE valid");
    CHECK(tlv.tag_number_lo == 0x10u && tlv.content_len == 5u &&
          tlv.content_va == scratch_va + 2u && raw_val_len == 5u,
          "DER short SEQUENCE fields");

    /* 2. Valid long-form length (0x81 0x80 = 128 bytes) */
    scratch[32] = 0x04u;
    scratch[33] = 0x81u;
    scratch[34] = 0x80u;
    memset(scratch + 35, 0xCC, 128u);
    slice.ptr_va = scratch_va + 32u;
    slice.length = 131u;
    CHECK(rkos_der_decode(&slice, &tlv) == 0 && tlv.content_len == 128u,
          "DER 0x81 0x80 OCTET STRING valid");

    /* 3. Non-minimal DER length (0x81 0x05 < 0x80) MUST return error 3 */
    const uint8_t der_bad_nonmin[] = { 0x04u, 0x81u, 0x05u, 1, 2, 3, 4, 5 };
    memcpy(scratch + 200u, der_bad_nonmin, sizeof(der_bad_nonmin));
    slice.ptr_va = scratch_va + 200u;
    slice.length = sizeof(der_bad_nonmin);
    CHECK(rkos_der_decode(&slice, &tlv) == 3, "DER non-minimal length rejected");

    /* 4. High-tag-number context tag [999] (0x9F, 0x87, 0x67, len=2) */
    const uint8_t der_high_tag[] = { 0x9Fu, 0x87u, 0x67u, 0x02u, 0xAAu, 0xBBu };
    memcpy(scratch + 240u, der_high_tag, sizeof(der_high_tag));
    slice.ptr_va = scratch_va + 240u;
    slice.length = sizeof(der_high_tag);
    CHECK(rkos_der_decode(&slice, &tlv) == 0 && tlv.tag_number_lo == 999u && tlv.content_len == 2u,
          "DER high-tag [999] decoded");

    /* 5. NIST FIPS 180-4 SHA-256("abc") via firmware ccdigest_info @ 0x0001612C */
    uint8_t sha256_out[32];
    const uint8_t nist_sha256_abc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    rkos_ccdigest_oneshot(0x0001612Cu, 3u, "abc", sha256_out);
    CHECK(memcmp(sha256_out, nist_sha256_abc, 32u) == 0,
          "SHA-256(\"abc\") matches NIST FIPS 180-4 using ROM K512@0x16180");

    /* 6. NIST FIPS 180-4 SHA-384("abc") via firmware ccdigest_info @ 0x00016154 */
    uint8_t sha384_out[48];
    const uint8_t nist_sha384_abc[48] = {
        0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b,
        0xb5, 0xa0, 0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07,
        0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63,
        0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed,
        0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23,
        0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7
    };
    rkos_ccdigest_oneshot(0x00016154u, 3u, "abc", sha384_out);
    CHECK(memcmp(sha384_out, nist_sha384_abc, 48u) == 0,
          "SHA-384(\"abc\") matches NIST FIPS 180-4 using ROM ccdigest_info@0x16154");

    /* 7. RFC 4231 Test Case 2 HMAC-SHA256("Jefe", "what do ya want for nothing?") */
    uint8_t hmac_out[32];
    const uint8_t rfc4231_tc2[32] = {
        0x5b, 0xdc, 0xc1, 0x46, 0xbf, 0x60, 0x75, 0x4e,
        0x6a, 0x04, 0x24, 0x26, 0x08, 0x95, 0x75, 0xc7,
        0x5a, 0x00, 0x3f, 0x08, 0x9d, 0x27, 0x39, 0x83,
        0x9d, 0xec, 0x58, 0xb9, 0x64, 0xec, 0x38, 0x43
    };
    const char *msg = "what do ya want for nothing?";
    rkos_cchmac_oneshot(0x0001612Cu, 4u, "Jefe", strlen(msg), msg, hmac_out);
    CHECK(memcmp(hmac_out, rfc4231_tc2, 32u) == 0,
          "HMAC-SHA256 matches RFC 4231 Test Case 2");
}

static void test_hardened_heap(void)
{
    rkos_host_bus_reset();
    rkos_stack_guard_init();

    uint32_t p_first = rtkit_malloc(48u);
    uint32_t p_second = rkos_calloc(8u, 16u);
    uint32_t p_aligned = rkos_memalign(64u, 96u);

    CHECK(p_first >= 0x20001140u && p_first < 0x2001D140u, "rtkit_malloc in main arena");
    CHECK(p_second != 0u && rkos_mem_is_const_byte(RKOS_VMA_RAW(p_second, 128u), 0u, 128u) == 1,
          "rkos_calloc zero-initialized");
    CHECK(p_aligned != 0u && (p_aligned & 63u) == 0u, "rkos_memalign 64B aligned");

    /* Grow via realloc and verify payload preservation */
    uint8_t *first_bytes = (uint8_t *)RKOS_VMA_RAW(p_first, 48u);
    for (unsigned int i = 0u; i < 48u; i++) {
        first_bytes[i] = (uint8_t)(i + 1u);
    }
    uint32_t p_grown = rkos_realloc(p_first, 200u);
    CHECK(p_grown != 0u, "rkos_realloc grow succeeded");
    const uint8_t *grown_bytes = (const uint8_t *)RKOS_VMA_RAW(p_grown, 200u);
    bool preserved = true;
    for (unsigned int i = 0u; i < 48u; i++) {
        if (grown_bytes[i] != (uint8_t)(i + 1u)) {
            preserved = false;
        }
    }
    CHECK(preserved, "rkos_realloc preserved payload");

    rtkit_free(p_aligned);
    rtkit_free(p_second);
    rtkit_free(p_grown);

    uint32_t heap_va = *RKOS_VMA_PTR(const uint32_t *, 0x20001078u);
    CHECK(heap_va == 0x2001DC38u, "lazy g_kernel_heap pointer at 0x2001DC38");
    CHECK(heap_alloc_or_coalesce(heap_va) == g_state_plat.kernel_heap.free_head_va + 0x0Cu,
          "heap integrity & coalesce pass");
    CHECK(g_state_plat.kernel_heap.alloc_count == 0u &&
          g_state_plat.kernel_heap.used_bytes == 0u,
          "all heap blocks cleanly returned to free list");

    /* Verify hardened heap catches double-free ("Dblfree:%p") on uncoalesced block */
    uint32_t guard_hi = rtkit_malloc(32u);
    uint32_t victim = rtkit_malloc(32u);
    uint32_t guard_lo = rtkit_malloc(32u);
    CHECK(guard_hi != 0u && guard_lo != 0u, "guard allocations around victim");
    rtkit_free(victim);
    jmp_buf jb;
    if (setjmp(jb) == 0) {
        rkos_host_arm_trap(&jb);
        rtkit_free(victim);
        rkos_host_disarm_trap();
        CHECK(0, "double free should have panicked");
    } else {
        CHECK(strncmp(rkos_host_last_panic_msg(), "Dblfree:", 8u) == 0,
              "double-free triggers exact 'Dblfree:%p' panic");
    }

    /* Reset and verify use-after-free scrub corruption ("Freeblk nonzero:%p") */
    rkos_host_bus_reset();
    rkos_stack_guard_init();
    uint32_t blk_user = rtkit_malloc(64u);
    rtkit_free(blk_user);
    *RKOS_VMA_PTR(uint8_t *, blk_user + 8u) = 0x42u; /* corrupt scrubbed free block */
    if (setjmp(jb) == 0) {
        rkos_host_arm_trap(&jb);
        (void)rtkit_malloc(32u);
        rkos_host_disarm_trap();
        CHECK(0, "corrupted free block should have panicked");
    } else {
        CHECK(strncmp(rkos_host_last_panic_msg(), "Freeblk nonzero:", 16u) == 0,
              "UAF write triggers exact 'Freeblk nonzero:%p' panic");
    }
}

static void test_nvic_sched_mutex(void)
{
    rkos_host_bus_reset();

    /* NVIC enable/pending/priority + IRQ_CommonHandler dispatch */
    const uint32_t vec = 20u; /* External IRQ 4 */
    CHECK(RKOS_REG32_READ(RKOS_NVIC_ISER_BASE) == 0u, "NVIC vec 20 initially disabled");
    (void)rkos_nvic_set_enable(0u, vec, 1);
    CHECK((RKOS_REG32_READ(RKOS_NVIC_ISER_BASE) & (1u << 4)) != 0u, "NVIC vec 20 enabled");
    CHECK(rkos_nvic_set_pending(0u, vec) == 0u && rkos_nvic_is_pending(0u, vec) == 1,
          "NVIC vec 20 pended");
    CHECK(rkos_nvic_clear_pending(0u, vec) == 0u && rkos_nvic_is_pending(0u, vec) == 0,
          "NVIC vec 20 pending cleared");
    CHECK(rkos_nvic_set_priority(0u, vec, 4u) == 0u, "NVIC set_priority valid");

    const uint32_t ctx_va = 0x02000100u;
    *RKOS_VMA_PTR(uint32_t *, ctx_va) = 5u;
    uint32_t cb_va = rkos_host_register_cb(sample_irq_handler);
    CHECK(rkos_nvic_register_handler(0u, vec, cb_va, ctx_va) == 0u, "NVIC register_handler");
    rkos_host_set_ipsr(vec);
    CHECK(IRQ_CommonHandler() == 0x5A5Au && *RKOS_VMA_PTR(uint32_t *, ctx_va) == 0x15u,
          "IRQ_CommonHandler dispatched registered handler with context");
    rkos_host_set_ipsr(0u);

    /* Scheduler 64-priority BSD TAILQ & ready_mask (init empty tqh_last = &tqh_first) */
    const uint32_t head_slot_12 = 0x2001DCD8u + 8u * 12u;
    *RKOS_VMA_PTR(uint32_t *, head_slot_12 + 4u) = head_slot_12;
    const uint32_t node_first = 0x02000200u;
    const uint32_t node_second = 0x02000240u;
    (void)rkos_sched_tailq_enqueue(node_first, 12u);
    (void)rkos_sched_tailq_enqueue(node_second, 12u);
    CHECK((g_state_sched.ready_mask & (1ULL << 12)) != 0ULL, "ready_mask bit 12 set");
    uint32_t n0_next = *RKOS_VMA_PTR(uint32_t *, node_first);
    uint32_t n0_prev = *RKOS_VMA_PTR(uint32_t *, node_first + 4u);
    (void)rkos_sched_tailq_remove(n0_next, n0_prev, 12u);
    CHECK((g_state_sched.ready_mask & (1ULL << 12)) != 0ULL, "ready_mask bit 12 stays set with 1 thread");
    uint32_t n1_next = *RKOS_VMA_PTR(uint32_t *, node_second);
    uint32_t n1_prev = *RKOS_VMA_PTR(uint32_t *, node_second + 4u);
    (void)rkos_sched_tailq_remove(n1_next, n1_prev, 12u);
    CHECK((g_state_sched.ready_mask & (1ULL << 12)) == 0ULL, "ready_mask bit 12 cleared when empty");

    /* Stack watermark & StOvfl detection */
    const uint32_t stk_va = 0x02001000u;
    (void)rkos_stack_paint_words(stk_va, 0x100u);
    *RKOS_VMA_PTR(uint32_t *, stk_va + 0xECu) = 0x11111111u;
    CHECK(rkos_stack_scan_unused(stk_va, 0x100u) == 0x18u, "stack_scan_unused exact watermark");

    /* Recursive mutex lock/trylock/unlock */
    const uint32_t fake_thread_va = 0x02002000u;
    const uint32_t mtx_va = 0x02002100u;
    *RKOS_VMA_PTR(uint32_t *, RKOS_CURRENT_TASK_VA) = fake_thread_va;
    CHECK(mutex_lock(mtx_va) == 0u, "outer mutex_lock returns 0");
    CHECK(rkos_mutex_trylock(mtx_va) == 0x20002, "recursive mutex_trylock returns 0x20002");
    CHECK(mutex_unlock(mtx_va) == 0, "mutex_unlock releases ownership");
    CHECK(*RKOS_VMA_PTR(uint32_t *, mtx_va) == 0u, "mutex owner cleared");

    /* Memory region table (default init registers slot 7 ROM + slot 8 SRAM; slot 6 = extra RAM) */
    CHECK(rkos_mem_region_register(6u, 0x20020000u, 0x00008000u) == 0u, "register shared SRAM slot 6");
    CHECK(rkos_mem_region_is_ro(0x20005000u), "slot 8 matches bit 8 of mask 0x135");
    CHECK(!rkos_mem_region_is_ro(0x00013200u), "slot 7 cleared in mask 0x135");
    CHECK(rkos_mem_region_is_ram(0x20024000u), "0x20024000 classified as RAM (slot 6)");
}

static void test_platform_ftab_bmid_crashlog(void)
{
    rkos_host_bus_reset();
    rkos_stack_guard_init();

    /* Static constructors from `.init_array` */
    (void)rkos_init_qspi_objects();
    uint32_t plugin_ret = rkos_init_crashlog_plugin();
    uint32_t aux_va = rkos_init_aux_heap();
    (void)rkos_init_platform_drivers();
    CHECK(aux_va == 0x2001DAA0u && g_state_plat.aux_heap.check_enabled == 1u,
          "init_aux_heap initialized 0x2001DAA0");
    CHECK(plugin_ret == 0x2001DDE8u &&
          *RKOS_VMA_PTR(uint32_t *, 0x2001DE08u) == RKOS_RTBUDDY_DRV_VA,
          "init_crashlog_plugin wired RTBuddy driver pointer");

    /* Stage-2 remap builder + BMID + rtkit_crashlog_init (6 entries @ 0x200004C8) */
    uint32_t rc = rkos_remap_build_table(0x20000u, 0x90000u, RKOS_REMAP_CFG_VA, 6u);
    CHECK(rc == 0u, "rkos_remap_build_table succeeded on firmware 0x200004C8 table");

    const uint32_t shm_va = RKOS_CRASHLOG_SHM_BASE;
    *RKOS_VMA_PTR(uint32_t *, shm_va) = 0x20u; /* rel_bmid */
    rkos_bmid_hdr_t *bmid = RKOS_VMA_PTR(rkos_bmid_hdr_t *, shm_va + 0x20u);
    bmid->magic = RKOS_BMID_MAGIC;
    bmid->magic_hi = 0u;
    bmid->version = RKOS_BMID_VERSION;
    bmid->desc_offset = 0x40u;
    *RKOS_VMA_PTR(uint32_t *, shm_va + 0x44u) = 0x60u; /* desc_off */
    *RKOS_VMA_PTR(uint32_t *, shm_va + 0x48u) = 0x80u; /* root_rel */
    *RKOS_VMA_PTR(uint32_t *, shm_va + 0x60u) = 4u;    /* max_slot */
    *RKOS_VMA_PTR(uint32_t *, shm_va + 0x64u) = 0x100u;/* entries_rel */
    CHECK(rkos_bmid_validate(shm_va + 0x20u), "rkos_bmid_validate true");
    rtkit_crashlog_init(shm_va);
    CHECK(*RKOS_VMA_PTR(uint32_t *, shm_va + 0x100u + 0x10u) != 0u,
          "rtkit_crashlog_init populated BMID slot 0 crashlog window");

    /* rkosftab parser & tag lookup */
    uint8_t ftab_img[128];
    memset(ftab_img, 0, sizeof(ftab_img));
    rkos_ftab_hdr_t *fhdr = (rkos_ftab_hdr_t *)(void *)ftab_img;
    memcpy(fhdr->magic, "rkosftab", 8u);
    fhdr->entry_count = 2u;
    rkos_ftab_entry_t *fent = (rkos_ftab_entry_t *)(void *)(ftab_img + sizeof(rkos_ftab_hdr_t));
    memcpy(fent[0].tag, "bver", 4u);
    fent[0].data_offset = 0x1000u;
    fent[0].data_length = 0x24u;
    memcpy(fent[1].tag, "rkof", 4u);
    fent[1].data_offset = 0x4000u;
    fent[1].data_length = 0x20EC20u;

    uint32_t off = 0u, len = 0u;
    CHECK(rkos_ftab_lookup_tag_buf(ftab_img, sizeof(ftab_img), "rkof", &off, &len) == 0 &&
          off == 0x4000u && len == 0x20EC20u,
          "rkosftab lookup 'rkof' succeeded");

    /* Dynamic 16B vector push + FTAB stream seek */
    const uint32_t vec_va = 0x02003000u;
    const uint32_t item[4] = { 0x11u, 0x22u, 0x33u, 0x44u };
    CHECK(rkos_vec16_push(vec_va, item) == 0u &&
          *RKOS_VMA_PTR(uint32_t *, vec_va) == 8u &&
          *RKOS_VMA_PTR(uint32_t *, vec_va + 4u) == 1u,
          "rkos_vec16_push allocated initial 8-slot vector");

    const uint32_t stream_va = 0x02003100u;
    *RKOS_VMA_PTR(uint32_t *, stream_va + 4u) = 0x2000u;
    CHECK(rkos_ftab_stream_seek(stream_va, 0x1800u) == 0u &&
          *RKOS_VMA_PTR(uint32_t *, stream_va + 8u) == 0x1800u,
          "rkos_ftab_stream_seek valid offset");
    CHECK(rkos_ftab_stream_seek(stream_va, 0x2001u) == 0x80000001u,
          "rkos_ftab_stream_seek out-of-range rejected");

    /* Crashlog banner + CLHE dump (wire host UART mux at 0x20000718) */
    *RKOS_VMA_PTR(uint32_t *, 0x2000071Cu) = 0x02003200u;
    *RKOS_VMA_PTR(uint32_t *, 0x20000720u) = 1u;
    *RKOS_VMA_PTR(uint32_t *, 0x02003200u) = 0x02003210u;
    *RKOS_VMA_PTR(uint32_t *, 0x02003210u) = 0x02003220u;
    *RKOS_VMA_PTR(uint32_t *, 0x02003224u) = 0x00018081u;
    *RKOS_VMA_PTR(uint32_t *, RKOS_CRASHLOG_STATE_VA + 0x18u) = 0x20022000u;
    crashlog_print_banner(NULL);
    CHECK(strstr(rkos_host_uart_tx_log(), "RTKit:") != NULL &&
          strstr(rkos_host_uart_tx_log(), "AirPods3,6") != NULL,
          "crashlog_print_banner emitted RTKit & AirPods3,6 over UART");
    crashlog_dump(0x21u, 0x00003EE1u);
    const rkos_clhe_hdr_t *clhe = RKOS_VMA_PTR(const rkos_clhe_hdr_t *, 0x20022000u);
    CHECK(clhe->signature == RKOS_CLHE_MAGIC && clhe->version == RKOS_CLHE_VER_3_3 &&
          clhe->fault_pc == 0x00003EE1u,
          "crashlog_dump wrote valid CLHE v3.3 header");
}

int main(void)
{
    CHECK(rkos_layout_check_anchor() == (int)RKOS_RAM_BSS_SIZE, "compile-time layout anchor");

    test_libc_and_div64();
    test_crc32_strtoul_printf();
    test_der_and_crypto();
    test_hardened_heap();
    test_nvic_sched_mutex();
    test_platform_ftab_bmid_crashlog();

    printf("[rkos host check] %d/%d checks passed\n",
           g_checks_run - g_checks_failed, g_checks_run);
    return (g_checks_failed == 0) ? 0 : 1;
}
