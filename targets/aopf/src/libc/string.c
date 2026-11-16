/*
 * string.c — Freestanding C11 memory, string, 64-bit shift/division, CRC8,
 * and CRC32 routines reconstructed from aopf.i64.
 */

#include <stddef.h>
#include <stdint.h>
#include "aopf/cpu.h"
#include "aopf/libc.h"
#include "aopf/memmap.h"

/* @provenance RVA 0x6ce4 sub_6CE4 */
void *aopf_memchr(const void *buf, int ch, size_t len)
{
    const uint8_t *cur = (const uint8_t *)buf;
    uint8_t needle = (uint8_t)ch;

    for (size_t idx = 0; idx < len; idx++) {
        if (cur[idx] == needle) {
            return (void *)(uintptr_t)&cur[idx];
        }
    }
    return NULL;
}

/* @provenance RVA 0x6d08 sub_6D08 */
int aopf_memcmp(const void *lhs, const void *rhs, size_t len)
{
    const uint8_t *p_left = (const uint8_t *)lhs;
    const uint8_t *p_right = (const uint8_t *)rhs;

    while (len > 0u && (((uintptr_t)p_left & 3u) != 0u)) {
        uint8_t b_left = *p_left++;
        uint8_t b_right = *p_right++;
        len--;
        if (b_left != b_right) {
            return (int)b_left - (int)b_right;
        }
    }

    while (len >= 4u) {
        uint32_t w_left = (uint32_t)p_left[0]
                        | ((uint32_t)p_left[1] << 8)
                        | ((uint32_t)p_left[2] << 16)
                        | ((uint32_t)p_left[3] << 24);
        uint32_t w_right = (uint32_t)p_right[0]
                         | ((uint32_t)p_right[1] << 8)
                         | ((uint32_t)p_right[2] << 16)
                         | ((uint32_t)p_right[3] << 24);
        if (w_left != w_right) {
            uint32_t be_left = aopf_bswap32(w_left);
            uint32_t be_right = aopf_bswap32(w_right);
            return (be_left >= be_right) ? 1 : -1;
        }
        p_left += 4u;
        p_right += 4u;
        len -= 4u;
    }

    while (len > 0u) {
        uint8_t b_left = *p_left++;
        uint8_t b_right = *p_right++;
        len--;
        if (b_left != b_right) {
            return (int)b_left - (int)b_right;
        }
    }
    return 0;
}

static inline uint32_t load_le32(const uint8_t *ptr)
{
    return (uint32_t)ptr[0]
         | ((uint32_t)ptr[1] << 8)
         | ((uint32_t)ptr[2] << 16)
         | ((uint32_t)ptr[3] << 24);
}

static inline void store_le32(uint8_t *ptr, uint32_t word_val)
{
    ptr[0] = (uint8_t)word_val;
    ptr[1] = (uint8_t)(word_val >> 8);
    ptr[2] = (uint8_t)(word_val >> 16);
    ptr[3] = (uint8_t)(word_val >> 24);
}

