#include "rkof/cpu.h"
#include "rkof/libc.h"

extern int sub_18726C(void *sink_ctx, const char *fmt, va_list ap);

/* @provenance RVA 0xca68 sub_CA68 */
void *sub_CA68(const void *buf, int ch, size_t len)
{
    if (len == 0u) {
        return NULL;
    }

    const uint8_t *cur = (const uint8_t *)buf;
    uint8_t needle = (uint8_t)ch;
    while (len != 0u) {
        if (*cur == needle) {
            return (void *)(uintptr_t)cur;
        }
        cur++;
        len--;
    }
    return NULL;
}

static inline uint32_t load_le32_bytes(const uint8_t *ptr)
{
    return (uint32_t)ptr[0]
        | ((uint32_t)ptr[1] << 8u)
        | ((uint32_t)ptr[2] << 16u)
        | ((uint32_t)ptr[3] << 24u);
}

/* @provenance RVA 0xca8c memcmp */
int rkof_memcmp(const void *lhs, const void *rhs, size_t len)
{
    if (lhs == rhs) {
        return 0;
    }

    const uint8_t *pl = (const uint8_t *)lhs;
    const uint8_t *pr = (const uint8_t *)rhs;

    if (len >= 4u) {
        while (((uintptr_t)pl & 3u) != 0u) {
            int diff = (int)pl[0] - (int)pr[0];
            pl++;
            pr++;
            len--;
            if (diff != 0) {
                return diff;
            }
        }

        while (len >= 4u) {
            uint32_t wl = load_le32_bytes(pl);
            uint32_t wr = load_le32_bytes(pr);
            pl += 4u;
            pr += 4u;
            len -= 4u;
            if (wl != wr) {
                uint32_t bel = cpu_bswap32(wl);
                uint32_t ber = cpu_bswap32(wr);
                return (bel < ber) ? -1 : 1;
            }
        }
    }

    while (len != 0u) {
        int diff = (int)pl[0] - (int)pr[0];
        pl++;
        pr++;
        len--;
        if (diff != 0) {
            return diff;
        }
    }
    return 0;
}

/* @provenance RVA 0xcbc0 memcpy */
void *rkof_memcpy(void *dst, const void *src, size_t len)
{
    uintptr_t delta = (uintptr_t)dst - (uintptr_t)src;
    if (delta == 0u || len == 0u) {
        return dst;
    }

    uint8_t *out = (uint8_t *)dst;
    const uint8_t *in = (const uint8_t *)src;
    if (delta < len) {
        size_t idx = len;
        while (idx > 0u) {
            idx--;
            out[idx] = in[idx];
        }
    } else {
        for (size_t idx = 0u; idx < len; idx++) {
            out[idx] = in[idx];
        }
    }
    return dst;
}

/* @provenance RVA 0xcbb8 memcpy_thunk */
void *memcpy_thunk(void *dst, const void *src, size_t len)
{
    return rkof_memcpy(dst, src, len);
}

/* @provenance RVA 0x301b0 j_memcpy */
void *j_memcpy(void *dst, const void *src, size_t len)
{
    return rkof_memcpy(dst, src, len);
}

/* @provenance RVA 0xccd0 memset */
void *rkof_memset(void *dst, int byte_val, size_t len)
{
    uint8_t *out = (uint8_t *)dst;
    uint8_t fill = (uint8_t)byte_val;
    for (size_t idx = 0u; idx < len; idx++) {
        out[idx] = fill;
    }
    return dst;
}

/* @provenance RVA 0xcd48 sub_CD48 */
char *sub_CD48(const char *str, int ch)
{
    uint8_t needle = (uint8_t)ch;
    const uint8_t *cur = (const uint8_t *)str;
    for (;;) {
        uint8_t cur_byte = *cur;
        if (cur_byte == needle) {
            return (char *)(uintptr_t)cur;
        }
        if (cur_byte == 0u) {
            return NULL;
        }
        cur++;
    }
}

/* @provenance RVA 0xcd78 strcmp */
int rkof_strcmp(const char *lhs, const char *rhs)
{
    const uint8_t *pl = (const uint8_t *)lhs;
    const uint8_t *pr = (const uint8_t *)rhs;
    for (;;) {
        uint8_t cl = *pl++;
        uint8_t cr = *pr++;
        if (cl != cr || cl == 0u) {
            return (int)cl - (int)cr;
        }
    }
}

/* @provenance RVA 0xce20 sub_CE20 */
size_t sub_CE20(const char *str)
{
    const uint8_t *cur = (const uint8_t *)str;
    size_t len = 0u;
    while (cur[len] != 0u) {
        len++;
    }
    return len;
}

/* @provenance RVA 0xcea8 strncmp */
int rkof_strncmp(const char *lhs, const char *rhs, size_t max_len)
{
    const uint8_t *pl = (const uint8_t *)lhs;
    const uint8_t *pr = (const uint8_t *)rhs;
    while (max_len != 0u) {
        uint8_t cl = *pl++;
        uint8_t cr = *pr++;
        max_len--;
        if (cl != cr || cl == 0u) {
            return (int)cl - (int)cr;
        }
    }
    return 0;
}

/* @provenance RVA 0x187d7e rtkit_mem_is_filled */
uint32_t rtkit_mem_is_filled(const void *buf, uint32_t byte_val, size_t len)
{
    uint8_t expected = (uint8_t)byte_val;
    uint32_t word_pat = 0u;
    if (byte_val != 0u) {
        for (uint32_t idx = 0u; idx < 4u; idx++) {
            word_pat = (word_pat << 8u) | byte_val;
        }
    }

    const uint8_t *cur = (const uint8_t *)buf;
    while (len != 0u && (((uintptr_t)cur & 3u) != 0u)) {
        if ((uint32_t)(*cur) != byte_val) {
            return 0u;
        }
        cur++;
        len--;
    }

    while (len > 3u) {
        const uint32_t *wptr = (const uint32_t *)(const void *)cur;
        if (*wptr != word_pat) {
            return 0u;
        }
        cur += 4u;
        len -= 4u;
    }

    for (size_t idx = 0u; idx < len; idx++) {
        if ((uint32_t)cur[idx] != (uint32_t)expected && (uint32_t)cur[idx] != byte_val) {
            return 0u;
        }
        if ((uint32_t)cur[idx] != byte_val) {
            return 0u;
        }
    }
    return 1u;
}

struct rkof_fmt_buf_sink {
    uintptr_t putc_fn;
    size_t    cap;
    char     *dst;
    size_t    written;
    uint32_t  flags;
};

/* @provenance RVA 0x187818 vsnprintf */
int rkof_vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap)
{
    char scratch[2] = { 0, 0 };
    struct rkof_fmt_buf_sink sink;
    sink.putc_fn = 0x00187851u;
    sink.dst = (cap != 0u) ? buf : scratch;
    sink.cap = (cap != 0u) ? cap : 2u;
    sink.written = 0u;
    sink.flags = 0u;

    if (sink.dst != NULL) {
        sink.dst[0] = '\0';
    }
    return sub_18726C(&sink, fmt, ap);
}

/* @provenance RVA 0x1877fe snprintf */
int rkof_snprintf(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int res = rkof_vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return res;
}
