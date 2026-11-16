/*
 * libc.h -- the freestanding C library carried by the image.
 */
#ifndef BTP_LIBC_H
#define BTP_LIBC_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

void  *memcpy(void *dst, const void *src, size_t len);
void  *memmove(void *dst, const void *src, size_t len);
void  *memset(void *dst, int value, size_t len);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
int    strncmp(const char *left, const char *right, size_t count);
char  *strrchr(const char *s, int c);
size_t strlcpy(char *dst, const char *src, size_t size);
size_t strlcat(char *dst, const char *src, size_t size);
int    snprintf(char *dst, size_t capacity, const char *format, ...);
int    vsnprintf(char *dst, size_t capacity, const char *format, va_list args);

/*
 * The formatter writes through a sink so the same core serves snprintf and
 * (in debug builds) the console.
 */
struct fmt_sink {
    void   (*putc)(struct fmt_sink *sink, int c);
    size_t   capacity;
    char    *buffer;
    size_t   stored;        /* characters actually stored */
    size_t   total;         /* reserved; reference core leaves this word unchanged */
};

int fmt_vformat(struct fmt_sink *sink, const char *format, va_list *args);
int fmt_u64_decimal(char *buffer, size_t capacity, uint64_t value);

/* compiler-rt helpers referenced by the image */
uint64_t __aeabi_llsl(uint64_t value, int shift);
uint64_t __aeabi_llsr(uint64_t value, int shift);
uint64_t __udivdi3(uint64_t numerator, uint64_t denominator);

#endif /* BTP_LIBC_H */