/* @provenance RVA 0x6e46 rtk_memcpy */
void *rtk_memcpy(void *dst, const void *src, size_t len, uint32_t delta_r3)
{
    uint8_t *out_p = (uint8_t *)dst;
    const uint8_t *in_p = (const uint8_t *)src;

    if ((size_t)delta_r3 >= len) {
        if (len >= 4u) {
            while (((uintptr_t)out_p & 3u) != 0u) {
                *out_p++ = *in_p++;
                len--;
            }
            if (((uintptr_t)in_p & 3u) == 0u) {
                while (len >= 64u) {
                    uint32_t blk0[8];
                    for (size_t idx = 0u; idx < 8u; idx++) {
                        blk0[idx] = load_le32(in_p + idx * 4u);
                    }
                    in_p += 32u;
                    for (size_t idx = 0u; idx < 8u; idx++) {
                        store_le32(out_p + idx * 4u, blk0[idx]);
                    }
                    out_p += 32u;

                    uint32_t blk1[8];
                    for (size_t idx = 0u; idx < 8u; idx++) {
                        blk1[idx] = load_le32(in_p + idx * 4u);
                    }
                    in_p += 32u;
                    for (size_t idx = 0u; idx < 8u; idx++) {
                        store_le32(out_p + idx * 4u, blk1[idx]);
                    }
                    out_p += 32u;
                    len -= 64u;
                }
                while (len >= 16u) {
                    uint32_t quad[4];
                    for (size_t idx = 0u; idx < 4u; idx++) {
                        quad[idx] = load_le32(in_p + idx * 4u);
                    }
                    in_p += 16u;
                    for (size_t idx = 0u; idx < 4u; idx++) {
                        store_le32(out_p + idx * 4u, quad[idx]);
                    }
                    out_p += 16u;
                    len -= 16u;
                }
            }
            while (len >= 4u) {
                uint32_t word_val = load_le32(in_p);
                in_p += 4u;
                store_le32(out_p, word_val);
                out_p += 4u;
                len -= 4u;
            }
        }
        while (len > 0u) {
            *out_p++ = *in_p++;
            len--;
        }
    } else {
        in_p += len;
        out_p += len;
        if (len >= 4u) {
            while (((uintptr_t)out_p & 3u) != 0u) {
                *--out_p = *--in_p;
                len--;
            }
            if (((uintptr_t)in_p & 3u) == 0u) {
                while (len >= 64u) {
                    uint32_t blk0[8];
                    in_p -= 32u;
                    for (size_t idx = 0u; idx < 8u; idx++) {
                        blk0[idx] = load_le32(in_p + idx * 4u);
                    }
                    out_p -= 32u;
                    for (size_t idx = 0u; idx < 8u; idx++) {
                        store_le32(out_p + idx * 4u, blk0[idx]);
                    }

                    uint32_t blk1[8];
                    in_p -= 32u;
                    for (size_t idx = 0u; idx < 8u; idx++) {
                        blk1[idx] = load_le32(in_p + idx * 4u);
                    }
                    out_p -= 32u;
                    for (size_t idx = 0u; idx < 8u; idx++) {
                        store_le32(out_p + idx * 4u, blk1[idx]);
                    }
                    len -= 64u;
                }
                while (len >= 16u) {
                    uint32_t quad[4];
                    in_p -= 16u;
                    for (size_t idx = 0u; idx < 4u; idx++) {
                        quad[idx] = load_le32(in_p + idx * 4u);
                    }
                    out_p -= 16u;
                    for (size_t idx = 0u; idx < 4u; idx++) {
                        store_le32(out_p + idx * 4u, quad[idx]);
                    }
                    len -= 16u;
                }
            }
            while (len >= 4u) {
                in_p -= 4u;
                uint32_t word_val = load_le32(in_p);
                out_p -= 4u;
                store_le32(out_p, word_val);
                len -= 4u;
            }
        }
        while (len > 0u) {
            *--out_p = *--in_p;
            len--;
        }
    }
    return dst;
}

/* @provenance RVA 0x6e40 sub_6E40 */
void *rtk_memmove_entry(void *dst, const void *src, size_t len)
{
    uint32_t delta = (uint32_t)((uintptr_t)dst - (uintptr_t)src);
    if (delta == 0u) {
        return dst;
    }
    return rtk_memcpy(dst, src, len, delta);
}

/* @provenance RVA 0x6e34 rtk_memcpy_wrap */
void *rtk_memcpy_wrap(void *dst, const void *src, size_t len)
{
    return rtk_memmove_entry(dst, src, len);
}

/* @provenance RVA 0x6f50 rtk_memset */
void *rtk_memset(void *dst, int byte_val, size_t len)
{
    uint8_t *out_bytes = (uint8_t *)dst;
    uint8_t fill = (uint8_t)byte_val;

    for (size_t idx = 0; idx < len; idx++) {
        out_bytes[idx] = fill;
    }
    return dst;
}

/* @provenance RVA 0x6fd8 sub_6FD8 */
int aopf_strcmp(const char *lhs, const char *rhs)
{
    const uint8_t *p_left = (const uint8_t *)lhs;
    const uint8_t *p_right = (const uint8_t *)rhs;

    for (;;) {
        uint8_t c_left = *p_left++;
        uint8_t c_right = *p_right++;
        if (c_left == 0u || c_left != c_right) {
            return (int)c_left - (int)c_right;
        }
    }
}

/* @provenance RVA 0x7080 sub_7080 */
size_t aopf_strlen(const char *str)
{
    size_t count = 0u;
    while (str[count] != '\0') {
        count++;
    }
    return count;
}

