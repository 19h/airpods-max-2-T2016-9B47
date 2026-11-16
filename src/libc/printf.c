/*
 * printf.c -- the sink-based formatter behind snprintf()/vsnprintf().
 *
 * Supported conversions: %c %d %i %u %x %X %p %s and %%, flags '-' '+' '0' '#',
 * field width (digits or '*'), precision, and the h/l/z length modifiers.
 * Floating-point conversions are not compiled in and print "@"; an unknown
 * conversion echoes '%' followed by the character.
 */
#include "btp/libc.h"
#include "btp/rtkit.h"
#include <stdbool.h>

#define FMT_NUMBER_MAX  24          /* 64-bit value in any base plus prefix */

static const char fmt_digits_lower[] = "0123456789abcdef";
static const char fmt_digits_upper[] = "0123456789ABCDEF";

struct fmt_spec {
    bool left;
    bool plus;
    bool zero;
    bool alternate;
    int  width;
    int  precision;     /* -1: unspecified */
    char length;        /* 0, 'h', 'H' (hh), 'l', 'L' (ll), 'z' */
};

/* The recovered core keeps its emission count separately from the sink.
 * In particular, the word at sink+0x10 is never modified by either routine. */
struct fmt_output {
    struct fmt_sink *sink;
    uint32_t count;
};

/* @provenance RVA 0x6A38 _snprintf_putc */
static void fmt_sink_putc(struct fmt_sink *sink, int c)
{
    if (sink->buffer && sink->stored < sink->capacity - 1) {
        size_t index = sink->stored;
        /* 0x6A38 terminates the next slot before publishing this character. */
        if (c != 0)
            ((volatile char *)sink->buffer)[index + 1] = '\0';
        sink->stored = index + 1;
        ((volatile char *)sink->buffer)[index] = (char)c;
    }
}

/* @provenance RVA 0x6B3E _vsnprintf_outlined_emit_char */
static inline void fmt_emit(struct fmt_output *output, int c)
{
    output->sink->putc(output->sink, c);
    output->count++;
}

static void fmt_pad(struct fmt_output *sink, char c, int count)
{
    while (count > 0) {
        fmt_emit(sink, c);
        --count;
    }
}

static void fmt_string(struct fmt_output *sink, const char *s, const struct fmt_spec *spec)
{
    int len;

    if (!s)
        s = "<null>";
    /* The recovered formatter parses precision but ignores it for strings. */
    len = (int)strlen(s);

    if (!spec->left)
        fmt_pad(sink, spec->zero ? '0' : ' ', spec->width - len);
    for (int i = 0; i < len; ++i)
        fmt_emit(sink, s[i]);
    if (spec->left)
        fmt_pad(sink, ' ', spec->width - len);
}

/* Render `value` in `base`, most significant digit first; returns the length. */
static int fmt_unsigned(char *out, uint64_t value, unsigned base, bool upper)
{
    const char *digits = upper ? fmt_digits_upper : fmt_digits_lower;
    int n = 0;

    if (value == 0) {
        out[n++] = '0';
        return n;
    }
    while (value) {
        uint64_t quotient = __udivdi3(value, base);

        out[n++] = digits[(unsigned)(value - quotient * base)];
        value = quotient;
    }
    for (int i = 0; i < n / 2; ++i) {
        char t = out[i];
        out[i] = out[n - 1 - i];
        out[n - 1 - i] = t;
    }
    return n;
}

/* @provenance RVA 0x694C _format_uint64_decimal */
int fmt_u64_decimal(char *buffer, size_t capacity, uint64_t value)
{
    char tmp[FMT_NUMBER_MAX];
    int n = fmt_unsigned(tmp, value, 10, false);
    size_t i;

    for (i = 0; i < (size_t)n && i + 1 < capacity; ++i)
        buffer[i] = tmp[i];
    if (capacity)
        buffer[i] = '\0';
    return n;
}

static uint64_t fmt_arg_unsigned(va_list *args, char length)
{
    switch (length) {
    case 'L': return va_arg(*args, unsigned long long);
    case 'l': return va_arg(*args, unsigned long);
    case 'z': return va_arg(*args, size_t);
    case 'h': return (uint16_t)va_arg(*args, unsigned int);
    case 'H': return (uint8_t)va_arg(*args, unsigned int);
    default:  return va_arg(*args, unsigned int);
    }
}

static int64_t fmt_arg_signed(va_list *args, char length)
{
    switch (length) {
    case 'L': return va_arg(*args, long long);
    case 'l': return va_arg(*args, long);
    case 'z': return (int64_t)va_arg(*args, size_t);
    case 'h': return (int16_t)va_arg(*args, int);
    case 'H': return (int8_t)va_arg(*args, int);
    default:  return va_arg(*args, int);
    }
}

/* Emit a rendered number applying precision (zero fill), width and flags. */
static void fmt_number(struct fmt_output *sink, char *number, int len,
                       const struct fmt_spec *spec)
{
    int sign = (number[0] == '-' || number[0] == '+') ? 1
             : (len >= 2 && number[0] == '0' && number[1] == 'x' ? 2 : 0);

    /* Precision has no effect on integer output in this image. Padding is
     * streamed: it must never expand the bounded digits array. */

    if (spec->width <= len) {
        for (int i = 0; i < len; ++i)
            fmt_emit(sink, number[i]);
        return;
    }

    if (spec->left) {
        for (int i = 0; i < len; ++i)
            fmt_emit(sink, number[i]);
        fmt_pad(sink, ' ', spec->width - len);
    } else if (spec->zero) {
        for (int i = 0; i < sign; ++i)
            fmt_emit(sink, number[i]);
        fmt_pad(sink, '0', spec->width - len);
        for (int i = sign; i < len; ++i)
            fmt_emit(sink, number[i]);
    } else {
        fmt_pad(sink, ' ', spec->width - len);
        for (int i = 0; i < len; ++i)
            fmt_emit(sink, number[i]);
    }
}

