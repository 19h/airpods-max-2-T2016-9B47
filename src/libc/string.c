/*
 * string.c -- memory and string routines of the freestanding C library.
 *
 * The image carries hand-tuned, word-at-a-time versions of these routines.
 * Target assembly preserves access widths and ordering, including word reads
 * in strlen/strncmp and forward byte propagation in overlapping strl* calls.
 * The host adapter uses byte loops with the same final-memory contracts:
 * memcpy tolerates overlap (it is memmove), memset returns its destination,
 * and the BSD strl* routines return the length they tried to create.
 */
#include "btp/libc.h"

/* @provenance RVA 0x5C68 btp_memcpy */
#ifdef BTP_HOST
void *memcpy(void *dst, const void *src, size_t len)
{
    return memmove(dst, src, len);
}
#else
__attribute__((naked))
void *memcpy(void *dst, const void *src, size_t len)
{
    __asm__ volatile("b.w memmove");
}
#endif

/* @provenance RVA 0x5C70 memmove */
/* @provenance RVA 0x5C76 _memmove_core */
#ifdef BTP_HOST
void *memmove(void *dst, const void *src, size_t len)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    if (d == s || len == 0)
        return dst;

    if (d < s || d >= s + len) {
        while (len--)
            *d++ = *s++;
    } else {
        d += len;
        s += len;
        while (len--)
            *--d = *--s;
    }
    return dst;
}
#else
__attribute__((naked))
void *memmove(void *dst, const void *src, size_t len)
{
    __asm__ volatile(
        "subs r3, r0, r1\n"
        "it eq\n"
        "bxeq lr\n"
        "push.w {r0, r4, r7, sb, lr}\n"
        "add r7, sp, #8\n"
        "cmp r3, r2\n"
        "blo .Lmemmove_5cfe\n"
        "cmp r2, #4\n"
        "blo .Lmemmove_5cec\n"
        ".Lmemmove_5c84:\n"
        "tst.w r0, #3\n"
        "ittt ne\n"
        "ldrbne.w r3, [r1], #1\n"
        "strbne.w r3, [r0], #1\n"
        "subne r2, r2, #1\n"
        "bne .Lmemmove_5c84\n"
        "tst.w r1, #3\n"
        "it ne\n"
        "subne r2, #0x10\n"
        "bne .Lmemmove_5cd8\n"
        "subs r2, #0x40\n"
        "blo .Lmemmove_5cc8\n"
        "push.w {r5, r6, r8, sl}\n"
        "nop \n"
        "nop \n"
        "nop \n"
        "nop \n"
        ".Lmemmove_5cb0:\n"
        "ldm.w r1!, {r3, r4, r5, r6, r8, sb, sl, ip}\n"
        "subs r2, #0x40\n"
        "stm.w r0!, {r3, r4, r5, r6, r8, sb, sl, ip}\n"
        "ldm.w r1!, {r3, r4, r5, r6, r8, sb, sl, ip}\n"
        "stm.w r0!, {r3, r4, r5, r6, r8, sb, sl, ip}\n"
        "bhs .Lmemmove_5cb0\n"
        "pop.w {r5, r6, r8, sl}\n"
        ".Lmemmove_5cc8:\n"
        "adds r2, #0x30\n"
        "blo .Lmemmove_5cd8\n"
        ".Lmemmove_5ccc:\n"
        "subs r2, #0x10\n"
        "ldm.w r1!, {r3, r4, sb, ip}\n"
        "stm.w r0!, {r3, r4, sb, ip}\n"
        "bhs .Lmemmove_5ccc\n"
        ".Lmemmove_5cd8:\n"
        "adds r2, #0xc\n"
        "blo .Lmemmove_5ce8\n"
        ".Lmemmove_5cdc:\n"
        "subs r2, r2, #4\n"
        "ldr.w r3, [r1], #4\n"
        "str.w r3, [r0], #4\n"
        "bhs .Lmemmove_5cdc\n"
        ".Lmemmove_5ce8:\n"
        "add.w r2, r2, #4\n"
        ".Lmemmove_5cec:\n"
        "subs r2, r2, #1\n"
        "itt hs\n"
        "ldrbhs.w r3, [r1], #1\n"
        "strbhs.w r3, [r0], #1\n"
        "bhi .Lmemmove_5cec\n"
        "pop.w {r0, r4, r7, sb, pc}\n"
        ".Lmemmove_5cfe:\n"
        "add r1, r2\n"
        "add r0, r2\n"
        "cmp r2, #4\n"
        "blo .Lmemmove_5d6c\n"
        ".Lmemmove_5d06:\n"
        "tst.w r0, #3\n"
        "ittt ne\n"
        "ldrbne.w r3, [r1, #-0x1]!\n"
        "strbne.w r3, [r0, #-0x1]!\n"
        "subne r2, r2, #1\n"
        "bne .Lmemmove_5d06\n"
        "tst.w r1, #3\n"
        "it ne\n"
        "subne r2, #0x10\n"
        "bne .Lmemmove_5d58\n"
        "subs r2, #0x40\n"
        "blo .Lmemmove_5d48\n"
        "push.w {r5, r6, r8, sl}\n"
        "nop \n"
        "nop \n"
        "nop \n"
        ".Lmemmove_5d30:\n"
        "ldmdb.w r1!, {r3, r4, r5, r6, r8, sb, sl, ip}\n"
        "subs r2, #0x40\n"
        "stmdb.w r0!, {r3, r4, r5, r6, r8, sb, sl, ip}\n"
        "ldmdb.w r1!, {r3, r4, r5, r6, r8, sb, sl, ip}\n"
        "stmdb.w r0!, {r3, r4, r5, r6, r8, sb, sl, ip}\n"
        "bhs .Lmemmove_5d30\n"
        "pop.w {r5, r6, r8, sl}\n"
        ".Lmemmove_5d48:\n"
        "adds r2, #0x30\n"
        "blo .Lmemmove_5d58\n"
        ".Lmemmove_5d4c:\n"
        "subs r2, #0x10\n"
        "ldmdb.w r1!, {r3, r4, sb, ip}\n"
        "stmdb.w r0!, {r3, r4, sb, ip}\n"
        "bhs .Lmemmove_5d4c\n"
        ".Lmemmove_5d58:\n"
        "adds r2, #0xc\n"
        "blo .Lmemmove_5d68\n"
        ".Lmemmove_5d5c:\n"
        "subs r2, r2, #4\n"
        "ldr.w r3, [r1, #-0x4]!\n"
        "str.w r3, [r0, #-0x4]!\n"
        "bhs .Lmemmove_5d5c\n"
        ".Lmemmove_5d68:\n"
        "add.w r2, r2, #4\n"
        ".Lmemmove_5d6c:\n"
        "subs r2, r2, #1\n"
        "itt hs\n"
        "ldrbhs.w r3, [r1, #-0x1]!\n"
        "strbhs.w r3, [r0, #-0x1]!\n"
        "bhi .Lmemmove_5d6c\n"
        "pop.w {r0, r4, r7, sb, pc}\n"
    );
}
#endif

