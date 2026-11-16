#!/usr/bin/env python3
"""Compare ARM formatter and memory routines in QEMU with the original ROM at address zero.

The virt board supplies ordinary RAM at 0x4D289840 for the original canary.
This exercises scalar Thumb code on Cortex-A15, not M-profile exception entry
or actual PHY hardware. No reference instructions or literal pools are patched.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

from arm_semantics import ROOT, TEST, REFERENCE_SHA256, format_cases

ENTRY = r"""
.syntax unified
.thumb
.text
.thumb_func
.global start
start:
    ldr sp, =0x41200000
    bl test
finish:
    sub sp, #8
    str r0, [sp, #4]
    ldr r0, =0x20026
    str r0, [sp]
    mov r1, sp
    movs r0, #0x20
    svc #0xab
    b .
.thumb_func
.global __stack_chk_fail
__stack_chk_fail:
    movs r0, #99
    b finish
.thumb_func
.global call_words
call_words:
    push {r4-r7, lr}
    sub sp, #68
    mov r4, r0
    mov r5, r1
    movs r6, #0
1:
    ldr r7, [r5, r6]
    str r7, [sp, r6]
    adds r6, #4
    cmp r6, #64
    blo 1b
    ldr r0, [r5, #-16]
    ldr r1, [r5, #-12]
    ldr r2, [r5, #-8]
    ldr r3, [r5, #-4]
    blx r4
    add sp, #68
    pop {r4-r7, pc}
.thumb_func
.global enable_memory
/* Identity sections mark test memory Normal, permitting unaligned loads. */
enable_memory:
    movs r1, #0
    mcr p15, 0, r1, c2, c0, 2
    mcr p15, 0, r0, c2, c0, 0
    mvn r1, #0
    mcr p15, 0, r1, c3, c0, 0
    dsb
    mrc p15, 0, r0, c1, c0, 0
    orr r0, r0, #1
    mcr p15, 0, r0, c1, c0, 0
    isb
    bx lr
.thumb_func
.global write_text
write_text:
    mov r1, r0
    movs r0, #4
    svc #0xab
    bx lr
"""

C_PREFIX = r"""
#include <stdint.h>
#include <stddef.h>
int snprintf(char *, size_t, const char *, ...);
void *memmove(void *, const void *, size_t);
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
size_t strlen(const char *);
size_t strnlen(const char *, size_t);
int strncmp(const char *, const char *, size_t);
char *strrchr(const char *, int);
size_t strlcpy(char *, const char *, size_t);
size_t strlcat(char *, const char *, size_t);
uint32_t call_words(uintptr_t, const uint32_t *);
void write_text(const char *);
void enable_memory(uint32_t *);
static uint32_t sections[4096] __attribute__((aligned(16384)));
uint32_t __stack_chk_guard;
static const char payload[] = "abcdefg";
struct format_case { const char *format; uint32_t words[16]; };
static const struct format_case cases[] = {
"""

C_SUFFIX = r"""
};
static char original_buffer[128], rebuilt_buffer[128];
static unsigned char original_memory[640], rebuilt_memory[640];
static unsigned char original_large[0xA000], rebuilt_large[0xA000];
static void number(uint32_t n) {
    char text[12];
    unsigned pos = 11;
    text[pos] = 0;
    do { text[--pos] = (char)('0' + n % 10); n /= 10; } while (n);
    write_text(text + pos);
}
static void string_result(int match, unsigned *tested, unsigned *failed, const char *name) {
    ++*tested;
    if (!match) {
        ++*failed;
        write_text(name); write_text(" case="); number(*tested); write_text("\n");
    }
}
static void string_tests(unsigned *tested, unsigned *failed) {
    typedef size_t (*length_fn)(const char *);
    typedef size_t (*bounded_fn)(const char *, size_t);
    typedef int (*compare_fn)(const char *, const char *, size_t);
    typedef char *(*search_fn)(const char *, int);
    typedef size_t (*copy_fn)(char *, const char *, size_t);
    static const unsigned bounds[] = {0, 1, 2, 15, 16, 17, 64, 80};
    static const int chars[] = {0, 'a', 0x80, 0x1FF, -1};
    for (unsigned length = 0; length <= 64; ++length) {
        for (unsigned align = 0; align < 4; ++align) {
            char *a = (char *)original_memory + 32 + align;
            char *b = (char *)rebuilt_memory + 32 + align;
            for (unsigned j = 0; j < 640; ++j)
                original_memory[j] = rebuilt_memory[j] = 0xA5;
            for (unsigned j = 0; j < length; ++j)
                a[j] = b[j] = (char)(j % 3 == 0 ? 'a' : j % 3 == 1 ? 0x80 : 0xFF);
            a[length] = b[length] = 0;
            string_result(((length_fn)(uintptr_t)0x5E01)(a) == strlen(b),
                          tested, failed, "STRLEN_DIVERGENCE");
            for (unsigned n = 0; n < sizeof bounds / sizeof bounds[0]; ++n)
                string_result(((bounded_fn)(uintptr_t)0x6AEB)(a, bounds[n]) == strnlen(b, bounds[n]),
                              tested, failed, "STRNLEN_DIVERGENCE");
            for (unsigned n = 0; n < sizeof chars / sizeof chars[0]; ++n) {
                char *pa = ((search_fn)(uintptr_t)0x6AFF)(a, chars[n]);
                char *pb = strrchr(b, chars[n]);
                string_result((pa ? pa - a : -1) == (pb ? pb - b : -1),
                              tested, failed, "STRRCHR_DIVERGENCE");
            }
            for (unsigned other = 0; other < 4; ++other) {
                for (unsigned n = 0; n < sizeof bounds / sizeof bounds[0]; ++n) {
                    for (unsigned variant = 0; variant < 4; ++variant) {
                        char *aa = (char *)original_memory + 192 + other;
                        char *bb = (char *)rebuilt_memory + 192 + other;
                        for (unsigned j = 0; j <= length; ++j) aa[j] = bb[j] = a[j];
                        if (length && variant == 1) aa[0] = bb[0] = 0x7F;
                        if (length && variant == 2) aa[length - 1] = bb[length - 1] = 1;
                        if (variant == 3) aa[length] = bb[length] = 'b';
                        aa[length + 1] = bb[length + 1] = 0;
                        string_result(((compare_fn)(uintptr_t)0x5E89)(a, aa, bounds[n])
                                      == strncmp(b, bb, bounds[n]), tested, failed, "STRNCMP_DIVERGENCE");
                    }
                    for (unsigned which = 0; which < 2; ++which) {
                        char *da = (char *)original_memory + 352 + other;
                        char *db = (char *)rebuilt_memory + 352 + other;
                        for (unsigned j = 352; j < 640; ++j)
                            original_memory[j] = rebuilt_memory[j] = 0xA5;
                        da[0] = db[0] = 'x'; da[1] = db[1] = 'y'; da[2] = db[2] = 0;
                        copy_fn ref = (copy_fn)(uintptr_t)(which ? 0x6A61 : 0x6AB9);
                        copy_fn impl = which ? strlcat : strlcpy;
                        int match = ref(da, a, bounds[n]) == impl(db, b, bounds[n]);
                        for (unsigned j = 0; j < 640; ++j)
                            if (original_memory[j] != rebuilt_memory[j]) match = 0;
                        string_result(match, tested, failed,
                                      which ? "STRLCAT_DIVERGENCE" : "STRLCPY_DIVERGENCE");
                    }
                }
            }
        }
    }
    /* Overlap has defined instruction behavior in this image, including forward
       propagation up to the capacity. Each fixture contains an accessible NUL. */
    for (unsigned which = 0; which < 2; ++which) {
        for (unsigned length = 0; length <= 32; ++length) {
            for (unsigned direction = 0; direction < 2; ++direction) {
                for (unsigned delta = 0; delta < 4; ++delta) {
                    for (unsigned n = 0; n < sizeof bounds / sizeof bounds[0]; ++n) {
                        for (unsigned j = 0; j < 640; ++j)
                            original_memory[j] = rebuilt_memory[j] = 0xA5;
                        unsigned src = 64 + (direction ? delta : 0);
                        unsigned dst = 64 + (direction ? 0 : delta);
                        for (unsigned j = 0; j < length; ++j)
                            original_memory[src + j] = rebuilt_memory[src + j] = (unsigned char)('a' + j % 26);
                        original_memory[src + length] = rebuilt_memory[src + length] = 0;
                        copy_fn ref = (copy_fn)(uintptr_t)(which ? 0x6A61 : 0x6AB9);
                        copy_fn impl = which ? strlcat : strlcpy;
                        int match = ref((char *)original_memory + dst, (char *)original_memory + src, bounds[n])
                                    == impl((char *)rebuilt_memory + dst, (char *)rebuilt_memory + src, bounds[n]);
                        for (unsigned j = 0; j < 640; ++j)
                            if (original_memory[j] != rebuilt_memory[j]) match = 0;
                        string_result(match, tested, failed,
                                      which ? "STRLCAT_OVERLAP_DIVERGENCE" : "STRLCPY_OVERLAP_DIVERGENCE");
                    }
                }
            }
        }
    }
}
int test(void) {
    static const unsigned capacities[] = {0, 1, 5, 80};
    unsigned failed = 0, tested = 0;
    for (unsigned i = 0; i < 4096; ++i) sections[i] = (i << 20) | 0x1C02;
    enable_memory(sections);
    string_tests(&tested, &failed);
    typedef void *(*move_fn)(void *, const void *, size_t);
    for (unsigned which = 0; which < 2; ++which) {
        move_fn original_move = (move_fn)(uintptr_t)(which ? 0x5C69 : 0x5C71);
        move_fn rebuilt_move = which ? memcpy : memmove;
        for (unsigned length = 0; length <= 256; ++length) {
            for (unsigned offset = 0; offset < 8; ++offset) {
                for (unsigned direction = 0; direction < 2; ++direction) {
                    for (unsigned j = 0; j < 640; ++j)
                        original_memory[j] = rebuilt_memory[j] = (unsigned char)(j * 37 + 11);
                    unsigned src = direction ? 32 : 32 + offset;
                    unsigned dst = direction ? 32 + offset : 32;
                    void *a = original_move(original_memory + dst, original_memory + src, length);
                    void *b = rebuilt_move(rebuilt_memory + dst, rebuilt_memory + src, length);
                    int match = a == original_memory + dst && b == rebuilt_memory + dst;
                    for (unsigned j = 0; j < 640; ++j)
                        if (original_memory[j] != rebuilt_memory[j]) match = 0;
                    ++tested;
                    if (!match) {
                        ++failed;
                        write_text("MEMORY_DIVERGENCE length="); number(length);
                        write_text(" offset="); number(offset); write_text("\n");
                    }
                }
            }
        }
    }
    typedef void *(*fill_fn)(void *, int, size_t);
    static const int fills[] = {0, 0x55, 0xFF, 0x1234};
    for (unsigned length = 0; length <= 256; ++length) {
        for (unsigned offset = 0; offset < 8; ++offset) {
            for (unsigned n = 0; n < sizeof fills / sizeof fills[0]; ++n) {
                for (unsigned j = 0; j < 640; ++j)
                    original_memory[j] = rebuilt_memory[j] = (unsigned char)(j * 37 + 11);
                void *a = ((fill_fn)(uintptr_t)0x5D81)(original_memory + 32 + offset, fills[n], length);
                void *b = memset(rebuilt_memory + 32 + offset, fills[n], length);
                int match = a == original_memory + 32 + offset && b == rebuilt_memory + 32 + offset;
                for (unsigned j = 0; j < 640; ++j)
                    if (original_memory[j] != rebuilt_memory[j]) match = 0;
                ++tested;
                if (!match) {
                    ++failed;
                    write_text("FILL_DIVERGENCE length="); number(length);
                    write_text(" offset="); number(offset); write_text("\n");
                }
            }
        }
    }
    for (unsigned which = 0; which < 2; ++which) {
        move_fn original_move = (move_fn)(uintptr_t)(which ? 0x5C69 : 0x5C71);
        move_fn rebuilt_move = which ? memcpy : memmove;
        for (unsigned offset = 0; offset < 8; ++offset) {
            for (unsigned direction = 0; direction < 2; ++direction) {
                for (unsigned j = 0; j < sizeof original_large; ++j)
                    original_large[j] = rebuilt_large[j] = (unsigned char)(j * 37 + 11);
                unsigned src = direction ? 32 : 32 + offset;
                unsigned dst = direction ? 32 + offset : 32;
                void *a = original_move(original_large + dst, original_large + src, 0x9900);
                void *b = rebuilt_move(rebuilt_large + dst, rebuilt_large + src, 0x9900);
                int match = a == original_large + dst && b == rebuilt_large + dst;
                for (unsigned j = 0; j < sizeof original_large; ++j)
                    if (original_large[j] != rebuilt_large[j]) match = 0;
                ++tested;
                if (!match) { ++failed; write_text("LARGE_COPY_DIVERGENCE\n"); }
            }
        }
    }
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        for (unsigned k = 0; k < sizeof capacities / sizeof capacities[0]; ++k) {
            uint32_t words[20] = {0};
            for (unsigned j = 0; j < 128; ++j)
                original_buffer[j] = rebuilt_buffer[j] = (char)0xA5;
            words[0] = (uintptr_t)original_buffer;
            words[1] = capacities[k];
            words[2] = (uintptr_t)cases[i].format;
            for (unsigned j = 0; j < 16; ++j) words[j+3] = cases[i].words[j];
            uint32_t original = call_words(0x69E7, words + 4);
            words[0] = (uintptr_t)rebuilt_buffer;
            uint32_t rebuilt = call_words((uintptr_t)snprintf, words + 4);
            int match = original == rebuilt;
            for (unsigned j = 0; j < 128; ++j)
                if (original_buffer[j] != rebuilt_buffer[j]) match = 0;
            ++tested;
            if (!match) {
                ++failed;
                write_text("DIVERGENCE "); number(i);
                write_text(" capacity="); number(capacities[k]);
                write_text(" original="); number(original);
                write_text(" rebuilt="); number(rebuilt); write_text("\n");
            }
        }
    }
    write_text("CASES "); number(tested);
    write_text(" FAILED "); number(failed); write_text("\n");
    return failed != 0;
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path)
    args = parser.parse_args()
    build = args.build.resolve()
    build.mkdir(parents=True, exist_ok=True)
    reference = ROOT.parent / "phyb"
    if hashlib.sha256(reference.read_bytes()).hexdigest() != REFERENCE_SHA256:
        raise ValueError("reference firmware hash mismatch")
    cases = list(format_cases())
    initializers = []
    for fmt, values in cases:
        words = ["(uintptr_t)payload" if n == TEST + 0x300 else f"0x{n & 0xFFFFFFFF:X}u"
                 for n in values]
        initializers.append("    {" + json.dumps(fmt) + ", {" + ", ".join(words) + "}},")
    (build / "formatter_cases.c").write_text(C_PREFIX + "\n".join(initializers) + C_SUFFIX)
    (build / "formatter_entry.S").write_text(ENTRY)
    (build / "formatter.ld").write_text("""ENTRY(start)
MEMORY { RAM (rwx) : ORIGIN = 0x41000000, LENGTH = 1M }
SECTIONS { .text : { *(.text*) *(.rodata*) } >RAM
 .data : { *(.data*) } >RAM
 .bss : { *(.bss*) } >RAM }
""")
    output = build / "formatter-qemu.elf"
    command = ["clang", "-target", "armv7em-none-eabi", "-mcpu=cortex-m4",
               "-O1", "-fno-builtin", "-ffreestanding", "-nostdlib", "-fuse-ld=lld",
               "-Wl,-T," + str(build / "formatter.ld"), str(build / "formatter_entry.S"),
               str(build / "formatter_cases.c")]
    command.extend(str(build / "arm/libc" / (name + ".o")) for name in ("printf", "string", "aeabi"))
    command.extend(["-o", str(output)])
    subprocess.run(command, check=True, cwd=ROOT)
    qemu = ["qemu-system-arm", "-M", "virt", "-cpu", "cortex-a15", "-m", "512M",
            "-nographic", "-semihosting-config", "enable=on,target=native", "-device",
            f"loader,file={reference},addr=0,force-raw=on", "-device",
            f"loader,file={output},cpu-num=0"]
    try:
        run = subprocess.run(qemu, capture_output=True, text=True, timeout=30)
    except subprocess.TimeoutExpired as exc:
        (build / "qemu-timeout.txt").write_bytes((exc.stdout or b"") + (exc.stderr or b""))
        run = subprocess.CompletedProcess(qemu, 124, "", "QEMU timeout; see qemu-timeout.txt")
    expected_count = (len(cases) * 4 + 2 * 257 * 8 * 2 + 257 * 8 * 4 + 2 * 8 * 2
                      + 65 * 4 * (1 + 8 + 5 + 4 * 8 * (4 + 2)) + 2 * 33 * 2 * 4 * 8)
    summary = re.search(r"CASES (\d+) FAILED (\d+)", run.stdout + run.stderr)
    executed_count = int(summary[1]) if summary else None
    failures = int(summary[2]) if summary else None
    complete = executed_count == expected_count and failures == 0
    report = {"scope": "finite scalar Thumb formatter/memory/string comparison; not exception/hardware proof",
              "reference_sha256": REFERENCE_SHA256,
              "compiled_case_count": expected_count,
              "executed_case_count": executed_count, "failures": failures, "complete": complete,
              "comparison_elf_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
              "build_command": command, "qemu_command": qemu,
              "exit_code": run.returncode, "stdout": run.stdout, "stderr": run.stderr}
    (build / "formatter-qemu.json").write_text(json.dumps(report, indent=2) + "\n")
    print(run.stdout + run.stderr, end="")
    return run.returncode if run.returncode else (0 if complete else 1)


if __name__ == "__main__":
    raise SystemExit(main())
