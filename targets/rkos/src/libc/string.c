#include "rkos/rkos.h"

/*
 * Reconstructed freestanding memory, string, fortified-bounds, and 64-bit
 * unsigned division/modulo routines for `rkos`.
 */

#ifdef RKOS_HOST
#define RKOS_EARLY_TEXT
#else
#define RKOS_EARLY_TEXT __attribute__((section(".text.early")))
#endif

/* @provenance RVA 0x1168 sub_1168 */
RKOS_EARLY_TEXT void *rkos_memchr(const void *s, int c, size_t n)
{
    const uint8_t *p = (const uint8_t *)s;
    uint8_t needle = (uint8_t)(c & 0xFF);

    while (n > 0u) {
        if (*p == needle) {
            return (void *)(uintptr_t)p;
        }
        p++;
        n--;
    }
    return NULL;
}

/* @provenance RVA 0x118c sub_118C */
RKOS_EARLY_TEXT int rkos_memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    if (p1 == p2 || n == 0u) {
        return 0;
    }
    if (n >= 4u) {
        while (((uintptr_t)p1 & 3u) != 0u && n > 0u) {
            int diff = (int)*p1 - (int)*p2;
            p1++;
            p2++;
            n--;
            if (diff != 0) {
                return diff;
            }
        }
        while (n >= 4u) {
            uint32_t w1 = ((uint32_t)p1[0]) | ((uint32_t)p1[1] << 8) |
                          ((uint32_t)p1[2] << 16) | ((uint32_t)p1[3] << 24);
            uint32_t w2 = ((uint32_t)p2[0]) | ((uint32_t)p2[1] << 8) |
                          ((uint32_t)p2[2] << 16) | ((uint32_t)p2[3] << 24);
            if (w1 != w2) {
                uint32_t be1 = ((w1 & 0x000000FFu) << 24) |
                               ((w1 & 0x0000FF00u) << 8) |
                               ((w1 & 0x00FF0000u) >> 8) |
                               ((w1 & 0xFF000000u) >> 24);
                uint32_t be2 = ((w2 & 0x000000FFu) << 24) |
                               ((w2 & 0x0000FF00u) << 8) |
                               ((w2 & 0x00FF0000u) >> 8) |
                               ((w2 & 0xFF000000u) >> 24);
                return (be1 < be2) ? -1 : 1;
            }
            p1 += 4;
            p2 += 4;
            n -= 4u;
        }
    }
    while (n > 0u) {
        int diff = (int)*p1 - (int)*p2;
        if (diff != 0) {
            return diff;
        }
        p1++;
        p2++;
        n--;
    }
    return 0;
}

/* @provenance RVA 0x12c6 sub_12C6 */
/* Fall-through body of 0x12C0 (`subs r3, r0, r1; it eq; bxeq lr`), receiving
 * `delta = (uintptr_t)dst - (uintptr_t)src` in AAPCS `r3`. */
RKOS_EARLY_TEXT void *rkos_memmove_body(void *dst, const void *src, size_t n, uintptr_t delta)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (delta < n) {
        d += n;
        s += n;
        while (n > 0u) {
            d--;
            s--;
            *d = *s;
            n--;
        }
    } else {
        while (n > 0u) {
            *d++ = *s++;
            n--;
        }
    }
    return dst;
}

/* @provenance RVA 0x12c0 memcpy */
RKOS_EARLY_TEXT void *rkos_memcpy(void *dst, const void *src, size_t n)
{
    uintptr_t delta = (uintptr_t)dst - (uintptr_t)src;
    if (delta == 0u || n == 0u) {
        return dst;
    }
    return rkos_memmove_body(dst, src, n, delta);
}

/* @provenance RVA 0x12b8 memcpy_thunk */
RKOS_EARLY_TEXT void *memcpy_thunk(void *dst, const void *src, size_t n)
{
    return rkos_memcpy(dst, src, n);
}

/* @provenance RVA 0x13d0 memset */
RKOS_EARLY_TEXT void *rkos_memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    uint8_t fill = (uint8_t)(c & 0xFF);

    while (n > 0u) {
        *d++ = fill;
        n--;
    }
    return dst;
}

/* @provenance RVA 0x1448 sub_1448 */
RKOS_EARLY_TEXT char *rkos_strchr(const char *s, int c)
{
    uint8_t target = (uint8_t)(c & 0xFF);
    for (;;) {
        uint8_t ch = (uint8_t)*s;
        if (ch == target) {
            return (char *)(uintptr_t)s;
        }
        if (ch == 0u) {
            return NULL;
        }
        s++;
    }
}

/* @provenance RVA 0x1478 strcmp */
RKOS_EARLY_TEXT int rkos_strcmp(const char *s1, const char *s2)
{
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (;;) {
        uint8_t c1 = *p1++;
        uint8_t c2 = *p2++;
        if (c1 == 0u || c1 != c2) {
            return (int)c1 - (int)c2;
        }
    }
}