/* @provenance RVA 0x5D80 btp_memset */
#ifdef BTP_HOST
void *memset(void *dst, int value, size_t len)
{
    uint8_t *d = dst;

    while (len--)
        *d++ = (uint8_t)value;
    return dst;
}
#else
__attribute__((naked))
void *memset(void *dst, int value, size_t len)
{
    __asm__ volatile(
        "push {r0, r4, r7, lr}\n"
        "add r7, sp, #8\n"
        "and.w r1, r1, #0xff\n"
        "cmp r2, #4\n"
        "orr.w r1, r1, r1, lsl #8\n"
        "blo .Lmemset_5dec\n"
        "orr.w r1, r1, r1, lsl #16\n"
        ".Lmemset_5d94:\n"
        "tst.w r0, #3\n"
        "itt ne\n"
        "strbne.w r1, [r0], #1\n"
        "subne r2, r2, #1\n"
        "bne .Lmemset_5d94\n"
        "mov r3, r1\n"
        "mov r4, r1\n"
        "subs r2, #0x40\n"
        "mov ip, r1\n"
        "blo .Lmemset_5dd0\n"
        "push.w {r5, r6, r8, sb}\n"
        "mov r5, r1\n"
        "mov r6, r1\n"
        "mov r8, r1\n"
        "mov sb, r1\n"
        "nop \n"
        "nop \n"
        "nop \n"
        "nop \n"
        ".Lmemset_5dc0:\n"
        "stm.w r0!, {r1, r3, r4, r5, r6, r8, sb, ip}\n"
        "subs r2, #0x40\n"
        "stm.w r0!, {r1, r3, r4, r5, r6, r8, sb, ip}\n"
        "bhs .Lmemset_5dc0\n"
        "pop.w {r5, r6, r8, sb}\n"
        ".Lmemset_5dd0:\n"
        "adds r2, #0x30\n"
        "blo .Lmemset_5ddc\n"
        ".Lmemset_5dd4:\n"
        "subs r2, #0x10\n"
        "stm.w r0!, {r1, r3, r4, ip}\n"
        "bhs .Lmemset_5dd4\n"
        ".Lmemset_5ddc:\n"
        "adds r2, #0xc\n"
        "blo .Lmemset_5de8\n"
        ".Lmemset_5de0:\n"
        "subs r2, r2, #4\n"
        "str.w r1, [r0], #4\n"
        "bhs .Lmemset_5de0\n"
        ".Lmemset_5de8:\n"
        "str r1, [r0, r2]\n"
        "pop {r0, r4, r7, pc}\n"
        ".Lmemset_5dec:\n"
        "subs r2, r2, #1\n"
        "it hs\n"
        "strbhs.w r1, [r0], #1\n"
        "bhi .Lmemset_5dec\n"
        "pop {r0, r4, r7, pc}\n"
    );
}
#endif

