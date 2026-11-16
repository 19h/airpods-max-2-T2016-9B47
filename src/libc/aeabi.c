/*
 * aeabi.c -- ARM EABI run-time helpers linked into the image.
 *
 * The image is linked without compiler-rt, so the handful of helpers the
 * compiler may call out to are provided here.  The two shifts use one defined
 * C implementation on both host and target, including ARM's low-byte amount.
 * Division still retains target instructions and needs separate C recovery.
 */
#include "btp/libc.h"

/* ---- 64-bit shifts and division -------------------------------------------- */

/* Register shifts in the reference use the low byte of R2. The original
 * helpers return zero for amounts 64..255, then repeat at multiples of 256.
 * Keep every C word shift in 0..31 so the behavior is defined for all input
 * bit patterns, including negative int arguments. */
/* @provenance RVA 0x5F54 __aeabi_llsl */
uint64_t __aeabi_llsl(uint64_t value, int shift)
{
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    uint32_t amount = (uint32_t)shift & 0xFFu;

    if (amount == 0)
        return value;
    if (amount < 32) {
        high = (high << amount) | (low >> (32 - amount));
        low <<= amount;
    } else if (amount < 64) {
        high = low << (amount - 32);
        low = 0;
    } else {
        return 0;
    }
    return ((uint64_t)high << 32) | low;
}

/* @provenance RVA 0x5F76 __aeabi_llsr */
uint64_t __aeabi_llsr(uint64_t value, int shift)
{
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    uint32_t amount = (uint32_t)shift & 0xFFu;

    if (amount == 0)
        return value;
    if (amount < 32) {
        low = (low >> amount) | (high << (32 - amount));
        high >>= amount;
    } else if (amount < 64) {
        low = high >> (amount - 32);
        high = 0;
    } else {
        return 0;
    }
    return ((uint64_t)high << 32) | low;
}

/* Unsigned 64-bit division by shift-and-subtract. */
/* @provenance RVA 0x5F98 __udivdi3 */
#ifdef BTP_HOST
uint64_t __udivdi3(uint64_t numerator, uint64_t denominator)
{
    uint64_t quotient = 0;
    uint64_t bit = 1;

    if (denominator == 0)
        return 0;
    while ((int64_t)denominator >= 0 && denominator < numerator) {
        denominator = __aeabi_llsl(denominator, 1);
        bit = __aeabi_llsl(bit, 1);
    }
    while (bit) {
        if (numerator >= denominator) {
            numerator -= denominator;
            quotient |= bit;
        }
        denominator = __aeabi_llsr(denominator, 1);
        bit = __aeabi_llsr(bit, 1);
    }
    return quotient;
}
#else
__attribute__((naked))
uint64_t __udivdi3(uint64_t numerator, uint64_t denominator)
{
    __asm__ volatile(
        "push.w {r3, r4, r5, r6, r7, r8, sb, sl, fp, lr}\n"
        "mov r6, r0\n"
        "clz r0, r0\n"
        "cmp r1, #0\n"
        "add.w r5, r0, #0x20\n"
        "clz r0, r2\n"
        "it ne\n"
        "clzne r5, r1\n"
        "add.w r4, r0, #0x20\n"
        "cmp r3, #0\n"
        "it ne\n"
        "clzne r4, r3\n"
        "subs r0, r4, r5\n"
        "cmp r0, #0x3f\n"
        "bls .L__udivdi3_5fcc\n"
        "movs r6, #0\n"
        "mov.w sb, #0\n"
        "b .L__udivdi3_6052\n"
        ".L__udivdi3_5fcc:\n"
        "mov sb, r1\n"
        "beq .L__udivdi3_6052\n"
        "mov r8, r2\n"
        "adds r2, r0, #1\n"
        "str r0, [sp]\n"
        "mov r0, r6\n"
        "mov r1, sb\n"
        "mov sl, r3\n"
        "bl __aeabi_llsr\n"
        "mov r7, r0\n"
        "ldr r0, [sp]\n"
        "mov fp, r1\n"
        "mov r1, sb\n"
        "rsb.w r2, r0, #0x3f\n"
        "mov r0, r6\n"
        "bl __aeabi_llsl\n"
        "mvns r2, r4\n"
        "movs r4, #0\n"
        "mov.w ip, #0\n"
        "add.w lr, r2, r5\n"
        ".L__udivdi3_5ffe:\n"
        "cmp.w lr, #0\n"
        "orr.w r6, r4, r0, lsl #1\n"
        "lsl.w r3, r1, #1\n"
        "beq .L__udivdi3_604a\n"
        "lsls r5, r7, #1\n"
        "lsl.w r4, fp, #1\n"
        "orr.w r0, r3, r0, lsr #31\n"
        "add.w lr, lr, #1\n"
        "orr.w r1, r5, r1, lsr #31\n"
        "orr.w r4, r4, r7, lsr #31\n"
        "subs.w r7, r1, r8\n"
        "sbc.w fp, r4, sl\n"
        "mvns r2, r4\n"
        "mvns r5, r1\n"
        "adds.w r5, r5, r8\n"
        "adcs.w r2, r2, sl\n"
        "itt pl\n"
        "movpl fp, r4\n"
        "movpl r7, r1\n"
        "orr.w r1, ip, r0\n"
        "mov.w ip, #0\n"
        "mov r0, r6\n"
        "lsrs r4, r2, #0x1f\n"
        "b .L__udivdi3_5ffe\n"
        ".L__udivdi3_604a:\n"
        "orr.w r0, r3, r0, lsr #31\n"
        "orr.w sb, ip, r0\n"
        ".L__udivdi3_6052:\n"
        "mov r0, r6\n"
        "mov r1, sb\n"
        "pop.w {r3, r4, r5, r6, r7, r8, sb, sl, fp, pc}\n"
    );
}
#endif