/* @provenance RVA 0x1520 sub_1520 */
RKOS_EARLY_TEXT size_t rkos_strlen(const char *s)
{
    size_t len = 0u;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

/* @provenance RVA 0x15a8 strncmp */
RKOS_EARLY_TEXT int rkos_strncmp(const char *s1, const char *s2, size_t n)
{
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    while (n > 0u) {
        uint8_t c1 = *p1++;
        uint8_t c2 = *p2++;
        if (c1 == 0u || c1 != c2) {
            return (int)c1 - (int)c2;
        }
        n--;
    }
    return 0;
}

/* @provenance RVA 0x1682 sub_1682 */
RKOS_EARLY_TEXT uint64_t rkos_udivmoddi4(uint64_t num, uint64_t den, uint64_t *rem_out)
{
    if (den == 0ULL) {
        if (rem_out != NULL) {
            *rem_out = 0ULL;
        }
        return 0ULL;
    }
    if ((den >> 32) == 0ULL && (num >> 32) == 0ULL) {
        uint32_t q32 = (uint32_t)num / (uint32_t)den;
        if (rem_out != NULL) {
            *rem_out = (uint64_t)((uint32_t)num - q32 * (uint32_t)den);
        }
        return (uint64_t)q32;
    }

    uint64_t quot = 0ULL;
    uint64_t rem = 0ULL;
    for (int bit = 63; bit >= 0; bit--) {
        rem = (rem << 1) | ((num >> bit) & 1ULL);
        if (rem >= den) {
            rem -= den;
            quot |= (1ULL << bit);
        }
    }
    if (rem_out != NULL) {
        *rem_out = rem;
    }
    return quot;
}

/* @provenance RVA 0x1674 sub_1674 */
RKOS_EARLY_TEXT uint64_t rkos_udivdi3(uint64_t num, uint64_t den)
{
    return rkos_udivmoddi4(num, den, NULL);
}

/* @provenance RVA 0x186a sub_186A */
RKOS_EARLY_TEXT uint64_t rkos_umoddi3(uint64_t num, uint64_t den)
{
    uint64_t rem = 0ULL;
    (void)rkos_udivmoddi4(num, den, &rem);
    return rem;
}

/* @provenance RVA 0x5c7e sub_5C7E */
int rkos_mem_is_const_byte(const void *buf, uint8_t byte_val, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t word_pat = (uint32_t)byte_val * 0x01010101u;

    while (((uintptr_t)p & 3u) != 0u && len > 0u) {
        if (*p != byte_val) {
            return 0;
        }
        p++;
        len--;
    }
    while (len >= 4u) {
        uint32_t w = *(const uint32_t *)(const void *)p;
        if (w != word_pat) {
            return 0;
        }
        p += 4;
        len -= 4u;
    }
    for (size_t i = 0u; i < len; i++) {
        if (p[i] != byte_val) {
            return 0;
        }
    }
    return 1;
}

/* @provenance RVA 0x5cd2 sub_5CD2 */
int rkos_timingsafe_bcmp(const void *a, const void *b, size_t len)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    uint32_t diff = 0u;

    while (len > 0u) {
        diff |= (uint32_t)(*pa++ ^ *pb++);
        len--;
    }
    return (diff != 0u) ? 1 : 0;
}

/* @provenance RVA 0x5cf2 sub_5CF2 */
uint32_t *rkos_word_copy32(uint32_t *dst, const uint32_t *src, size_t nbytes)
{
    while (nbytes > 31u) {
        nbytes -= 32u;
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        dst[4] = src[4];
        dst[5] = src[5];
        dst[6] = src[6];
        dst[7] = src[7];
        src += 8;
        dst += 8;
    }
    size_t idx = 0u;
    while (nbytes >= 4u) {
        nbytes -= 4u;
        dst[idx] = src[idx];
        idx++;
    }
    return dst;
}

/* @provenance RVA 0x5d3c __memset_chk */
void rkos_memset_chk_fail(void)
{
    rtkit_panic("memset_chk");
}

/* @provenance RVA 0x5d34 sub_5D34 */
void *rkos_memset_chk(void *dst, int c, size_t len, size_t dstlen)
{
    if (dstlen < len) {
        rkos_memset_chk_fail();
    }
    return rkos_memset(dst, c, len);
}

/* @provenance RVA 0x5dc4 sub_5DC4 */
size_t rkos_strnlen(const char *s, size_t maxlen)
{
    size_t idx = 0u;
    while (idx < maxlen) {
        if (s[idx] == '\0') {
            return idx;
        }
        idx++;
    }
    return maxlen;
}

/* @provenance RVA 0x5d92 sub_5D92 */
size_t rkos_strlcpy(char *dst, const char *src, size_t dstsize)
{
    if (dstsize == 0u) {
        return rkos_strlen(src);
    }
    size_t limit = dstsize - 1u;
    size_t copied = 0u;
    while (copied < limit && src[copied] != '\0') {
        dst[copied] = src[copied];
        copied++;
    }
    dst[copied] = '\0';
    return copied + rkos_strlen(src + copied);
}

/* @provenance RVA 0x5d4c sub_5D4C */
char *rkos_stpncpy(char *dst, const char *src, size_t n)
{
    size_t slen = rkos_strnlen(src, n);
    if (n > slen) {
        rkos_memcpy(dst, src, slen);
        rkos_memset(dst + slen, 0, n - slen);
        return dst + slen;
    }
    rkos_memcpy(dst, src, n);
    return dst + n;
}

#ifndef RKOS_HOST
/* Standard freestanding compiler-support symbols for ARMv7E-M */
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

RKOS_EARLY_TEXT void *memcpy(void *dst, const void *src, size_t n)
{
    return rkos_memcpy(dst, src, n);
}
RKOS_EARLY_TEXT void *memmove(void *dst, const void *src, size_t n)
{
    return rkos_memcpy(dst, src, n);
}
RKOS_EARLY_TEXT void *memset(void *dst, int c, size_t n)
{
    return rkos_memset(dst, c, n);
}
RKOS_EARLY_TEXT int memcmp(const void *s1, const void *s2, size_t n)
{
    return rkos_memcmp(s1, s2, n);
}
RKOS_EARLY_TEXT size_t strlen(const char *s)
{
    return rkos_strlen(s);
}
RKOS_EARLY_TEXT int strcmp(const char *s1, const char *s2)
{
    return rkos_strcmp(s1, s2);
}
RKOS_EARLY_TEXT int strncmp(const char *s1, const char *s2, size_t n)
{
    return rkos_strncmp(s1, s2, n);
}
#endif
