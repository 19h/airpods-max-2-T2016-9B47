/*
 * printf.c — RTKit formatted output engine (rtk_printf, rtk_vprintf,
 * rtk_snprintf, rtk_vsnprintf, rtk_vsnprintf_clamped, rtk_vformat,
 * rtk_i64_to_dec, rtk_call_once) reconstructed from aopf.i64.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "aopf/libc.h"
#include "aopf/rtkit.h"

/* @shim RVA 0x4f4f4 nullsub_5 */
void rtk_vprintf_nop_putc(rtk_fmt_sink_t *sink, int ch)
{
    (void)sink;
    (void)ch;
}

/* @provenance RVA 0x4fae8 sub_4FAE8 */
void rtk_vsnprintf_putc(rtk_fmt_sink_t *sink, int ch)
{
    char *dst = sink->buf_ptr;
    if (dst == NULL) {
        return;
    }
    if (sink->write_pos < sink->buf_size - 1u) {
        if (ch != 0) {
            dst[sink->write_pos + 1u] = '\0';
        }
        dst[sink->write_pos] = (char)ch;
        sink->write_pos += 1u;
    }
}

/* @provenance RVA 0x4fa00 sub_4FA00 */
char *rtk_i64_to_dec(char *buf32, int64_t val, int is_signed, int force_plus)
{
    int non_neg = (is_signed == 0) || (val >= 0);
    uint64_t mag = non_neg ? (uint64_t)val : (0ull - (uint64_t)val);

    size_t pos = 31u;
    buf32[pos] = '\0';

    while (mag >= 10ull) {
        uint64_t quot = rtk_u64_div_u32(mag, 10u);
        uint32_t digit = (uint32_t)(mag - quot * 10ull);
        pos--;
        buf32[pos] = (char)('0' + digit);
        mag = quot;
    }

    pos--;
    buf32[pos] = (char)('0' + (uint32_t)mag);

    if (!non_neg) {
        pos--;
        buf32[pos] = '-';
    } else if (force_plus != 0) {
        pos--;
        buf32[pos] = '+';
    }
    return &buf32[pos];
}

static void fmt_emit_char(rtk_fmt_sink_t *sink, int ch, int *out_count)
{
    if (sink != NULL && sink->putc_fn != NULL) {
        sink->putc_fn(sink, ch);
    }
    (*out_count)++;
}

static size_t fmt_u64_base(char *buf32, uint64_t val, uint32_t base, int upper)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t pos = 31u;
    buf32[pos] = '\0';
    do {
        uint64_t quot = rtk_u64_div_u32(val, base);
        uint32_t rem = (uint32_t)(val - quot * (uint64_t)base);
        pos--;
        buf32[pos] = digits[rem];
        val = quot;
    } while (val != 0ull);
    return pos;
}