/* ---- memory helpers -------------------------------------------------------- */

void __aeabi_memcpy(void *dst, const void *src, size_t n);
void __aeabi_memcpy4(void *dst, const void *src, size_t n);
void __aeabi_memcpy8(void *dst, const void *src, size_t n);
void __aeabi_memmove(void *dst, const void *src, size_t n);
void __aeabi_memmove4(void *dst, const void *src, size_t n);
void __aeabi_memmove8(void *dst, const void *src, size_t n);
void __aeabi_memset(void *dst, size_t n, int c);
void __aeabi_memset4(void *dst, size_t n, int c);
void __aeabi_memset8(void *dst, size_t n, int c);
void __aeabi_memclr(void *dst, size_t n);
void __aeabi_memclr4(void *dst, size_t n);
void __aeabi_memclr8(void *dst, size_t n);

void __aeabi_memcpy(void *dst, const void *src, size_t n)   { memmove(dst, src, n); }
void __aeabi_memcpy4(void *dst, const void *src, size_t n)  { memmove(dst, src, n); }
void __aeabi_memcpy8(void *dst, const void *src, size_t n)  { memmove(dst, src, n); }
void __aeabi_memmove(void *dst, const void *src, size_t n)  { memmove(dst, src, n); }
void __aeabi_memmove4(void *dst, const void *src, size_t n) { memmove(dst, src, n); }
void __aeabi_memmove8(void *dst, const void *src, size_t n) { memmove(dst, src, n); }
void __aeabi_memset(void *dst, size_t n, int c)             { memset(dst, c, n); }
void __aeabi_memset4(void *dst, size_t n, int c)            { memset(dst, c, n); }
void __aeabi_memset8(void *dst, size_t n, int c)            { memset(dst, c, n); }
void __aeabi_memclr(void *dst, size_t n)                    { memset(dst, 0, n); }
void __aeabi_memclr4(void *dst, size_t n)                   { memset(dst, 0, n); }
void __aeabi_memclr8(void *dst, size_t n)                   { memset(dst, 0, n); }