/* @provenance RVA 0x5E00 strlen */
#ifdef BTP_HOST
size_t strlen(const char *s)
{
    const char *p = s;

    while (*p)
        ++p;
    return (size_t)(p - s);
}
#else
__attribute__((naked, aligned(4)))
size_t strlen(const char *s)
{
    __asm__ volatile(
        "push {r7, lr}\n"
        "mov r7, sp\n"
        "ldr r3, .Lstrlen_constant\n"
        "add.w ip, r0, #4\n"
        "and.w r2, r0, #3\n"
        "bic.w r0, r0, #3\n"
        "lsl.w r2, r2, #3\n"
        "ldr.w r1, [r0], #4\n"
        "rsb.w r2, r2, #0x20\n"
        "lsr.w r2, r3, r2\n"
        "orr.w r1, r1, r2\n"
        "sub.w r2, r1, r3\n"
        "bic.w r2, r2, r1\n"
        "tst.w r2, r3, lsl #7\n"
        "bne .Lstrlen_5e52\n"
        "nop \n"
        "nop \n"
        "nop \n"
        "nop \n"
        "nop \n"
        "nop \n"
        ".Lstrlen_5e40:\n"
        "ldr.w r1, [r0], #4\n"
        "sub.w r2, r1, r3\n"
        "bic.w r2, r2, r1\n"
        "tst.w r2, r3, lsl #7\n"
        "beq .Lstrlen_5e40\n"
        ".Lstrlen_5e52:\n"
        "sub.w r0, r0, ip\n"
        "tst.w r1, #0xff\n"
        "beq .Lstrlen_5e6e\n"
        "add.w r0, r0, #1\n"
        "tst.w r1, #0xff00\n"
        "ittt ne\n"
        "addne r0, #1\n"
        "tstne.w r1, #0xff0000\n"
        "addne r0, #1\n"
        ".Lstrlen_5e6e:\n"
        "pop {r7, pc}\n"
        ".Lstrlen_constant:\n"
        ".word 0x01010101\n"
    );
}
#endif

/* @provenance RVA 0x6AEA strnlen */
#ifdef BTP_HOST
size_t strnlen(const char *s, size_t maxlen)
{
    size_t n = 0;

    while (n < maxlen && s[n])
        ++n;
    return n;
}
#else
__attribute__((naked, aligned(4)))
size_t strnlen(const char *s, size_t maxlen)
{
    __asm__ volatile(
        "movs r2, #0\n"
        ".Lstrnlen_6aec:\n"
        "cmp r1, r2\n"
        "beq .Lstrnlen_6afa\n"
        "ldrb r3, [r0, r2]\n"
        "cbz r3, .Lstrnlen_6af8\n"
        "adds r2, #1\n"
        "b .Lstrnlen_6aec\n"
        ".Lstrnlen_6af8:\n"
        "mov r1, r2\n"
        ".Lstrnlen_6afa:\n"
        "mov r0, r1\n"
        "bx lr\n"
    );
}
#endif

/* @provenance RVA 0x5E88 strncmp */
#ifdef BTP_HOST
int strncmp(const char *left, const char *right, size_t count)
{
    while (count--) {
        uint8_t a = (uint8_t)*left++;
        uint8_t b = (uint8_t)*right++;

        if (a != b)
            return (int)a - (int)b;
        if (a == 0)
            break;
    }
    return 0;
}
#else
/* The original literal precedes the entry by four bytes. */
__asm__(".pushsection .text.strncmp,\"ax\",%progbits\n"
        ".balign 4\n.Lstrncmp_constant: .word 0x01010101\n.popsection");
