#include "rkos/rkos.h"

/*
 * Reconstructed CRC-32 (`0x5568`), `strtoul`/`strtoull` (`0x53F0`, `0x54BC`,
 * `0x5C10`), and freestanding `printf`/`snprintf`/`vsnprintf`/`vscnprintf`
 * (`0x5592..0x5C0E`).
 */

static const char g_digits_lower[] = "0123456789abcdefghijklmnopqrstuvwxyz";
static const char g_digits_upper[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static uint8_t rkos_ascii_tolower(uint8_t ch)
{
    if (ch >= (uint8_t)'A' && ch <= (uint8_t)'Z') {
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
        uint8_t b = *p++;
        len--;
        crc = table[(b ^ (crc & 0xFFu))] ^ (crc >> 8);
    }
    return ~crc;
}

/* @shim RVA 0x54bc sub_54BC (pending_c: not shown equivalent to raw) */
const uint8_t *rkos_strtol_parse_base(const uint8_t *s, uint8_t *sign_out, uint32_t *base_inout)
{
    while ((*s >= 9u && *s <= 13u) || *s == (uint8_t)' ') {
        s++;
    }

    uint8_t sign_ch = *s;
    if (sign_ch == (uint8_t)'+' || sign_ch == (uint8_t)'-') {
        s++;
    } else {
        sign_ch = (uint8_t)'+';
    }
    *sign_out = sign_ch;

    uint32_t base = *base_inout;
    const uint8_t *start = s;

    if (*s == (uint8_t)'0') {
        if ((base & ~0x10u) == 0u && ((s[1] | 0x20u) == (uint8_t)'x')) {
            uint8_t d = rkos_ascii_tolower(s[2]);
            if (rkos_memchr(g_digits_lower, d, 16u) != NULL) {
                *base_inout = 16u;
                start = s + 2;
            } else {
                *base_inout = 16u;
                start = s;
            }
        } else if (base == 0u) {
            uint8_t d = rkos_ascii_tolower(s[1]);
            if (rkos_memchr(g_digits_lower, d, 8u) != NULL) {
                *base_inout = 8u;
                start = s + 1;
            } else {
                *base_inout = 10u;
                start = s;
            }
        }
    } else if (base == 0u) {
        *base_inout = 10u;
    }

    if ((*base_inout - 2u) >= 35u) {
        return NULL;
    }
    return start;
}

/* @shim RVA 0x53f0 sub_53F0 (pending_c: not shown equivalent to raw) */
uint64_t rkos_strtoull_scanner(const uint8_t **cursor, uint32_t base,
                               uint32_t max_lo, uint32_t max_hi,
                               uint64_t cutoff, uint32_t cutlim, uint8_t *sign_io)
{
    const uint8_t *p = *cursor;
    if (p == NULL) {
        return 0ULL;
    }

    uint64_t acc = 0ULL;
    int any_digit = 0;

    for (;;) {
        uint8_t ch = rkos_ascii_tolower(*p);
        const char *hit = (const char *)rkos_memchr(g_digits_lower, ch, base);
        if (hit == NULL) {
            break;
        }
        uint32_t digit = (uint32_t)(hit - g_digits_lower);
        any_digit = 1;

        if (acc > cutoff || (acc == cutoff && digit > cutlim)) {
            uint8_t *errno_flag = (uint8_t *)RKOS_VMA_RAW(0x2001DCA4u, 1u);
            *errno_flag = 1u;
            while (rkos_memchr(g_digits_lower, rkos_ascii_tolower(*p), base) != NULL) {
                p++;
            }
            *cursor = p;
            *sign_io = (uint8_t)'+';
            return ((uint64_t)max_hi << 32) | (uint64_t)max_lo;
        }

        acc = acc * (uint64_t)base + (uint64_t)digit;
        p++;
        *cursor = p;
    }

    if (!any_digit) {
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

/* @shim RVA 0x5bd0 sub_5BD0 (pending_c: not shown equivalent to raw) */
void rkos_snprintf_putc(rkos_printf_sink_t *sink, int ch)
{
    if (sink->buf != NULL && sink->cap > 0u) {
        if (sink->pos < sink->cap - 1u) {
            sink->buf[sink->pos] = (char)ch;
            if (ch != 0) {
                sink->buf[sink->pos + 1u] = '\0';
            }
            sink->pos++;
        }
    }
}

static void rkos_sink_emit(rkos_printf_sink_t *sink, char ch, int *total)
{
    sink->putc_fn(sink, (unsigned char)ch);
    (*total)++;
}

/* @shim RVA 0x55e0 sub_55E0 (pending_c: not shown equivalent to raw) */
int rkos_vprintf_core(rkos_printf_sink_t *sink, const char *fmt, va_list ap)
{
    int total = 0;

    while (*fmt != '\0') {
        if (*fmt != '%') {
            rkos_sink_emit(sink, *fmt++, &total);
            continue;
        }
        fmt++;
        if (*fmt == '%') {
            rkos_sink_emit(sink, *fmt++, &total);
            continue;
        }

        int left_align = 0;
        int zero_pad = 0;
        int force_plus = 0;
        int alt_form = 0;

        for (;;) {
            if (*fmt == '-') {
                left_align = 1;
                fmt++;
            } else if (*fmt == '0') {
                zero_pad = 1;
                fmt++;
            } else if (*fmt == '+') {
                force_plus = 1;
                fmt++;
            } else if (*fmt == '#') {
                alt_form = 1;
                fmt++;
            } else if (*fmt == ' ') {
                fmt++;
            } else {
                break;
            }
        }
        if (left_align) {
            zero_pad = 0;
        }

        int width = 0;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                left_align = 1;
                zero_pad = 0;
                width = -width;
            }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
        }

        int precision = -1;
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            if (*fmt == '*') {
                precision = va_arg(ap, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    precision = precision * 10 + (*fmt - '0');
                    fmt++;
                }
            }
        }

        int len_mod = 0; /* 0=int, 1=long/size_t, 2=long long */
        if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') {
                len_mod = 2;
                fmt++;
            } else {
                len_mod = 1;
            }
        } else if (*fmt == 'z') {
            len_mod = 1;
            fmt++;
        } else if (*fmt == 'h') {
            fmt++;
            if (*fmt == 'h') {
                fmt++;
            }
        }

        char spec = *fmt;
        if (spec == '\0') {
            break;
        }
        fmt++;

        if (spec == 'c') {
            char ch = (char)va_arg(ap, int);
            if (!left_align) {
                for (int i = 1; i < width; i++) {
                    rkos_sink_emit(sink, ' ', &total);
                }
            }
            rkos_sink_emit(sink, ch, &total);
            if (left_align) {
                for (int i = 1; i < width; i++) {
                    rkos_sink_emit(sink, ' ', &total);
                }
            }
            continue;
        }

        if (spec == 's') {
            const char *str = va_arg(ap, const char *);
            if (str == NULL) {
                str = "(null)";
            }
            size_t slen = (precision >= 0) ? rkos_strnlen(str, (size_t)precision)
                                           : rkos_strlen(str);
            if (!left_align) {
                for (int i = (int)slen; i < width; i++) {
                    rkos_sink_emit(sink, ' ', &total);
                }
            }
            for (size_t i = 0u; i < slen; i++) {
                rkos_sink_emit(sink, str[i], &total);
            }
            if (left_align) {
                for (int i = (int)slen; i < width; i++) {
                    rkos_sink_emit(sink, ' ', &total);
                }
            }
            continue;
        }

        char num_buf[32];
        const char *digits = "";
        const char *prefix = "";

        if (spec == 'd' || spec == 'i') {
            int64_t sval = (len_mod == 2) ? va_arg(ap, int64_t)
                         : (len_mod == 1) ? (int64_t)va_arg(ap, long)
                                          : (int64_t)va_arg(ap, int);
            digits = rkos_format_i64_dec(num_buf, 0, (uint64_t)sval, 1, force_plus);
            if (*digits == '-' || *digits == '+') {
                prefix = (*digits == '-') ? "-" : "+";
                digits++;
            }
        } else if (spec == 'u') {
            uint64_t uval = (len_mod == 2) ? va_arg(ap, uint64_t)
                          : (len_mod == 1) ? (uint64_t)va_arg(ap, unsigned long)
                                           : (uint64_t)va_arg(ap, unsigned int);
            digits = rkos_format_i64_dec(num_buf, 0, uval, 0, 0);
        } else if (spec == 'x' || spec == 'X' || spec == 'p') {
            uint64_t xval;
            if (spec == 'p') {
                uintptr_t pv = (uintptr_t)va_arg(ap, void *);
                xval = (uint64_t)pv;
                prefix = "0x";
            } else {
                xval = (len_mod == 2) ? va_arg(ap, uint64_t)
                     : (len_mod == 1) ? (uint64_t)va_arg(ap, unsigned long)
                                      : (uint64_t)va_arg(ap, unsigned int);
                if (alt_form && xval != 0ULL) {
                    prefix = (spec == 'X') ? "0X" : "0x";
                }
            }
            const char *alphabet = (spec == 'X') ? g_digits_upper : g_digits_lower;
            num_buf[31] = '\0';
            int pos = 31;
            if (xval == 0ULL) {
                num_buf[--pos] = '0';
            } else {
                while (xval != 0ULL && pos > 0) {
                    num_buf[--pos] = alphabet[xval & 0xFu];
                    xval >>= 4;
                }
            }
            digits = &num_buf[pos];
        } else {
            rkos_sink_emit(sink, spec, &total);
            continue;
        }

        int plen = (int)rkos_strlen(prefix);
        int dlen = (int)rkos_strlen(digits);
        int zprec = (precision > dlen) ? (precision - dlen) : 0;
        int full_len = plen + zprec + dlen;
        char pad_ch = (zero_pad && precision < 0) ? '0' : ' ';

        if (!left_align && pad_ch == ' ') {
            for (int i = full_len; i < width; i++) {
                rkos_sink_emit(sink, ' ', &total);
            }
        }
        for (int i = 0; i < plen; i++) {
            rkos_sink_emit(sink, prefix[i], &total);
        }
        if (!left_align && pad_ch == '0') {
            for (int i = full_len; i < width; i++) {
                rkos_sink_emit(sink, '0', &total);
            }
        }
        for (int i = 0; i < zprec; i++) {
            rkos_sink_emit(sink, '0', &total);
        }
        for (int i = 0; i < dlen; i++) {
            rkos_sink_emit(sink, digits[i], &total);
        }
        if (left_align) {
            for (int i = full_len; i < width; i++) {
                rkos_sink_emit(sink, ' ', &total);
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
    /* Raw compares signed: a zero `size` yields cap == -1 (SIZE_MAX). */
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

/* @shim RVA 0x55d4 sub_55D4 (pending_c: not shown equivalent to raw) */
static void rkos_console_putc(rkos_printf_sink_t *sink, int ch)
{
    (void)sink;
    if (ch != 0) {
        RKOS_REG32_WRITE(RKOS_MMIO_UART_BASE + 0x20u, (uint32_t)(uint8_t)ch);
    }
}

/* @shim RVA 0x55b0 sub_55B0 (pending_c: not shown equivalent to raw) */
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

/* @shim RVA 0x5592 sub_5592 (pending_c: not shown equivalent to raw) */
int rkos_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = rkos_vprintf(fmt, ap);
    va_end(ap);
    return ret;
}
