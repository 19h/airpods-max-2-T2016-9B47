/*
 * libc.h — Freestanding C11 & Embedded C++ ABI runtime prototypes for aopf.
 */

#ifndef AOPF_LIBC_H
#define AOPF_LIBC_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "aopf/shared.h"

/* Memory & string primitives (src/libc/string.c) */
void *aopf_memchr(const void *buf, int ch, size_t len);
int aopf_memcmp(const void *lhs, const void *rhs, size_t len);
void *rtk_memcpy_wrap(void *dst, const void *src, size_t len);
void *rtk_memmove_entry(void *dst, const void *src, size_t len);
void *rtk_memcpy(void *dst, const void *src, size_t len, uint32_t delta_r3);
void *rtk_memset(void *dst, int byte_val, size_t len);
int aopf_strcmp(const char *lhs, const char *rhs);
size_t aopf_strlen(const char *str);
int aopf_strncmp(const char *lhs, const char *rhs, size_t max_len);
char *rtk_stpncpy(char *dst, const char *src, size_t max_len);
size_t rtk_strlcat(char *dst, const char *src, size_t dst_size);
size_t rtk_strlcpy(char *dst, const char *src, size_t dst_size);
size_t rtk_strnlen(const char *str, size_t max_len);

/* Integer, bitshift, CRC, and ASCII helpers (src/libc/string.c) */
uint64_t rtk_u64_shl(uint64_t val, uint32_t shift_amt);
uint64_t rtk_u64_shr(uint64_t val, uint32_t shift_amt);
uint64_t rtk_u64_div_u32(uint64_t numer, uint32_t denom);
int32_t rtk_abs_i32(int32_t val);
int64_t rtk_abs_i64(int64_t val);
uint8_t rtk_crc8_step(uint8_t crc_acc, uint8_t data_byte);
uint8_t rtk_crc8_buf(uint8_t crc_init, const uint8_t *buf, size_t len);
uint32_t rtk_crc32_crash76(const uint8_t *buf76);
int rtk_is_ascii_printable(int ch);

/* Formatted output engine (src/libc/printf.c) */
typedef struct rtk_fmt_sink {
    void  (*putc_fn)(struct rtk_fmt_sink *sink, int ch); /* +0x00 */
    size_t  buf_size;                                    /* +0x04 */
    char   *buf_ptr;                                     /* +0x08 */
    size_t  write_pos;                                   /* +0x0c */
    size_t  aux_word;                                    /* +0x10 */
} rtk_fmt_sink_t;

typedef struct rtk_once {
    uint8_t  done_flag;   /* +0x00 */
    uint8_t  pad_01[3];   /* +0x01..+0x03 */
    uint32_t lock_word;   /* +0x04 */
} rtk_once_t;

void rtk_vprintf_nop_putc(rtk_fmt_sink_t *sink, int ch);
char *rtk_i64_to_dec(char *buf32, int64_t val, int is_signed, int force_plus);
int rtk_vformat(rtk_fmt_sink_t *sink, const char *fmt, va_list ap);
int rtk_vprintf(const char *fmt, va_list ap);
int rtk_printf(const char *fmt, ...);
void rtk_vsnprintf_putc(rtk_fmt_sink_t *sink, int ch);
int rtk_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int rtk_vsnprintf_clamped(char *buf, size_t size, const char *fmt, va_list ap);
int rtk_snprintf(char *buf, size_t size, const char *fmt, ...);
void rtk_call_once(rtk_once_t *once_ctrl, void (*init_fn)(void));

/* Heap allocator & Embedded C++ ABI (src/libc/cxxabi.c) */
void rtk_heap_init_free_hdr(rtk_heap_block_t *blk, uint32_t byte_size);
void rtk_heap_unlink_block(rtk_heap_ctx_t *heap, rtk_heap_block_t *blk);
void rtk_heap_insert_block(rtk_heap_ctx_t *heap, rtk_heap_block_t *blk);
void *rtk_heap_alloc(rtk_heap_ctx_t *heap, size_t req_size);
void rtk_heap_free_block(rtk_heap_ctx_t *heap, void *ptr);
void rtk_heap_lock_release(uint32_t prev_status);
void *rtk_malloc(size_t req_size);
void *rtk_calloc(size_t count, size_t elem_size);
void *rtk_realloc(void *old_ptr, size_t new_size);
void *rtk_memalign(size_t alignment, size_t req_size);
void rtk_free(void *ptr);

int rtk_cxa_guard_acquire(uint8_t *guard_obj);
void rtk_cxa_guard_unlock(void);
void rtk_cxa_guard_release(uint8_t *guard_obj);
void rtk_cxa_pure_virtual(void);
void *rtk_cxx_operator_new(size_t req_size);
void *rtk_cxx_operator_new_array(size_t req_size);
void rtk_cxx_operator_delete(void *ptr);
void rtk_cxx_operator_delete_array(void *ptr);
void *rtk_cxx_malloc_thunk(size_t req_size);
void rtk_cxx_free_thunk(void *ptr);

#endif /* AOPF_LIBC_H */