/* @provenance RVA 0x6444 _vsnprintf_core */
int fmt_vformat(struct fmt_sink *sink, const char *format, va_list *args)
{
    const char *p = format;
    struct fmt_output output = {sink, 0};
    volatile uint32_t saved_guard = __stack_chk_guard;

    while (*p) {
        struct fmt_spec spec = { false, false, false, false, 0, 0, 0 };
        char number[FMT_NUMBER_MAX + 2];
        int len;
        char conversion;

        if (*p != '%') {
            fmt_emit(&output, (uint8_t)*p++);
            continue;
        }
        ++p;

        /* The firmware accepts modifiers in any order, unlike the standard
         * printf grammar. Track the two length flags independently. */
        {
            unsigned short_count = 0, long_count = 0;
            bool size_length = false, precision_field = false;
            for (;;) {
                char token = *p;
                int *field = precision_field ? &spec.precision : &spec.width;
                if (token == '-') {
                    spec.left = true;
                    spec.zero = false;
                } else if (token == '+') {
                    spec.plus = true;
                } else if (token == '#') {
                    spec.alternate = true;
                } else if (token == '.') {
                    precision_field = true;
                } else if (token == '*') {
                    *field = va_arg(*args, int);
                } else if (token >= '0' && token <= '9') {
                    if (token == '0' && spec.width == 0)
                        spec.zero = true;
                    else
                        *field = (int)((uint32_t)*field * 10u + (unsigned)(token - '0'));
                } else if (token == 'h') {
                    if (short_count < 2)
                        ++short_count;
                } else if (token == 'l') {
                    if (long_count < 2)
                        ++long_count;
                } else if (token == 'z') {
                    size_length = true;
                } else {
                    break;
                }
                ++p;
            }
            spec.length = long_count >= 2 ? 'L' : (long_count ? 'l'
                         : (short_count >= 2 ? 'H' : (short_count ? 'h'
                         : (size_length ? 'z' : 0))));
        }
        conversion = *p;
        if (!conversion)
            break;
        ++p;

        switch (conversion) {
        case '%':
            fmt_emit(&output, '%');
            continue;

        case 'c':
            fmt_emit(&output, (uint8_t)va_arg(*args, int));
            continue;

        case 'n':
            /* 0x66A6 consumes no argument and performs no store. */
            continue;

        case 's':
            fmt_string(&output, va_arg(*args, const char *), &spec);
            continue;

        case 'd':
        case 'i': {
            int64_t value = fmt_arg_signed(args, spec.length);
            uint64_t magnitude;

            len = 0;
            if (value < 0) {
                number[len++] = '-';
                magnitude = 0u - (uint64_t)value;
            } else {
                if (spec.plus)
                    number[len++] = '+';
                magnitude = (uint64_t)value;
            }
            len += fmt_unsigned(number + len, magnitude, 10, false);
            break;
        }

        case 'u':
            len = 0;
            if (spec.plus)
                number[len++] = '+';
            len += fmt_unsigned(number + len, fmt_arg_unsigned(args, spec.length), 10, false);
            break;

        case 'p':
            spec.alternate = true;
            if (spec.length != 'L')
                spec.length = 'l';
            /* fall through */
        case 'x':
        case 'X':
            len = fmt_unsigned(number, fmt_arg_unsigned(args, spec.length), 16,
                               conversion == 'X');
            if (spec.alternate) {
                memmove(number + 2, number, (size_t)len);
                number[0] = '0';
                number[1] = 'x';
                len += 2;
            }
            break;

        case 'f':
        case 'e':
        case 'g':
        case 'a':
        case 'A':
        case 'E':
        case 'F':
        case 'G':
            /* floating point is not compiled into this image */
            (void)va_arg(*args, double);
            if (spec.precision > spec.width)
                spec.width = spec.precision;
            spec.zero = false;
            fmt_string(&output, "@", &spec);
            continue;

        default:
            fmt_emit(&output, '%');
            fmt_emit(&output, (uint8_t)conversion);
            continue;
        }

        fmt_number(&output, number, len, &spec);
    }
    if (saved_guard != __stack_chk_guard)
        __stack_chk_fail();
    return (int)output.count;
}

/* @provenance RVA 0x6A00 vsnprintf */
int vsnprintf(char *dst, size_t capacity, const char *format, va_list args)
{
    char discard[2];
    if (capacity == 0) {
        dst = discard;
        capacity = sizeof discard;
    }
    struct fmt_sink sink = {
        .putc = fmt_sink_putc,
        .capacity = capacity,
        .buffer = dst,
        .stored = 0,
        .total = 0,
    };
    va_list copy;
    int result;

    va_copy(copy, args);
    if (dst)
        dst[0] = '\0';
    result = fmt_vformat(&sink, format, &copy);
    va_end(copy);

    return result;
}

/* @provenance RVA 0x69E6 snprintf */
int snprintf(char *dst, size_t capacity, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = vsnprintf(dst, capacity, format, args);
    va_end(args);
    return result;
}
