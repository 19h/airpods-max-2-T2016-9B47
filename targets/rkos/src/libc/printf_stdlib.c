#include "rkos/rkos.h"

/*
 * Reconstructed CRC-32 (`0x5568`), `strtoul`/`strtoull` (`0x53F0`, `0x54BC`,
 * `0x5C10`), freestanding `printf`/`snprintf`/`vsnprintf`/`vscnprintf`
 * (`0x5592..0x5C0E`), and console mux output (`0x5DD8`).
 */

#ifdef RKOS_HOST
uint32_t rkos_host_invoke_mux_write(uint32_t fn_va, uint32_t dev_va, uint32_t mode,
                                    const uint8_t *buf, uint32_t *io_len);
#endif

static const char g_digits_lower[] = "0123456789abcdefghijklmnopqrstuvwxyz";
static const char g_digits_upper[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static inline uint8_t rkos_ascii_tolower(uint8_t ch)
{
    if ((uint32_t)(ch - 0x41u) < 26u) {
        return (uint8_t)(ch + 0x20u);
    }
    return ch;
}

/* @provenance RVA 0x5568 sub_5568 */
uint32_t rkos_crc32(const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    const uint32_t *table = (const uint32_t *)RKOS_VMA_RAW(RKOS_CRC32_TABLE_RVA, 1024u);
    uint32_t crc = 0xFFFFFFFFu;

    while (len > 0u) {
        uint8_t byte_val = *p++;
        len--;
        crc = table[(byte_val ^ (crc & 0xFFu))] ^ (crc >> 8);
    }
    return ~crc;
}

/* @provenance RVA 0x54bc sub_54BC */
const uint8_t *rkos_strtol_parse_base(const uint8_t *s, uint8_t *sign_out, uint32_t *base_inout)
{
    uint8_t ch;
    for (;;) {
        ch = *s;
        if ((uint32_t)(ch - 9u) >= 5u && ch != 0x20u) {
            break;
        }
        s++;
    }

    if (ch == (uint8_t)'+' || ch == (uint8_t)'-') {
        s++;
    } else {
        ch = (uint8_t)'+';
    }
    *sign_out = ch;

    const uint8_t *start = s;
    if (*s != (uint8_t)'0') {
        if (*base_inout == 0u) {
            *base_inout = 10u;
        }
    } else if ((*base_inout & ~0x10u) == 0u) {
        if ((s[1] | 0x20u) == (uint8_t)'x') {
            *base_inout = 16u;
            uint8_t hex_ch = rkos_ascii_tolower(s[2]);
            start = (rkos_memchr(g_digits_lower, hex_ch, 16u) != NULL) ? (s + 2) : s;
        } else if (*base_inout == 0u) {
            *base_inout = 8u;
            uint8_t oct_ch = rkos_ascii_tolower(s[1]);
            if (rkos_memchr(g_digits_lower, oct_ch, 8u) != NULL) {
                start = s + 1;
            } else {
                *base_inout = 10u;
                start = s;
            }
        }
    }

    if ((*base_inout - 2u) >= 35u) {
        return NULL;
    }
    return start;
}

/* @provenance RVA 0x53f0 sub_53F0 */
uint64_t rkos_strtoull_scanner(const uint8_t **cursor, uint32_t base,
                               uint32_t max_lo, uint32_t max_hi,
                               uint64_t cutoff, uint32_t cutlim, uint8_t *sign_io)
{
    const uint8_t *p = *cursor;
    uint64_t acc = 0ULL;
    int32_t last_digit = -1;

    for (;;) {
        uint8_t ch = rkos_ascii_tolower(*p);
        const char *hit = (const char *)rkos_memchr(g_digits_lower, ch, base);
        if (hit == NULL) {
            break;
        }
        int32_t digit = (int32_t)(hit - g_digits_lower);
        last_digit = digit;

        if (acc > cutoff || (acc == cutoff && digit > (int32_t)cutlim)) {
            uint8_t *errno_flag = (uint8_t *)RKOS_VMA_RAW(0x2001DCA4u, 1u);
            *errno_flag = 1u;
            for (;;) {
                p = *cursor;
                uint8_t rem_ch = rkos_ascii_tolower(*p);
                if (rkos_memchr(g_digits_lower, rem_ch, base) == NULL) {
                    break;
                }
                *cursor = p + 1;
            }
            *sign_io = (uint8_t)'+';
            return ((uint64_t)max_hi << 32) | (uint64_t)max_lo;
        }

        acc = acc * (uint64_t)base + (uint64_t)(uint32_t)digit;
        p = *cursor + 1;
        *cursor = p;
    }

    if (last_digit == -1) {
        *cursor = NULL;
        return 0ULL;
    }
    return acc;
}

/* @provenance RVA 0x5c10 sub_5C10 */
uint32_t rkos_strtoul(const char *nptr, char **endptr, uint32_t base)
{
    const uint8_t *orig = (const uint8_t *)nptr;
    uint32_t b = base;
    uint8_t sign_ch = (uint8_t)'+';
    const uint8_t *scan = rkos_strtol_parse_base(orig, &sign_ch, &b);

    if (b < 2u || b > 36u || scan == NULL) {
        if (endptr != NULL) {
            *endptr = (char *)(uintptr_t)orig;
        }
        return 0u;
    }

    uint32_t cutoff = 0xFFFFFFFFu / b;
    uint32_t cutlim = 0xFFFFFFFFu - (cutoff * b);
    uint64_t val64 = rkos_strtoull_scanner(&scan, b, 0xFFFFFFFFu, 0u,
                                           (uint64_t)cutoff, cutlim, &sign_ch);
    uint32_t val = (uint32_t)val64;

    if (endptr != NULL) {
        *endptr = (char *)(uintptr_t)((scan != NULL) ? scan : orig);
    }
    if (sign_ch != (uint8_t)'+') {
        return (uint32_t)(0u - val);
    }
    return val;
}

/* @provenance RVA 0x5ae8 sub_5AE8 */
char *rkos_format_i64_dec(char buf[32], int pad_arg, uint64_t val, int is_signed, int force_plus)
{
    (void)pad_arg;
    int negative = (is_signed != 0 && ((int64_t)val < 0));
    uint64_t mag = negative ? (0ULL - val) : val;

    buf[31] = '\0';
    int pos = 30;
    while (mag >= 10ULL && pos > 1) {
        uint64_t rem = 0ULL;
        mag = rkos_udivmoddi4(mag, 10ULL, &rem);
        buf[pos--] = (char)('0' + (int)rem);
    }
    buf[pos] = (char)('0' + (int)mag);

    if (negative) {
        buf[--pos] = '-';
    } else if (force_plus != 0) {
        buf[--pos] = '+';
    }
    return &buf[pos];
}

/* @provenance RVA 0x5bd0 sub_5BD0 */
rkos_printf_sink_t *rkos_snprintf_putc(rkos_printf_sink_t *sink, int ch)
{
    char *buf = sink->buf;
    if (buf != NULL) {
        uint32_t pos = (uint32_t)sink->pos;
        uint32_t cap_minus_one = (uint32_t)sink->cap - 1u;
        if (pos < cap_minus_one) {
            if (ch != 0) {
                buf[pos + 1u] = '\0';
                buf = sink->buf;
                pos = (uint32_t)sink->pos;
            }
            sink->pos = (size_t)(pos + 1u);
            buf[pos] = (char)ch;
        }
    }
    return sink;
}

/* @provenance RVA 0x5dd8 sub_5DD8 */
uint32_t rkos_console_mux_putc(uint32_t mux_va, uint32_t ch)
{
    uint32_t all_ok = 1u;
    uint8_t byte_buf = (uint8_t)ch;

    for (uint32_t idx = 0u; idx < *RKOS_VMA_PTR(const uint32_t *, mux_va + 8u); idx++) {
        uint32_t written = 1u;
        uint32_t status = 0u;
        do {
            uint32_t devs_va = *RKOS_VMA_PTR(const uint32_t *, mux_va + 4u);
            uint32_t dev_va = *RKOS_VMA_PTR(const uint32_t *, devs_va + 4u * idx);
            uint32_t vtbl_va = *RKOS_VMA_PTR(const uint32_t *, dev_va);
            uint32_t fn_va = *RKOS_VMA_PTR(const uint32_t *, vtbl_va + 4u);
            written = 1u;
#ifdef RKOS_HOST
            status = rkos_host_invoke_mux_write(fn_va, dev_va, 2u, &byte_buf, &written);
#else
            status = rkos_call_fn4(vtbl_va, fn_va, dev_va, 2u,
                                   (uint32_t)(uintptr_t)&byte_buf,
                                   (uint32_t)(uintptr_t)&written);
#endif
        } while ((written | status) == 0u);

        all_ok = (all_ok & 1u) & ((status == 0u) ? 1u : 0u);
    }

    return ((all_ok & 1u) != 0u) ? ch : 0xFFFFFFFFu;
}

/* @provenance RVA 0x55d4 sub_55D4 */
rkos_printf_sink_t *rkos_console_putc(rkos_printf_sink_t *sink, int ch)
{
    if (ch != 0) {
        uint32_t rc = rkos_console_mux_putc(0x20000718u, (uint32_t)(uint8_t)ch);
        return (rkos_printf_sink_t *)(uintptr_t)rc;
    }
    return sink;
}

static inline void rkos_emit_char(rkos_printf_sink_t *sink, uint8_t ch, int *total)
{
    (void)sink->putc_fn(sink, (int)ch);
    (*total)++;
}

/* @provenance RVA 0x55e0 sub_55E0 */
int rkos_vprintf_core(rkos_printf_sink_t *sink, const char *fmt, va_list ap)
{
    int total = 0;
    const uint8_t *cur = (const uint8_t *)fmt;

    while (*cur != 0u) {
        if (*cur != (uint8_t)'%') {
            rkos_emit_char(sink, *cur++, &total);
            continue;
        }
        cur++;

        uint32_t flags = 0u;
        int vals[2] = { 0, 0 };
        int in_prec = 0;
        int done_spec = 0;

        while (!done_spec) {
            uint8_t spec = *cur++;
            switch (spec) {
            case '#':
                flags |= 0x20u;
                continue;
            case '+':
                flags |= 0x80u;
                continue;
            case '-':
                flags = (flags & ~0x500u) + 0x400u;
                continue;
            case '.':
                in_prec = 1;
                continue;
            case '0':
                if (vals[0] == 0) {
                    flags |= 0x100u;
                    continue;
                }
                vals[in_prec] = vals[in_prec] * 10;
                continue;
            case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                vals[in_prec] = vals[in_prec] * 10 + (int)(spec - (uint8_t)'0');
                continue;
            case '*':
                vals[in_prec] = va_arg(ap, int);
                continue;
            case 'h':
                flags |= ((flags & 4u) << 1) | 4u;
                continue;
            case 'l':
                flags |= ((flags & 1u) << 1) | 1u;
                continue;
            case 'z':
                flags |= 0x10u;
                continue;
            case '%':
                rkos_emit_char(sink, (uint8_t)'%', &total);
                done_spec = 1;
                break;
            case 'c': {
                uint8_t ch_val = (uint8_t)va_arg(ap, int);
                rkos_emit_char(sink, ch_val, &total);
                done_spec = 1;
                break;
            }
            case 'n':
                done_spec = 1;
                break;
            case 'A': case 'E': case 'F': case 'G':
            case 'a': case 'e': case 'f': case 'g': {
                (void)va_arg(ap, double);
                char flt_buf[4] = { '@', '\0', '\0', '\0' };
                int eff_width = vals[1];
                if (vals[0] > eff_width) {
                    eff_width = vals[0];
                }
                vals[0] = eff_width;
                vals[1] = 0;
                uint32_t eff_flags = flags & ~0x100u;
                const char *text = flt_buf;
                int slen = 1;
                if (eff_width > slen && (eff_flags & 0x400u) == 0u) {
                    for (int pad = eff_width - slen; pad >= 1; pad--) {
                        rkos_emit_char(sink, (uint8_t)' ', &total);
                    }
                }
                rkos_emit_char(sink, (uint8_t)text[0], &total);
                if (eff_width > slen && (eff_flags & 0x400u) != 0u) {
                    for (int pad = eff_width - slen; pad >= 1; pad--) {
                        rkos_emit_char(sink, (uint8_t)' ', &total);
                    }
                }
                done_spec = 1;
                break;
            }
            case 's':
            case 'd': case 'i':
            case 'u':
            case 'x': case 'X': case 'p': {
                char num_buf[32];
                const char *digits = NULL;
                const char *prefix = NULL;
                int has_prefix = 0;
                uint32_t eff_flags = flags;

                if (spec == (uint8_t)'s') {
                    const char *s_arg = va_arg(ap, const char *);
                    digits = (s_arg != NULL) ? s_arg : "<null>";
                } else if (spec == (uint8_t)'d' || spec == (uint8_t)'i') {
                    int64_t sval;
                    if ((flags & 2u) != 0u) {
                        sval = va_arg(ap, int64_t);
                    } else if ((flags & 1u) != 0u) {
                        sval = (int64_t)va_arg(ap, long);
                    } else if ((flags & 8u) != 0u) {
                        sval = (int64_t)(int8_t)va_arg(ap, int);
                    } else if ((flags & 4u) != 0u) {
                        sval = (int64_t)(int16_t)va_arg(ap, int);
                    } else if ((flags & 0x10u) != 0u) {
                        sval = (int64_t)(uint32_t)va_arg(ap, size_t);
                    } else {
                        sval = (int64_t)va_arg(ap, int);
                    }
                    digits = rkos_format_i64_dec(num_buf, 0, (uint64_t)sval, 1,
                                                 (int)(flags & 0x80u));
                    eff_flags = flags | 0x800u;
                } else if (spec == (uint8_t)'u') {
                    uint64_t uval;
                    if ((flags & 2u) != 0u) {
                        uval = va_arg(ap, uint64_t);
                    } else if ((flags & 1u) != 0u) {
                        uval = (uint64_t)va_arg(ap, unsigned long);
                    } else if ((flags & 8u) != 0u) {
                        uval = (uint64_t)(uint8_t)va_arg(ap, unsigned int);
                    } else if ((flags & 4u) != 0u) {
                        uval = (uint64_t)(uint16_t)va_arg(ap, unsigned int);
                    } else if ((flags & 0x10u) != 0u) {
                        uval = (uint64_t)va_arg(ap, size_t);
                    } else {
                        uval = (uint64_t)va_arg(ap, unsigned int);
                    }
                    digits = rkos_format_i64_dec(num_buf, 0, uval, 0,
                                                 (int)(flags & 0x80u));
                    eff_flags = flags | 0x800u;
                } else {
                    if (spec == (uint8_t)'X') {
                        flags |= 0x40u;
                    } else if (spec == (uint8_t)'p') {
                        flags |= 0x21u;
                    }
                    uint64_t xval;
                    if (spec == (uint8_t)'p') {
                        xval = (uint64_t)(uintptr_t)va_arg(ap, void *);
                    } else if ((flags & 2u) != 0u) {
                        xval = va_arg(ap, uint64_t);
                    } else if ((flags & 1u) != 0u) {
                        xval = (uint64_t)va_arg(ap, unsigned long);
                    } else if ((flags & 8u) != 0u) {
                        xval = (uint64_t)(uint8_t)va_arg(ap, unsigned int);
                    } else if ((flags & 4u) != 0u) {
                        xval = (uint64_t)(uint16_t)va_arg(ap, unsigned int);
                    } else {
                        xval = (uint64_t)va_arg(ap, unsigned int);
                    }
                    eff_flags = flags | 0x800u;
                    const char *alphabet = ((flags & 0x40u) != 0u) ? g_digits_upper : g_digits_lower;
                    num_buf[31] = '\0';
                    int pos = 30;
                    do {
                        num_buf[pos--] = alphabet[xval & 0xFu];
                        xval >>= 4;
                    } while (xval != 0ULL);
                    digits = &num_buf[pos + 1];
                    if ((flags & 0x20u) != 0u) {
                        prefix = "0x";
                        has_prefix = 1;
                    }
                }

                int width_rem = vals[0];
                int dlen = (int)rkos_strlen(digits);
                if (width_rem < 1) {
                    if (width_rem == 0) {
                        width_rem = 0x7FFFFFFF;
                        eff_flags = 0u;
                    }
                } else if (width_rem < dlen) {
                    width_rem = 0x7FFFFFFF;
                    eff_flags = 0u;
                } else if ((eff_flags & 0x400u) == 0u) {
                    uint8_t pad_ch = ((eff_flags & 0x100u) != 0u) ? (uint8_t)'0' : (uint8_t)' ';
                    int full_len = dlen + (has_prefix ? (int)rkos_strlen(prefix) : 0);
                    if (width_rem > full_len) {
                        int rem_after_pad = full_len;
                        if ((~eff_flags & 0x900u) == 0u &&
                            (digits[0] == '-' || digits[0] == '+')) {
                            rkos_emit_char(sink, (uint8_t)digits[0], &total);
                            digits++;
                            rem_after_pad = full_len - 1;
                        }
                        if (has_prefix && (eff_flags & 0x100u) != 0u) {
                            while (*prefix != '\0') {
                                rkos_emit_char(sink, (uint8_t)*prefix++, &total);
                            }
                            prefix = NULL;
                        }
                        for (int pad = width_rem - full_len; pad >= 1; pad--) {
                            rkos_emit_char(sink, pad_ch, &total);
                        }
                        width_rem = rem_after_pad;
                    }
                }

                if (prefix != NULL) {
                    while (*prefix != '\0') {
                        rkos_emit_char(sink, (uint8_t)*prefix++, &total);
                        width_rem--;
                    }
                }
                while (*digits != '\0') {
                    rkos_emit_char(sink, (uint8_t)*digits++, &total);
                    width_rem--;
                }
                if ((eff_flags & 0x400u) != 0u) {
                    while (width_rem-- >= 1) {
                        rkos_emit_char(sink, (uint8_t)' ', &total);
                    }
                }
                done_spec = 1;
                break;
            }
            default:
                if (spec == 0u) {
                    return total;
                }
                rkos_emit_char(sink, (uint8_t)'%', &total);
                rkos_emit_char(sink, spec, &total);
                done_spec = 1;
                break;
            }
        }
    }

    return total;
}

/* @provenance RVA 0x5b98 sub_5B98 */
int rkos_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    char dummy = '\0';
    rkos_printf_sink_t sink;
    sink.putc_fn = rkos_snprintf_putc;
    sink.cap = (size != 0u) ? size : 1u;
    sink.buf = (size != 0u && buf != NULL) ? buf : &dummy;
    sink.pos = 0u;
    sink.flags = 0u;
    if (sink.buf != NULL) {
        sink.buf[0] = '\0';
    }
    return rkos_vprintf_core(&sink, fmt, ap);
}

/* @provenance RVA 0x5bf8 sub_5BF8 */
size_t rkos_vscnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    int n = rkos_vsnprintf(buf, size, fmt, ap);
    int written = (n < 0) ? 0 : n;
    int cap = (int)size - 1;
    return (size_t)(uint32_t)((written < cap) ? written : cap);
}

/* @provenance RVA 0x5b7e sub_5B7E */
int rkos_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = rkos_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

/* @provenance RVA 0x55b0 sub_55B0 */
int rkos_vprintf(const char *fmt, va_list ap)
{
    rkos_printf_sink_t sink;
    sink.putc_fn = rkos_console_putc;
    sink.cap = 0u;
    sink.buf = NULL;
    sink.pos = 0u;
    sink.flags = 0u;
    return rkos_vprintf_core(&sink, fmt, ap);
}

/* @provenance RVA 0x5592 sub_5592 */
int rkos_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = rkos_vprintf(fmt, ap);
    va_end(ap);
    return ret;
}
