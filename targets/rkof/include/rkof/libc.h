#ifndef RKOF_LIBC_H
#define RKOF_LIBC_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#if defined(RKOF_HOST_BUILD)
#define RKOF_LIBC_ASM(sym)
#else
#define RKOF_LIBC_ASM(sym) __asm__(sym)
#endif

void *sub_CA68(const void *buf, int ch, size_t len);
int rkof_memcmp(const void *lhs, const void *rhs, size_t len) RKOF_LIBC_ASM("memcmp");
void *memcpy_thunk(void *dst, const void *src, size_t len);
void *rkof_memcpy(void *dst, const void *src, size_t len) RKOF_LIBC_ASM("memcpy");
void *j_memcpy(void *dst, const void *src, size_t len);
void *rkof_memset(void *dst, int byte_val, size_t len) RKOF_LIBC_ASM("memset");
char *sub_CD48(const char *str, int ch);
int rkof_strcmp(const char *lhs, const char *rhs) RKOF_LIBC_ASM("strcmp");
size_t sub_CE20(const char *str);
int rkof_strncmp(const char *lhs, const char *rhs, size_t max_len) RKOF_LIBC_ASM("strncmp");
uint32_t rtkit_mem_is_filled(const void *buf, uint32_t byte_val, size_t len);
int rkof_vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap) RKOF_LIBC_ASM("vsnprintf");
int rkof_snprintf(char *buf, size_t cap, const char *fmt, ...) RKOF_LIBC_ASM("snprintf");

/* Compiler-RT / AEABI 64-bit integer & IEEE-754 helper surface */
int32_t sub_D210(uint64_t *mantissa_io);
uint64_t sub_D23C(uint64_t val, uint32_t shift);
int32_t sub_D5A4(uint64_t *mantissa_io);
int64_t sub_D5D0(int64_t num, int64_t den);
double sub_D610(float val);
int32_t sub_D680(double val);
int64_t sub_D6C2(float val);
uint32_t sub_D704(double val);
uint64_t sub_D736(float val);
float sub_D7BC(int64_t val);
uint64_t sub_D9F8(uint64_t val, uint32_t shift);
uint64_t sub_DD68(uint64_t num, uint64_t den);
uint64_t sub_DD76(uint64_t num, uint64_t den, uint64_t *rem_out);

#endif /* RKOF_LIBC_H */