/* @shim RVA 0x4f4f8 rtk_vformat */
int rtk_vformat(rtk_fmt_sink_t *sink, const char *fmt, va_list ap)
{
    int total = 0;
    if (fmt == NULL) {
        return 0;
    }

    while (*fmt != '\0') {
        if (*fmt != '%') {
            fmt_emit_char(sink, (uint8_t)*fmt++, &total);
            continue;
        }
        fmt++;
        if (*fmt == '%') {
            fmt_emit_char(sink, '%', &total);
            fmt++;
            continue;
        }

        int left_align = 0;
        int force_plus = 0;
        int zero_pad = 0;
        int alt_form = 0;
        for (;;) {
            if (*fmt == '-') { left_align = 1; fmt++; }
            else if (*fmt == '+') { force_plus = 1; fmt++; }
            else if (*fmt == '0') { zero_pad = 1; fmt++; }
            else if (*fmt == '#') { alt_form = 1; fmt++; }
            else if (*fmt == ' ') { fmt++; }
            else { break; }
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

        int len_mod = 0; /* 1=l, 2=ll, 3=z */
        if (*fmt == 'l') {
            fmt++;
            len_mod = 1;
            if (*fmt == 'l') {
                fmt++;
                len_mod = 2;
            }
        } else if (*fmt == 'z') {
            fmt++;
            len_mod = 3;
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

        char scratch[32];
        const char *piece = "";
        size_t piece_len = 0u;
        const char *prefix = "";
        size_t prefix_len = 0u;

        if (spec == 'c') {
            scratch[0] = (char)va_arg(ap, int);
            piece = scratch;
            piece_len = 1u;
        } else if (spec == 's') {
            piece = va_arg(ap, const char *);
            if (piece == NULL) {
                piece = "<null>";
            }
            piece_len = (precision >= 0) ? rtk_strnlen(piece, (size_t)precision) : aopf_strlen(piece);
            zero_pad = 0;
        } else if (spec == 'd' || spec == 'i') {
            int64_t sval;
            if (len_mod == 2) {
                sval = va_arg(ap, long long);
            } else if (len_mod == 1) {
                sval = va_arg(ap, long);
            } else if (len_mod == 3) {
                sval = (int64_t)va_arg(ap, ptrdiff_t);
            } else {
                sval = va_arg(ap, int);
            }
            char *dec = rtk_i64_to_dec(scratch, sval, 1, force_plus);
            if (*dec == '-' || *dec == '+') {
                prefix = dec;
                prefix_len = 1u;
                dec++;
            }
            piece = dec;
            piece_len = aopf_strlen(dec);
        } else if (spec == 'u' || spec == 'x' || spec == 'X' || spec == 'p') {
            uint64_t uval;
            if (spec == 'p') {
                uval = (uint64_t)(uintptr_t)va_arg(ap, void *);
                prefix = "0x";
                prefix_len = 2u;
            } else if (len_mod == 2) {
                uval = va_arg(ap, unsigned long long);
            } else if (len_mod == 1) {
                uval = va_arg(ap, unsigned long);
            } else if (len_mod == 3) {
                uval = (uint64_t)va_arg(ap, size_t);
            } else {
                uval = va_arg(ap, unsigned int);
            }

            if (spec == 'u') {
                piece = rtk_i64_to_dec(scratch, (int64_t)uval, 0, 0);
                piece_len = aopf_strlen(piece);
            } else {
                int upper = (spec == 'X');
                if (alt_form && spec != 'p') {
                    if (spec == 'x') { prefix = "0x"; prefix_len = 2u; }
                    else if (spec == 'X') { prefix = "0X"; prefix_len = 2u; }
                }
                size_t idx = fmt_u64_base(scratch, uval, 16u, upper);
                piece = &scratch[idx];
                piece_len = 31u - idx;
            }
        } else if (spec == 'f' || spec == 'F' || spec == 'e' || spec == 'E' ||
                   spec == 'g' || spec == 'G' || spec == 'a' || spec == 'A') {
            (void)va_arg(ap, double);
            scratch[0] = '@';
            piece = scratch;
            piece_len = 1u;
        } else {
            scratch[0] = '%';
            scratch[1] = spec;
            piece = scratch;
            piece_len = 2u;
        }

        size_t prec_zeros = 0u;
        if (precision >= 0 && spec != 's' && spec != 'c' && (size_t)precision > piece_len) {
            prec_zeros = (size_t)precision - piece_len;
            zero_pad = 0;
        }

        size_t full_len = prefix_len + prec_zeros + piece_len;
        size_t pad_len = ((size_t)width > full_len) ? ((size_t)width - full_len) : 0u;

        if (!left_align && !zero_pad) {
            for (size_t idx = 0u; idx < pad_len; idx++) {
                fmt_emit_char(sink, ' ', &total);
            }
        }
        for (size_t idx = 0u; idx < prefix_len; idx++) {
            fmt_emit_char(sink, (uint8_t)prefix[idx], &total);
        }
        if (!left_align && zero_pad) {
            for (size_t idx = 0u; idx < pad_len; idx++) {
                fmt_emit_char(sink, '0', &total);
            }
        }
        for (size_t idx = 0u; idx < prec_zeros; idx++) {
            fmt_emit_char(sink, '0', &total);
        }
        for (size_t idx = 0u; idx < piece_len; idx++) {
            fmt_emit_char(sink, (uint8_t)piece[idx], &total);
        }
        if (left_align) {
            for (size_t idx = 0u; idx < pad_len; idx++) {
                fmt_emit_char(sink, ' ', &total);
            }
        }
    }

    return total;
}

/* @shim RVA 0x4f4d0 rtk_vprintf */
int rtk_vprintf(const char *fmt, va_list ap)
{
    rtk_fmt_sink_t sink = {
        .putc_fn   = rtk_vprintf_nop_putc,
        .buf_size  = 0u,
        .buf_ptr   = NULL,
        .write_pos = 0u,
        .aux_word  = 0u,
    };
    return rtk_vformat(&sink, fmt, ap);
}

/* @shim RVA 0x4f4b0 rtk_printf */
int rtk_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int res = rtk_vprintf(fmt, ap);
    va_end(ap);
    return res;
}

/* @shim RVA 0x4fab0 rtk_vsnprintf */
int rtk_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    char dummy[2] = {0, 0};
    char *target_buf = (size != 0u) ? buf : dummy;
    size_t eff_size = (size != 0u) ? size : 2u;

    if (target_buf != NULL) {
        target_buf[0] = '\0';
    }
    rtk_fmt_sink_t sink = {
        .putc_fn   = rtk_vsnprintf_putc,
        .buf_size  = eff_size,
        .buf_ptr   = target_buf,
        .write_pos = 0u,
        .aux_word  = 0u,
    };
    return rtk_vformat(&sink, fmt, ap);
}

/* @shim RVA 0x4fb10 sub_4FB10 */
int rtk_vsnprintf_clamped(char *buf, size_t size, const char *fmt, va_list ap)
{
    int written = rtk_vsnprintf(buf, size, fmt, ap);
    int clamped_nonneg = (written < 0) ? 0 : written;
    int max_fit = (int)size - 1;
    return (clamped_nonneg < max_fit) ? clamped_nonneg : max_fit;
}

/* @shim RVA 0x4fa96 sub_4FA96 */
int rtk_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int res = rtk_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return res;
}

/* @shim RVA 0x4fb28 sub_4FB28 */
void rtk_call_once(rtk_once_t *once_ctrl, void (*init_fn)(void))
{
    if ((once_ctrl->done_flag & 1u) == 0u) {
        uint32_t lock_st = rtk_fastlock_lock(&once_ctrl->lock_word);
        (void)lock_st;
        if ((once_ctrl->done_flag & 1u) == 0u) {
            if (init_fn != NULL) {
                init_fn();
            }
            once_ctrl->done_flag = 1u;
        }
        (void)rtk_fastlock_unlock(&once_ctrl->lock_word);
    }
}