__attribute__((naked, aligned(4), section(".text.strncmp")))
int strncmp(const char *left, const char *right, size_t count)
{
    __asm__ volatile(
        "cmp r2, #0x10\n"
        "blo .Lstrncmp_5f36\n"
        ".Lstrncmp_5e8c:\n"
        "ldrb.w r3, [r0], #1\n"
        "ldrb.w ip, [r1], #1\n"
        "sub.w r2, r2, #1\n"
        "cmp r3, #1\n"
        "it hs\n"
        "cmphs r3, ip\n"
        "bne .Lstrncmp_5f4e\n"
        "tst.w r0, #3\n"
        "bne .Lstrncmp_5e8c\n"
        "push {r4, r5, r6, r7, lr}\n"
        "add r7, sp, #0xc\n"
        "push.w {r8, sl}\n"
        "ldr.w lr, .Lstrncmp_constant\n"
        "and.w sl, r1, #3\n"
        "bic.w r1, r1, #3\n"
        "lsl.w sl, sl, #3\n"
        "rsb.w r6, sl, #0x20\n"
        "sub.w r2, r2, r6, lsr #3\n"
        "ldr.w r8, [r1], #4\n"
        "lsr.w r3, lr, r6\n"
        "orr.w r8, r8, r3\n"
        "sub.w r3, r8, lr\n"
        "bic.w r3, r3, r8\n"
        "tst.w r3, lr, lsl #7\n"
        "lsr.w r5, r8, sl\n"
        "bne .Lstrncmp_5f26\n"
        "nop \n"
        "nop \n"
        "nop \n"
        "nop \n"
        "nop \n"
        "nop \n"
        ".Lstrncmp_5ef0:\n"
        "subs r2, r2, #4\n"
        "blo .Lstrncmp_5f22\n"
        "ldr.w r8, [r1], #4\n"
        "sub.w r3, r8, lr\n"
        "bic.w r3, r3, r8\n"
        "ldr.w ip, [r0], #4\n"
        "tst.w r3, lr, lsl #7\n"
        "lsl.w r4, r8, r6\n"
        "orr.w r4, r4, r5\n"
        "it eq\n"
        "cmpeq ip, r4\n"
        "lsr.w r5, r8, sl\n"
        "beq .Lstrncmp_5ef0\n"
        "sub.w r0, r0, #4\n"
        "sub.w r1, r1, #4\n"
        ".Lstrncmp_5f22:\n"
        "add.w r2, r2, #4\n"
        ".Lstrncmp_5f26:\n"
        "sub.w r1, r1, r6, lsr #3\n"
        "add.w r2, r2, r6, lsr #3\n"
        "pop.w {r8, sl}\n"
        "pop.w {r4, r5, r6, r7, lr}\n"
        ".Lstrncmp_5f36:\n"
        "subs r2, r2, #1\n"
        "itt lo\n"
        "movlo r0, #0\n"
        "bxlo lr\n"
        "ldrb.w r3, [r0], #1\n"
        "ldrb.w ip, [r1], #1\n"
        "cmp r3, #1\n"
        "it hs\n"
        "cmphs r3, ip\n"
        "beq .Lstrncmp_5f36\n"
        ".Lstrncmp_5f4e:\n"
        "sub.w r0, r3, ip\n"
        "bx lr\n"
    );
}
#endif

/* @provenance RVA 0x6AFE strrchr */
#ifdef BTP_HOST
char *strrchr(const char *s, int c)
{
    size_t end = 0;

    while (s[end++])
        ;
    do {
        --end;
        if ((uint8_t)s[end] == (uint8_t)c)
            return (char *)s + end;
    } while (end != 0);
    return NULL;
}
#else
__attribute__((naked, aligned(4)))
char *strrchr(const char *s, int c)
{
    __asm__ volatile(
        "movs r2, #0\n"
        ".Lstrrchr_6b00:\n"
        "ldrb r3, [r0, r2]\n"
        "adds r2, #1\n"
        "cmp r3, #0\n"
        "bne .Lstrrchr_6b00\n"
        "subs r2, #1\n"
        "uxtb r1, r1\n"
        ".Lstrrchr_6b0c:\n"
        "ldrb r3, [r0, r2]\n"
        "cmp r3, r1\n"
        "itt eq\n"
        "addeq r0, r2\n"
        "bxeq lr\n"
        "subs r2, #1\n"
        "adds r3, r2, #1\n"
        "itt eq\n"
        "moveq r0, #0\n"
        "bxeq lr\n"
        "b .Lstrrchr_6b0c\n"
    );
}
#endif