/* @provenance RVA 0x7108 sub_7108 */
int aopf_strncmp(const char *lhs, const char *rhs, size_t max_len)
{
    const uint8_t *p_left = (const uint8_t *)lhs;
    const uint8_t *p_right = (const uint8_t *)rhs;

    while (max_len > 0u) {
        uint8_t c_left = *p_left++;
        uint8_t c_right = *p_right++;
        max_len--;
        if (c_left == 0u || c_left != c_right) {
            return (int)c_left - (int)c_right;
        }
    }
    return 0;
}

/* @provenance RVA 0x4fc7a sub_4FC7A */
size_t rtk_strnlen(const char *str, size_t max_len)
{
    size_t count = 0u;
    while (count < max_len && str[count] != '\0') {
        count++;
    }
    return count;
}

/* @provenance RVA 0x4fbac sub_4FBAC */
char *rtk_stpncpy(char *dst, const char *src, size_t max_len)
{
    size_t copy_len = rtk_strnlen(src, max_len);
    if (max_len > copy_len) {
        rtk_memcpy_wrap(dst, src, copy_len);
        rtk_memset(dst + copy_len, 0, max_len - copy_len);
        return dst + copy_len;
    }
    rtk_memcpy_wrap(dst, src, max_len);
    return dst + max_len;
}

/* @provenance RVA 0x4fbf2 sub_4FBF2 */
size_t rtk_strlcat(char *dst, const char *src, size_t dst_size)
{
    size_t dst_len = rtk_strnlen(dst, dst_size);
    if (dst_size == 0u || dst_size == dst_len) {
        return dst_len + aopf_strlen(src);
    }
    size_t avail = dst_size - dst_len - 1u;
    size_t copied = 0u;
    while (copied < avail && src[copied] != '\0') {
        dst[dst_len + copied] = src[copied];
        copied++;
    }
    dst[dst_len + copied] = '\0';
    return dst_len + copied + aopf_strlen(src + copied);
}

/* @provenance RVA 0x4fc48 sub_4FC48 */
size_t rtk_strlcpy(char *dst, const char *src, size_t dst_size)
{
    if (dst_size == 0u) {
        return aopf_strlen(src);
    }
    size_t limit = dst_size - 1u;
    size_t copied = 0u;
    while (copied < limit && src[copied] != '\0') {
        dst[copied] = src[copied];
        copied++;
    }
    dst[copied] = '\0';
    return copied + aopf_strlen(src + copied);
}

static inline uint32_t arm_lsl32(uint32_t word_val, uint32_t shift_reg)
{
    uint32_t amt = shift_reg & 0xffu;
    if (amt == 0u) {
        return word_val;
    }
    if (amt >= 32u) {
        return 0u;
    }
    return word_val << amt;
}

static inline uint32_t arm_lsr32(uint32_t word_val, uint32_t shift_reg)
{
    uint32_t amt = shift_reg & 0xffu;
    if (amt == 0u) {
        return word_val;
    }
    if (amt >= 32u) {
        return 0u;
    }
    return word_val >> amt;
}

/* @provenance RVA 0x749c sub_749C */
uint64_t rtk_u64_shl(uint64_t val, uint32_t shift_amt)
{
    uint32_t lo_word = (uint32_t)val;
    uint32_t hi_word = (uint32_t)(val >> 32);

    if ((shift_amt & 0x20u) != 0u) {
        uint32_t new_hi = arm_lsl32(lo_word, shift_amt - 32u);
        return (uint64_t)new_hi << 32;
    }
    if (shift_amt == 0u) {
        return val;
    }
    uint32_t new_hi = arm_lsl32(hi_word, shift_amt) | arm_lsr32(lo_word, 32u - shift_amt);
    uint32_t new_lo = arm_lsl32(lo_word, shift_amt);
    return ((uint64_t)new_hi << 32) | new_lo;
}

/* @provenance RVA 0x7be0 sub_7BE0 */
uint64_t rtk_u64_shr(uint64_t val, uint32_t shift_amt)
{
    uint32_t lo_word = (uint32_t)val;
    uint32_t hi_word = (uint32_t)(val >> 32);

    if ((shift_amt & 0x20u) != 0u) {
        uint32_t new_lo = arm_lsr32(hi_word, shift_amt - 32u);
        return (uint64_t)new_lo;
    }
    if (shift_amt == 0u) {
        return val;
    }
    uint32_t new_lo = arm_lsr32(lo_word, shift_amt) | arm_lsl32(hi_word, 32u - shift_amt);
    uint32_t new_hi = arm_lsr32(hi_word, shift_amt);
    return ((uint64_t)new_hi << 32) | new_lo;
}

