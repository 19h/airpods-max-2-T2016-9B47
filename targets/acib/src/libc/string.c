#include "acib/libc.h"
#include <stdint.h>

/*
 * Freestanding C11 memory, string, and 64-bit shift helpers reconstructed
 * from acib ROM (0x0e4c58 .. 0x0e53fe).
 */

/* @provenance RVA 0x0e4c58 sub_E4C58 */
void *acib_memchr(const void *s, int c, size_t n)
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

/* @provenance RVA 0x0e4c7c sub_E4C7C */
int acib_memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *lhs = (const uint8_t *)s1;
    const uint8_t *rhs = (const uint8_t *)s2;

    if (lhs == rhs) {
        return 0;
    }

    if (n >= 4u) {
        while (((uintptr_t)lhs & 3u) != 0u) {
            int diff = (int)*lhs - (int)*rhs;
            lhs++;
            rhs++;
            n--;
            if (diff != 0) {
                return diff;
            }
        }
        while (n >= 4u) {
            uint32_t wl = ((uint32_t)lhs[0] << 24) | ((uint32_t)lhs[1] << 16) |
                          ((uint32_t)lhs[2] << 8)  | (uint32_t)lhs[3];
            uint32_t wr = ((uint32_t)rhs[0] << 24) | ((uint32_t)rhs[1] << 16) |
                          ((uint32_t)rhs[2] << 8)  | (uint32_t)rhs[3];
            if (wl != wr) {
                return (wl < wr) ? -1 : 1;
            }
            lhs += 4u;
            rhs += 4u;
            n -= 4u;
        }
    }

    while (n > 0u) {
        if (*lhs != *rhs) {
            return (int)*lhs - (int)*rhs;
        }
        lhs++;
        rhs++;
        n--;
    }
    return 0;
}

/* @provenance RVA 0x0e4da8 memcpy_wrapper */
void *memcpy_wrapper(void *dst, const void *src, size_t n)
{
    return memmove(dst, src, n);
}

/* @provenance RVA 0x0e4db0 memmove */
void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || n == 0u) {
        return dst;
    }
    if ((uintptr_t)d < (uintptr_t)s) {
        for (size_t i = 0u; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0u; i--) {
            d[i - 1u] = s[i - 1u];
        }
    }
    return dst;
}

/* @provenance RVA 0x0e4ec0 memset */
void *memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    uint8_t fill = (uint8_t)(c & 0xFF);

    for (size_t i = 0u; i < n; i++) {
        d[i] = fill;
    }
    return dst;
}

/* @provenance RVA 0x0e4f40 sub_E4F40 */
size_t acib_strlen(const char *s)
{
    const char *p = s;
    while (*p != '\0') {
        p++;
    }
    return (size_t)(p - s);
}

/* @provenance RVA 0x0e4fc8 sub_E4FC8 */
int acib_strncmp(const char *s1, const char *s2, size_t n)
{
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;

    if (n == 0u || a == b) {
        return 0;
    }
    while (n > 0u) {
        uint8_t ca = *a++;
        uint8_t cb = *b++;
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
        if (ca == 0u) {
            return 0;
        }
        n--;
    }
    return 0;
}

/* @provenance RVA 0x0e5094 sub_E5094 */
uint64_t acib_aeabi_llsl(uint64_t val, uint32_t shift)
{
    uint32_t lo = (uint32_t)(val & 0xFFFFFFFFu);
    uint32_t hi = (uint32_t)(val >> 32);
    uint32_t amt = shift & 0xFFu;

    if (amt >= 64u) {
        return 0u;
    }
    if (amt >= 32u) {
        hi = lo << (amt - 32u);
        lo = 0u;
    } else if (amt != 0u) {
        hi = (hi << amt) | (lo >> (32u - amt));
        lo = lo << amt;
    }
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

/* @provenance RVA 0x0e53dc sub_E53DC */
uint64_t acib_aeabi_llsr(uint64_t val, uint32_t shift)
{
    uint32_t lo = (uint32_t)(val & 0xFFFFFFFFu);
    uint32_t hi = (uint32_t)(val >> 32);
    uint32_t amt = shift & 0xFFu;

    if (amt >= 64u) {
        return 0u;
    }
    if (amt >= 32u) {
        lo = hi >> (amt - 32u);
        hi = 0u;
    } else if (amt != 0u) {
        lo = (lo >> amt) | (hi << (32u - amt));
        hi = hi >> amt;
    }
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

#if !defined(ACIB_HOST_TEST)
void *memcpy(void *dst, const void *src, size_t n);
void *memcpy(void *dst, const void *src, size_t n)
{
    return memmove(dst, src, n);
}
#endif