/* @provenance RVA 0x6AB8 strlcpy */
#ifdef BTP_HOST
size_t strlcpy(char *dst, const char *src, size_t size)
{
    size_t copied = 0;

    if (size == 0)
        return strlen(src);
    while (copied != size - 1 && src[copied]) {
        dst[copied] = src[copied];
        ++copied;
    }
    dst[copied] = '\0';
    return copied + strlen(src + copied);
}
#else
__attribute__((naked, aligned(4)))
size_t strlcpy(char *dst, const char *src, size_t size)
{
    __asm__ volatile(
        "push {r4, r6, r7, lr}\n"
        "add r7, sp, #8\n"
        "cbz r2, .Lstrlcpy_6ad0\n"
        "subs r4, r2, #1\n"
        "movs r2, #0\n"
        ".Lstrlcpy_6ac2:\n"
        "cmp r4, r2\n"
        "beq .Lstrlcpy_6adc\n"
        "ldrb r3, [r1, r2]\n"
        "cbz r3, .Lstrlcpy_6ada\n"
        "strb r3, [r0, r2]\n"
        "adds r2, #1\n"
        "b .Lstrlcpy_6ac2\n"
        ".Lstrlcpy_6ad0:\n"
        "mov r0, r1\n"
        "pop.w {r4, r6, r7, lr}\n"
        "b.w strlen\n"
        ".Lstrlcpy_6ada:\n"
        "mov r4, r2\n"
        ".Lstrlcpy_6adc:\n"
        "movs r2, #0\n"
        "strb r2, [r0, r4]\n"
        "adds r0, r1, r4\n"
        "bl strlen\n"
        "add r0, r4\n"
        "pop {r4, r6, r7, pc}\n"
    );
}
#endif

/* @provenance RVA 0x6A60 strlcat */
#ifdef BTP_HOST
size_t strlcat(char *dst, const char *src, size_t size)
{
    size_t dlen = strnlen(dst, size);
    size_t copied = 0;

    if (size == 0 || dlen == size)
        return dlen + strlen(src);
    while (copied != size - dlen - 1 && src[copied]) {
        dst[dlen + copied] = src[copied];
        ++copied;
    }
    dst[dlen + copied] = '\0';
    return dlen + copied + strlen(src + copied);
}
#else
__attribute__((naked, aligned(4)))
size_t strlcat(char *dst, const char *src, size_t size)
{
    __asm__ volatile(
        "push {r4, r5, r6, r7, lr}\n"
        "add r7, sp, #0xc\n"
        "str.w r8, [sp, #-0x4]!\n"
        "mov r4, r1\n"
        "mov r1, r2\n"
        "mov r6, r2\n"
        "mov r5, r0\n"
        "bl strnlen\n"
        "mov r8, r0\n"
        "cbz r6, .Lstrlcat_6a7c\n"
        "cmp r6, r8\n"
        "bne .Lstrlcat_6a86\n"
        ".Lstrlcat_6a7c:\n"
        "mov r0, r4\n"
        "bl strlen\n"
        "add r0, r8\n"
        "b .Lstrlcat_6ab2\n"
        ".Lstrlcat_6a86:\n"
        "mvn.w r1, r8\n"
        "add.w r0, r5, r8\n"
        "movs r2, #0\n"
        "add r1, r6\n"
        ".Lstrlcat_6a92:\n"
        "cmp r1, r2\n"
        "beq .Lstrlcat_6aa2\n"
        "ldrb r3, [r4, r2]\n"
        "cbz r3, .Lstrlcat_6aa0\n"
        "strb r3, [r0, r2]\n"
        "adds r2, #1\n"
        "b .Lstrlcat_6a92\n"
        ".Lstrlcat_6aa0:\n"
        "mov r1, r2\n"
        ".Lstrlcat_6aa2:\n"
        "movs r2, #0\n"
        "add.w r5, r1, r8\n"
        "strb r2, [r0, r1]\n"
        "adds r0, r4, r1\n"
        "bl strlen\n"
        "add r0, r5\n"
        ".Lstrlcat_6ab2:\n"
        "ldr.w r8, [sp], #4\n"
        "pop {r4, r5, r6, r7, pc}\n"
    );
}
#endif
