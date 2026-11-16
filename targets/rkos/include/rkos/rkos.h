#ifndef RKOS_H
#define RKOS_H

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "rkos/memmap.h"
#include "rkos/regs.h"
#include "rkos/shared.h"

/*
 * Master declarations for reconstructed C11 functions and boot/exception shims
 * in `targets/rkos/`.
 */

/* --- Indirect Vtable / Function-Pointer Dispatch (`src/rtkit/vtable_dispatch.c`) --- */
uint32_t rkos_call_fn0(uint32_t table_va, uint32_t fn_va);
uint32_t rkos_call_fn1(uint32_t table_va, uint32_t fn_va, uint32_t arg0);
uint32_t rkos_call_fn2(uint32_t table_va, uint32_t fn_va, uint32_t arg0, uint32_t arg1);
uint32_t rkos_call_fn3(uint32_t table_va, uint32_t fn_va,
                       uint32_t arg0, uint32_t arg1, uint32_t arg2);
uint32_t rkos_call_fn4(uint32_t table_va, uint32_t fn_va,
                       uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
uint32_t rkos_call_fn5(uint32_t table_va, uint32_t fn_va,
                       uint32_t arg0, uint32_t arg1, uint32_t arg2,
                       uint32_t arg3, uint32_t arg4);

/* --- Hand-written boot/exception shims, pending_c (`src/shims/arch_entry.c`) --- */
void Reset_Handler(void);
void Reset_Main(void);
void HardFault_Handler(void);
void NMI_Handler(void);
void SVCall_Handler(void);
void SysTick_Handler(void);
void rkos_sleep_save_and_wfi(void *ctx);
void rkos_sleep_restore(const void *ctx);
void rkos_halt_spin(void);

/* --- Boot, Exception, Stack & Static Constructors (`src/rtkit/boot.c`) --- */
void *rkos_stack_guard_init(void);
void __stack_chk_fail(void);
uint32_t rkos_stack_paint_words(uint32_t base_va, uint32_t byte_len);
int rkos_crit_lock_once(uint32_t *state_out);
uint32_t rkos_unwind_callsite_pc(uint32_t lr_val);
int rkos_unwind_frame(uint32_t exc_frame_va, uint32_t *out_frames,
                      uint32_t max_frames, uint32_t *out_count);
void rtkit_reboot_halt(void);
void rtkit_halt(uint32_t action);
int rkos_vpanic_report(const char *fmt, va_list ap);
void rtkit_panic(const char *fmt, ...);
void rtkit_exception_halt(uint32_t frame_va, uint32_t exc_num);
int rtkit_bad_register(uint32_t obj_va);
uint32_t rkos_init_qspi_objects(void);
uint32_t rkos_init_crashlog_plugin(void);
uint32_t rkos_init_aux_heap(void);
uint32_t rkos_init_platform_drivers(void);
void rtkit_boot_stage2(void);
void rtkit_main(void);
void rtkit_start(void);

/* --- NVIC, IRQ Dispatch & Critical Sections (`src/rtkit/irq.c`) --- */
uint32_t rkos_irq_save_and_raise_basepri(void);
int rkos_primask_disable_if_enabled(void);
int rkos_irq_enable_all(void);
int rkos_irq_are_enabled(void);
void rkos_cpu_wfi_idle(void);
uint32_t rkos_get_ipsr_num(void);
int rkos_nvic_set_enable_state(int bit_seed, uint32_t vec, int enable);
int rkos_nvic_set_enable(uint32_t self_va, uint32_t vec, int enable);
uint32_t rkos_nvic_register_handler(uint32_t self_va, uint32_t vec,
                                    uint32_t fn_va, uint32_t ctx_va);
uint32_t rkos_nvic_set_priority(uint32_t self_va, uint32_t vec, uint32_t prio_level);
uint32_t IRQ_CommonHandler(void);
int rkos_nvic_is_pending(uint32_t self_va, uint32_t vec);
uint32_t rkos_nvic_clear_pending(uint32_t self_va, uint32_t vec);
uint32_t rkos_nvic_set_pending(uint32_t self_va, uint32_t vec);
int rkos_nvic_get_noop(void);
uint32_t rkos_nvic_drv_op(uint32_t self_va, uint32_t op, uint32_t arg2, uint32_t arg3);
int rkos_nvic_is_active(uint32_t self_va, uint32_t vec);
uint32_t crit_enter(void);
uint32_t crit_exit(uint32_t state);

/* --- Freestanding Libc, String, CRC32, Strtol, Printf (`src/libc/`) --- */
void *memcpy_thunk(void *dst, const void *src, size_t n);
void *rkos_memcpy(void *dst, const void *src, size_t n);
void *rkos_memmove_body(void *dst, const void *src, size_t n, uintptr_t delta);
void *rkos_memset(void *dst, int c, size_t n);
void *rkos_memchr(const void *s, int c, size_t n);
int rkos_memcmp(const void *s1, const void *s2, size_t n);
char *rkos_strchr(const char *s, int c);
int rkos_strcmp(const char *s1, const char *s2);
size_t rkos_strlen(const char *s);
int rkos_strncmp(const char *lhs, const char *rhs, size_t n);
size_t rkos_strnlen(const char *s, size_t maxlen);
size_t rkos_strlcpy(char *dst, const char *src, size_t dstsize);
char *rkos_stpncpy(char *dst, const char *src, size_t n);
int rkos_mem_is_const_byte(const void *buf, uint8_t byte_val, size_t len);
int rkos_timingsafe_bcmp(const void *a, const void *b, size_t len);
uint32_t *rkos_word_copy32(uint32_t *dst, const uint32_t *src, size_t nbytes);
void *rkos_memset_chk(void *dst, int c, size_t len, size_t dstlen);
void rkos_memset_chk_fail(void);
uint64_t rkos_udivmoddi4(uint64_t num, uint64_t den, uint64_t *rem_out);
uint64_t rkos_udivdi3(uint64_t num, uint64_t den);
uint64_t rkos_umoddi3(uint64_t num, uint64_t den);

uint32_t rkos_crc32(const void *buf, size_t len);
const uint8_t *rkos_strtol_parse_base(const uint8_t *s, uint8_t *sign_out, uint32_t *base_inout);
uint64_t rkos_strtoull_scanner(const uint8_t **cursor, uint32_t base,
                               uint32_t max_lo, uint32_t max_hi,
                               uint64_t cutoff, uint32_t cutlim, uint8_t *sign_io);
uint32_t rkos_strtoul(const char *nptr, char **endptr, uint32_t base);

typedef struct rkos_printf_sink rkos_printf_sink_t;
typedef rkos_printf_sink_t *(*rkos_sink_putc_fn_t)(rkos_printf_sink_t *sink, int ch);
struct rkos_printf_sink {
    rkos_sink_putc_fn_t putc_fn;
    size_t              cap;
    char               *buf;
    size_t              pos;
    uint32_t            flags;
};

char *rkos_format_i64_dec(char buf[32], int pad_arg, uint64_t val, int is_signed, int force_plus);
rkos_printf_sink_t *rkos_snprintf_putc(rkos_printf_sink_t *sink, int ch);
uint32_t rkos_console_mux_putc(uint32_t mux_va, uint32_t ch);
rkos_printf_sink_t *rkos_console_putc(rkos_printf_sink_t *sink, int ch);
int rkos_vprintf_core(rkos_printf_sink_t *sink, const char *fmt, va_list ap);
int rkos_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
size_t rkos_vscnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int rkos_snprintf(char *buf, size_t size, const char *fmt, ...);
int rkos_vprintf(const char *fmt, va_list ap);
int rkos_printf(const char *fmt, ...);

/* --- ASN.1 DER & IMG4 Manifest Validation (`src/img4/img4_der.c`) --- */
int rkos_der_decode_ex(const rkos_der_input_t *in_slice, rkos_der_tlv_t *out_tlv,
                       uint32_t *raw_len_out);
int rkos_der_decode(const rkos_der_input_t *in_slice, rkos_der_tlv_t *out_tlv);
int rkos_der_decode_raw(const rkos_der_input_t *in_slice,
                        rkos_der_tlv_t *out_tlv,
                        int allow_trunc);
int rkos_der_parse_u64(const rkos_der_input_t *in_slice, uint64_t *out_val);
int rkos_der_parse_u32(const rkos_der_input_t *in_slice, uint32_t *out_val);
int rkos_der_slice_to_bounds(const rkos_der_input_t *in_slice, uint32_t out_bounds[2]);
int rkos_der_next_tlv(uint32_t cursor_bounds[2], rkos_der_tlv_t *out_tlv);
int rkos_der_parse_sequence_body(const rkos_der_input_t *seq_body,
                                 uint32_t spec_count,
                                 const rkos_der_spec_t *specs,
                                 void *out_buf,
                                 uint32_t out_size,
                                 uint32_t zero_size);
int rkos_der_parse_sequence(const rkos_der_input_t *in_slice,
                            uint32_t spec_count,
                            const rkos_der_spec_t *specs,
                            void *out_buf,
                            uint32_t out_size,
                            uint32_t zero_size);
int rkos_der_find_tag_in_bounds(uint32_t cursor_bounds[2],
                                uint32_t unused_arg,
                                uint32_t tag_lo,
                                uint32_t tag_hi,
                                rkos_der_input_t *out_slice);
int rkos_img4_cmp_fourcc(const rkos_der_input_t *in_slice, uint32_t expected_fourcc);
int rkos_img4_decode_typed_seq(const rkos_der_input_t *in_slice,
                               uint32_t spec_count,
                               const rkos_der_spec_t *specs,
                               void *out_body,
                               uint32_t expected_fourcc);
int rkos_img4_decode_im4m_signed(const rkos_der_input_t *in_slice, void *out_body);
int rkos_img4_decode_im4m_unsigned(const rkos_der_input_t *in_slice, void *out_body);
int rkos_img4_decode_im4c_signed(const rkos_der_input_t *in_slice, void *out_body);
int rkos_img4_decode_im4c_unsigned(const rkos_der_input_t *in_slice, void *out_body);
int rkos_img4_parse_prop_entry(const rkos_der_input_t *in_slice,
                               uint32_t unused_arg,
                               uint32_t prop_fourcc,
                               uint32_t prop_cls_hi,
                               uint32_t val_tag_lo,
                               uint32_t val_tag_hi,
                               uint32_t *out_item);
int rkos_img4_find_prop_in_bounds(uint32_t cursor_bounds[2],
                                  uint32_t unused_arg,
                                  uint32_t prop_fourcc,
                                  uint32_t prop_cls_hi,
                                  uint32_t val_tag_lo,
                                  uint32_t val_tag_hi,
                                  uint32_t *out_item);
int rkos_img4_find_prop_in_slice(const rkos_der_input_t *in_slice,
                                 uint32_t unused_arg,
                                 uint32_t prop_fourcc,
                                 uint32_t prop_cls_hi,
                                 uint32_t val_tag_lo,
                                 uint32_t val_tag_hi,
                                 uint32_t *out_item);
void *rkos_img4_zero_item32(void *item_buf);
int rkos_img4_get_manb_and_manp(const uint8_t *manifest_ctx,
                                rkos_der_input_t *out_manb,
                                rkos_der_input_t *out_manp);
int rkos_img4_lookup_obj_prop(const uint8_t *manifest_ctx,
                              uint32_t unused_arg,
                              uint32_t obj_fourcc,
                              uint32_t obj_cls_hi,
                              uint32_t prop_fourcc,
                              uint32_t prop_cls_hi,
                              uint32_t val_tag_lo,
                              uint32_t val_tag_hi,
                              uint32_t *out_item);
int rkos_img4_get_obj_octet_prop(const uint8_t *manifest_ctx,
                                 uint32_t unused_arg,
                                 uint32_t obj_fourcc,
                                 uint32_t obj_cls_hi,
                                 uint32_t prop_fourcc,
                                 uint32_t prop_cls_hi,
                                 uint32_t *out_data_va,
                                 uint32_t *out_data_len);
int rkos_img4_init_manifest_ctx(uint32_t buf_va,
                                uint32_t buf_len,
                                uint32_t is_signed,
                                uint8_t *out_ctx,
                                uint32_t container_fourcc);
int rkos_img4_init_im4m_signed(uint32_t buf_va, uint32_t buf_len, uint8_t *out_ctx);

/* --- CoreCrypto SHA-256/512/HMAC/DRBG (`src/kernel/crypto_der.c`) --- */
void rkos_sec_init_flag(uint32_t *out_zero);
int rkos_cc_cmp_safe(size_t len, const void *lhs_buf, const void *rhs_buf);
void rkos_store_be64(uint32_t lo, uint32_t hi, uint8_t out[8]);
void rkos_sha256_compress(uint32_t state[8], size_t nblocks, const void *data);
void rkos_sha512_compress(uint64_t state[8], size_t nblocks, const void *data);
void rkos_ccdigest_update(uint32_t di_va, void *ctx, size_t len, const void *data);
void rkos_ccdigest_final_64be(uint32_t di_va, void *ctx, uint8_t *digest);
void rkos_ccdigest_final_128be(uint32_t di_va, void *ctx, uint8_t *digest);
void rkos_ccdigest_oneshot(uint32_t di_va, size_t len, const void *data, void *digest);
int rkos_cchmac_init(uint32_t di_va, void *hctx, size_t key_len, const void *key);
void rkos_cchmac_final(uint32_t di_va, void *hctx, uint8_t *mac_out);
void rkos_cchmac_oneshot(uint32_t di_va, size_t key_len, const void *key,
                         size_t data_len, const void *data, uint8_t *mac_out);
void rkos_crypto_panic_str(const char *msg);
uint32_t rkos_drbg_clear(uint32_t drbg_va);
int rkos_drbg_need_reseed(uint32_t drbg_va);

/* --- CoreCrypto AES, BigNum (`ccn`), HMAC-DRBG & Helpers (`src/crypto/aes_ccn_drbg.c`) --- */
void rkos_aes_key_expand_enc(const uint32_t *key, uint32_t *ctx, uint32_t nk, uint32_t nr);
void rkos_aes_key_expand_enc128(const uint32_t *key, uint32_t *ctx);
void rkos_aes_key_expand_enc192(const uint32_t *key, uint32_t *ctx);
void rkos_aes_key_expand_enc256(const uint32_t *key, uint32_t *ctx);
void rkos_aes_key_expand_dec(const uint32_t *key, uint32_t *ctx, uint32_t nk, uint32_t nr);
uint32_t *rkos_aes_key_expand_dec128(uint32_t *key, uint32_t *ctx);
uint32_t *rkos_aes_key_expand_dec192(uint32_t *key, uint32_t *ctx);
uint32_t *rkos_aes_key_expand_dec256(uint32_t *key, uint32_t *ctx);
int rkos_aes_setup_enc(const uint32_t *key, int key_len_or_bits, uint32_t *ctx);
int rkos_aes_setup_dec(uint32_t *key, int key_len_or_bits, uint32_t *ctx);
int rkos_aes_encrypt_block(const uint32_t in_blk[4], uint32_t out_blk[4], const uint32_t *ctx);
int rkos_aes_decrypt_block(const uint32_t in_blk[4], uint32_t out_blk[4], const uint32_t *ctx);
void rkos_aes_cbc_decrypt(const uint32_t *in_blks, const uint32_t iv[4],
                          int nblocks, uint32_t *out_blks, const uint32_t *ctx);
void rkos_aes_cbc_encrypt(const uint32_t *in_blks, const uint32_t iv[4],
                          int nblocks, uint32_t *out_blks, const uint32_t *ctx);
void rkos_aes_ctr_crypt(const uint32_t *in_blks, uint32_t *out_blks,
                        uint32_t byte_len, uint32_t ctr_blk[4], const uint32_t *ctx);
void rkos_aes_ccm_decrypt(const uint32_t *in_blks, uint32_t *out_blks,
                          uint32_t cbc_mac[4], int nblocks,
                          const uint32_t *ctx, uint32_t ctr_blk[4], int len_field_size);
void rkos_aes_ccm_encrypt(const uint32_t *in_blks, uint32_t *out_blks,
                          uint32_t cbc_mac[4], int nblocks,
                          const uint32_t *ctx, uint32_t ctr_blk[4], int len_field_size);
uint32_t rkos_ccn_add(uint32_t count, uint32_t *dst, const uint32_t *lhs, const uint32_t *rhs);
uint32_t rkos_ccn_add1(uint32_t count, uint32_t *dst, const uint32_t *src, uint32_t val);
uint32_t rkos_ccn_addmul1(uint32_t count, uint32_t *acc, const uint32_t *src, uint32_t mul);
int rkos_ccn_cmp(uint32_t count, const uint32_t *lhs, const uint32_t *rhs);
uint32_t rkos_ccn_sub1(uint32_t count, uint32_t *dst, const uint32_t *src, uint32_t val);
uint32_t rkos_ccn_mul1(uint32_t count, uint32_t *dst, const uint32_t *src, uint32_t mul);
uint32_t rkos_ccn_n(uint32_t count, const uint32_t *src);
void rkos_ccn_set(uint32_t count, uint32_t *dst, const uint32_t *src);
uint32_t rkos_ccn_sub(uint32_t count, uint32_t *dst, const uint32_t *lhs, const uint32_t *rhs);
void rkos_ccn_shift_right(uint32_t count, uint32_t *dst, const uint32_t *src, uint32_t shift_bits);
uint32_t rkos_ref_mutex_dec_unlock(uint32_t obj_va);
void rkos_crypto_fips_abort(void);
void rkos_drbg_hmac_update(uint32_t drbg_va, uint32_t num_inputs, ...);
int rkos_drbg_hmac_init(uint32_t drbg_info_va, uint32_t drbg_va,
                        uint32_t entropy_len, const void *entropy,
                        uint32_t nonce_len, const void *nonce,
                        uint32_t ps_len, const void *ps);
int rkos_drbg_hmac_reseed_body(uint32_t drbg_va,
                               uint32_t entropy_len, const void *entropy,
                               uint32_t add_len, const void *add_in);
int rkos_drbg_hmac_reseed(uint32_t drbg_va,
                          uint32_t entropy_len, const void *entropy,
                          uint32_t add_len, const void *add_in);
int rkos_drbg_hmac_generate(uint32_t drbg_va,
                            uint32_t data_len, void *data_out,
                            uint32_t add_len, const void *add_in);
uint32_t rkos_take_and_dec_field58(uint32_t obj_va);
void *rkos_bzero32(void *dst);
void *rkos_bzero_len(void *dst, int unused_arg, size_t len);
uint32_t rkos_bitmap_shift_word(uint32_t bit_idx, uint32_t unused_arg, const uint32_t *words);
uint64_t rkos_umul_u32_u64(uint32_t unused_arg, uint32_t mul32, uint64_t val64);
int rkos_is_zero_u32(uint32_t val);
void *rkos_memcpy16(void *dst, const void *src);
uint32_t rkos_pack_low2_and_idx(uint32_t flags, uint32_t unused_arg, uint32_t idx);
int rkos_const_256(void);
uint32_t rkos_deref_table_slot(const uint32_t *table_ptr_va, uint32_t unused_arg,
                               uint32_t idx, uint32_t *out_val);

/* --- Hardened Kernel Heap (`src/kernel/heap.c`) --- */
uint32_t rkos_heap_init_struct(uint32_t heap_va);
int rkos_heap_range_in_regions(uint32_t heap_va, uint32_t start_va, uint32_t end_va);
int rkos_heap_span_valid(uint32_t heap_va, uint32_t start_va, uint32_t len);
int rkos_heap_blk_min_valid(uint32_t heap_va, uint32_t blk_va);
int rkos_heap_blk_alloc_valid(uint32_t heap_va, uint32_t blk_va);
uint32_t rkos_heap_mark_allocated(uint32_t heap_va, uint32_t blk_va,
                                  uint32_t byte_size, int track_usage);
uint32_t rkos_heap_unmark_allocated(uint32_t heap_va, uint32_t blk_va, int track_usage);
uint32_t rkos_heap_mark_free(uint32_t heap_va, uint32_t blk_va, uint32_t byte_size);
uint32_t rkos_heap_unmark_free(uint32_t heap_va, uint32_t blk_va);
int rkos_heap_freelist_unlink(uint32_t heap_va, uint32_t blk_va);
uint32_t rkos_heap_freelist_insert(uint32_t heap_va, uint32_t blk_va);
uint32_t rkos_heap_coalesce_pair(uint32_t heap_va, uint32_t left_va, uint32_t right_va);
uint32_t heap_alloc_or_coalesce(uint32_t heap_va);
uint32_t heap_alloc_impl(uint32_t heap_va, uint32_t size, uint32_t arg2, uint32_t arg3);
uint32_t heap_free_impl(uint32_t heap_va, uint32_t ptr_va, int track_usage);
uint32_t heap_free_blk(uint32_t heap_va, uint32_t ptr_va);
uint32_t rkos_kernel_heap_add_region(uint32_t base_va, uint32_t size_bytes);
uint32_t rkos_heap_disable_checks(void);
uint32_t rtkit_malloc(uint32_t size);
uint32_t rkos_realloc(uint32_t ptr_va, uint32_t new_size);
uint32_t rkos_calloc(uint32_t count, uint32_t size);
uint32_t rkos_memalign(uint32_t align, uint32_t size);
void rtkit_free(uint32_t ptr_va);
uint32_t j_rtkit_malloc(uint32_t size);
uint32_t cxx_operator_new(uint32_t size);
uint32_t cxx_operator_new_array(uint32_t size);
void j_rtkit_free(uint32_t ptr_va);
void j_rtkit_free_0(uint32_t ptr_va);
void j_rtkit_free_1(uint32_t ptr_va);
void j_rtkit_free_2(uint32_t ptr_va);
void j_rtkit_free_3(uint32_t ptr_va);
void j_rtkit_free_4(uint32_t ptr_va);
void j_rtkit_free_5(uint32_t ptr_va);
void j_rtkit_free_6(uint32_t ptr_va);
void j_rtkit_free_7(uint32_t ptr_va);
void j_rtkit_free_8(uint32_t ptr_va);
void j_rtkit_free_9(uint32_t ptr_va);
void j_rtkit_free_10(uint32_t ptr_va);

/* --- Scheduler, Ready Queue, Mutex & Memory Regions (`src/kernel/sched_sync.c`) --- */
uint32_t rtkit_sched_switch(uint32_t current_sp);
uint32_t rkos_sched_tailq_enqueue(uint32_t node_va, uint32_t prio);
uint32_t rkos_sched_tailq_remove(uint32_t next_va, uint32_t prev_link_va, uint32_t prio);
uint32_t rkos_unwind_frames_cnt(uint32_t exc_frame_va, uint32_t *out_frames, uint32_t max_frames);
uint32_t rkos_unwind_task(uint32_t task_va, uint32_t *out_frames, uint32_t max_frames);
bool rkos_task_clear_yield_check_preempt(uint32_t task_va);
uint32_t rkos_task_recalc_priority(uint32_t task_va);
uint32_t rkos_stack_scan_unused(uint32_t stack_base_va, uint32_t stack_size);
void thread_check_intr_or_stovfl(uint32_t thread_va);
int rkos_mutex_trylock(uint32_t mutex_va);
void mutex_lock_slow(uint32_t mutex_va);
uint32_t mutex_lock(uint32_t mutex_va);
int rkos_mutex_unlock_slow_impl(uint32_t mutex_va);
int mutex_unlock(uint32_t mutex_va);
uint32_t rkos_spinlock_release(uint32_t lock_va, uint32_t prev_state);
uint32_t rkos_mem_region_lookup(uint32_t addr, uint32_t *out_idx);
bool rkos_mem_region_is_ram(uint32_t addr);
bool rkos_mem_region_is_ro(uint32_t addr);
uint32_t rkos_alloc_tagged_handle(uint32_t val0, uint32_t tag_flags);
uint32_t rkos_mem_region_ensure_init(void);
uint32_t rkos_mem_region_register(int32_t slot, uint32_t base, uint32_t size);
uint32_t rkos_mem_region_register_thunk(int32_t slot, uint32_t base, uint32_t size);
uint32_t rkos_mem_region_apply_table(void);
void rkos_nullsub_1(void);
void rkos_nullsub_2(void);
void rkos_nullsub_3(void);
void rkos_nullsub_4(void);
void rkos_nullsub_5(void);
void rkos_nullsub_6(void);
void rkos_nullsub_7(void);
void rkos_nullsub_8(void);

/* --- FTAB, BMID, Stage-2 Remap & Crashlog (`src/platform/ftab_crashlog.c`) --- */
uint32_t rkos_fourcc_store_global(uint32_t fourcc);
void rkos_fourcc_to_str(uint32_t fourcc, char out[5]);
bool rkos_bmid_validate(uint32_t bmid_va);
uint32_t rkos_bmid_write_range(uint32_t base_va, uint32_t bmid_va, uint32_t slot,
                               uint32_t start_off, uint32_t length);
uint32_t rkos_remap_build_table(uint32_t text_reserve, uint32_t total_budget,
                                uint32_t cfg_va, uint32_t count);
void rtkit_crashlog_init(uint32_t shm_va);
int rkos_ftab_parse_header_buf(const void *buf, size_t buf_len, uint32_t *out_count);
int rkos_ftab_lookup_tag_buf(const void *buf, size_t buf_len, const char tag[4],
                             uint32_t *out_off, uint32_t *out_len);
uint32_t rkos_vec16_push(uint32_t vec_va, const uint32_t item[4]);
uint32_t rkos_status_unsupported(void);
uint32_t rkos_dev_claim_open(uint32_t obj_va);
uint32_t rkos_dev_release_open(uint32_t obj_va);
uint32_t rkos_ftab_stream_seek(uint32_t stream_va, uint32_t offset);
uint32_t rkos_ftab_stream_get_size(uint32_t stream_va, uint32_t *out_size);
uint32_t rkos_ftab_stream_get_backend(uint32_t stream_va, uint32_t *out_backend);
uint32_t rkos_pair_fill_same(uint32_t val, uint32_t unused_arg,
                             uint32_t *out_first, uint32_t *out_second);
bool rkos_crashlog_is_active(void);
void watchdog_expire_panic(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
uint32_t rkos_crashlog_register_OnList(uint32_t list_va, uint32_t sig, uint32_t ver,
                                       uint32_t cb_va, uint32_t ctx_va);
uint32_t rkos_crashlog_register_section(uint32_t sig, uint32_t ver,
                                        uint32_t cb_va, uint32_t ctx_va);
void crashlog_print_banner(const char *client_name);
void crashlog_dump(uint32_t reason, uint32_t fault_pc);

#endif /* RKOS_H */