#if defined(__arm__) || defined(__thumb__)
uint64_t __aeabi_llsl(uint64_t val, uint32_t shift_amt);
uint64_t __aeabi_llsl(uint64_t val, uint32_t shift_amt)
{
    return rtk_u64_shl(val, shift_amt);
}

uint64_t __aeabi_llsr(uint64_t val, uint32_t shift_amt);
uint64_t __aeabi_llsr(uint64_t val, uint32_t shift_amt)
{
    return rtk_u64_shr(val, shift_amt);
}
#endif

/* @provenance RVA 0x60b52 sub_60B52 */
uint64_t rtk_u64_div_u32(uint64_t numer, uint32_t denom)
{
    if (denom == 0u) {
        return 0u;
    }
    uint64_t quot = 0u;
    uint64_t rem = 0u;
    uint64_t cur = numer;
    uint64_t d64 = (uint64_t)denom;

    for (uint32_t step = 0u; step < 64u; step++) {
        rem = (rem << 1) | (cur >> 63);
        cur <<= 1;
        quot <<= 1;
        if (rem >= d64) {
            rem -= d64;
            quot |= 1u;
        }
    }
    return quot;
}

/* @provenance RVA 0x4f3dc sub_4F3DC */
int32_t rtk_abs_i32(int32_t val)
{
    return (val < 0) ? (int32_t)(0u - (uint32_t)val) : val;
}

/* @provenance RVA 0x4f3e4 sub_4F3E4 */
int64_t rtk_abs_i64(int64_t val)
{
    uint64_t mask = (val < 0) ? ~0ull : 0ull;
    return (int64_t)(((uint64_t)val ^ mask) - mask);
}

/* @provenance RVA 0x4f452 sub_4F452 */
uint8_t rtk_crc8_step(uint8_t crc_acc, uint8_t data_byte)
{
    uint8_t cur = crc_acc ^ data_byte;
    for (uint32_t bit_idx = 0u; bit_idx < 8u; bit_idx++) {
        if ((cur & 0x80u) != 0u) {
            cur = (uint8_t)((cur << 1) ^ 0x07u);
        } else {
            cur = (uint8_t)(cur << 1);
        }
    }
    return cur;
}

/* @provenance RVA 0x4f438 sub_4F438 */
uint8_t rtk_crc8_buf(uint8_t crc_init, const uint8_t *buf, size_t len)
{
    uint8_t acc = crc_init;
    for (size_t idx = 0u; idx < len; idx++) {
        acc = rtk_crc8_step(acc, buf[idx]);
    }
    return acc;
}

/* @provenance RVA 0x4f474 sub_4F474 */
uint32_t rtk_crc32_crash76(const uint8_t *buf76)
{
    const uint32_t *crc_tbl = AOPF_VMA_PTR(const uint32_t *, AOPF_CRC32_TABLE_BASE);
    uint32_t crc = 0xffffffffu;

    for (size_t idx = 0u; idx < 0x4cu; idx++) {
        uint32_t tbl_idx = (crc ^ buf76[idx]) & 0xffu;
        crc = crc_tbl[tbl_idx] ^ (crc >> 8);
    }
    return ~crc;
}

/* @provenance RVA 0x4f4a2 sub_4F4A2 */
int rtk_is_ascii_printable(int ch)
{
    return ((uint32_t)ch - 0x20u) < 0x5fu ? 1 : 0;
}

#if defined(__arm__) || defined(__thumb__)
/* Compiler-emitted freestanding EABI / ISO C aliases */
void *memcpy(void *dst, const void *src, size_t len);
void *memmove(void *dst, const void *src, size_t len);
void *memset(void *dst, int byte_val, size_t len);
int memcmp(const void *lhs, const void *rhs, size_t len);

void *memcpy(void *dst, const void *src, size_t len)
{
    return rtk_memmove_entry(dst, src, len);
}

void *memmove(void *dst, const void *src, size_t len)
{
    return rtk_memmove_entry(dst, src, len);
}

void *memset(void *dst, int byte_val, size_t len)
{
    return rtk_memset(dst, byte_val, len);
}

int memcmp(const void *lhs, const void *rhs, size_t len)
{
    return aopf_memcmp(lhs, rhs, len);
}
#endif
