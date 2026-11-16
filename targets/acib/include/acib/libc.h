#ifndef ACIB_LIBC_H
#define ACIB_LIBC_H

#include <stddef.h>
#include <stdint.h>

void    *acib_memchr(const void *s, int c, size_t n);
int      acib_memcmp(const void *s1, const void *s2, size_t n);
void    *memcpy_wrapper(void *dst, const void *src, size_t n);
void    *memmove(void *dst, const void *src, size_t n);
void    *memset(void *dst, int c, size_t n);
size_t   acib_strlen(const char *s);
int      acib_strncmp(const char *s1, const char *s2, size_t n);
uint64_t acib_aeabi_llsl(uint64_t val, uint32_t shift);
uint64_t acib_aeabi_llsr(uint64_t val, uint32_t shift);

#endif /* ACIB_LIBC_H */
