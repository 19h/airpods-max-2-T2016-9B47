#!/usr/bin/env python3
"""Independent finite DPD command/capture-IRQ execution in QEMU Normal memory, without memory hooks.

The reference ROM remains unmodified. Peripheral locations are ordinary RAM
on this Cortex-A15 fixture; this does not establish physical peripheral behavior
or reproduce the changing-read response model used by arm_semantics.py.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

from arm_semantics import ROOT, REFERENCE_SHA256
from qemu_formatter import ENTRY

PROGRAM = r"""
#include <stdint.h>
#include <stddef.h>
#include "btp/dpd.h"
void write_text(const char *);
void enable_memory(uint32_t *);
static uint32_t sections[4096] __attribute__((aligned(16384)));
static unsigned char scratch[0x1000], snapshot[0x20000];
_Static_assert(0xD178 + sizeof scratch + 0xBC0 + 0x300 + 6 * 4 <= sizeof snapshot, "snapshot capacity");
static unsigned tested, failed;
static void number(unsigned n) {
    char text[12]; unsigned pos = 11; text[pos] = 0;
    do { text[--pos] = (char)('0' + n % 10); n /= 10; } while (n);
    write_text(text + pos);
}
static void put(uintptr_t a, uint32_t value) { *(volatile uint32_t *)a = value; }
static void seed(uintptr_t command, uintptr_t length, uint32_t capacity,
                 uint32_t opcode, uint32_t trigger, uint32_t mode) {
    for (unsigned i = 0; i < 0xD178; ++i) *(volatile unsigned char *)(0x4D280000 + i) = 0;
    for (unsigned i = 0; i < sizeof scratch; ++i) scratch[i] = 0xA5;
    for (unsigned i = 0; i < 0x300; ++i) *(volatile unsigned char *)(0x4D160000 + i) = 0xA5;
    for (unsigned i = 0; i < 0xBC0; ++i) *(volatile unsigned char *)(0x4D148000 + i) = (unsigned char)(i * 37 + 11);
    put(0x4D28AAA8, 0x12345678); put(0x4D28AAAC, 752);
    put(0x4D28AA98, trigger); put(0x4D28B670, mode);
    put(0x4D141000, 0xA5A55A5A); put(0x4D14101C, 0);
    put(0x4D141604, 10); put(0x4D141608, 0x12345678);
    put(0xE000E018, 0x123456); put(0xE000E180, 0xA5A55A5A);
    if (length) put(length, capacity);
    put(command, opcode); put(command + 4, 2); put(command + 8, 7); put(command + 12, 3);
}
static unsigned capture(unsigned compare) {
    unsigned pos = 0, match = 1;
    const uintptr_t bases[] = {0x4D280000, (uintptr_t)scratch, 0x4D148000, 0x4D160000,
                              0x4D141000, 0x4D14101C, 0x4D141604, 0x4D141608, 0xE000E018, 0xE000E180};
    const unsigned sizes[] = {0xD178, sizeof scratch, 0xBC0, 0x300, 4, 4, 4, 4, 4, 4};
    for (unsigned region = 0; region < 10; ++region)
        for (unsigned i = 0; i < sizes[region]; ++i) {
            unsigned char value = *(volatile unsigned char *)(bases[region] + i);
            if (compare) { if (snapshot[pos] != value) match = 0; }
            else snapshot[pos] = value;
            ++pos;
        }
    return match;
}
static void check(uint32_t opcode, uintptr_t command, uintptr_t output, uintptr_t length,
                  uint32_t capacity, unsigned irq, uint32_t trigger, uint32_t mode) {
    typedef uint32_t (*command_fn)(const uint32_t *, uint32_t *, int *);
    typedef void (*irq_fn)(void);
    command_fn original = (command_fn)(uintptr_t)0x3685;
    seed(command, length, capacity, opcode, trigger, mode);
    uint32_t a = 0, b = 0;
    if (irq) ((irq_fn)(uintptr_t)0x3599)();
    else a = original((const uint32_t *)command, (uint32_t *)output, (int *)length);
    capture(0);
    seed(command, length, capacity, opcode, trigger, mode);
    if (irq) dpd_capture_done_isr();
    else b = dpd_handle_command((const uint32_t *)command, (uint32_t *)output, (int *)length);
    unsigned match = capture(1) && a == b;
    ++tested;
    if (!match) { ++failed; write_text("DPD_DIVERGENCE case="); number(tested); write_text("\n"); }
}
int test(void) {
    for (unsigned i = 0; i < 4096; ++i) sections[i] = (i << 20) | 0x1C02;
    /* Redirect SCS virtual addresses to ordinary RAM on the Cortex-A15 fixture. */
    sections[0xE00] = 0x4D100000 | 0x1C02;
    enable_memory(sections);
    const uintptr_t commands[] = {(uintptr_t)scratch + 0x100, 0x4D160000, 0x4D28AA84,
                                  0x4D28AA88, 0x4D28AAA4, 0x4D28B664};
    const uint32_t opcodes[] = {0, 1, 2, 3, 0xFFFFFFFF};
    const uint32_t capacities[] = {0, 7, 8, 0xFFFFFFFF};
    for (unsigned op = 0; op < 5; ++op)
    for (unsigned c = 0; c < 6; ++c)
    for (unsigned out = 0; out < 2; ++out)
    for (unsigned len = 0; len < 2; ++len)
    for (unsigned n = 0; n < 4; ++n)
        check(opcodes[op], commands[c], out ? 0x4D160100 : (uintptr_t)scratch + 0x400,
              len ? 0x4D160200 : (uintptr_t)scratch + 0x600, capacities[n], 0, 0, 3);
    check(3, commands[0], 0, (uintptr_t)scratch + 0x600, 8, 0, 0, 3);
    check(3, commands[0], (uintptr_t)scratch + 0x400, 0, 8, 0, 0, 3);
    check(3, commands[0], 0x4D28AAAC, (uintptr_t)scratch + 0x600, 8, 0, 0, 3);
    check(3, commands[0], (uintptr_t)scratch + 0x400, 0x4D28AAA8, 8, 0, 0, 3);
    const uint32_t triggers[] = {0, 1, 0xFFFFFFFF}, modes[] = {0, 1, 7, 15};
    for (unsigned g = 0; g < 3; ++g) for (unsigned m = 0; m < 4; ++m)
        check(0, commands[0], (uintptr_t)scratch + 0x400, (uintptr_t)scratch + 0x600,
              8, 1, triggers[g], modes[m]);
    write_text("CASES "); number(tested); write_text(" FAILED "); number(failed); write_text("\n");
    return failed != 0;
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    reference = ROOT.parent / 'phyb'
    if hashlib.sha256(reference.read_bytes()).hexdigest() != REFERENCE_SHA256:
        raise ValueError('reference firmware hash mismatch')
    (build / 'dpd-qemu.c').write_text(PROGRAM)
    (build / 'dpd-qemu.S').write_text(ENTRY)
    (build / 'dpd-qemu.ld').write_text('''ENTRY(start)
SECTIONS {
 . = 0x41000000;
 .text : { *(.text*) *(.rodata*) }
 .data : { *(.data*) }
 .bss (NOLOAD) : { *(.bss*) }
 .state.dpd 0x4D28AA88 (NOLOAD) : { *(.state.dpd) }
}
''')
    elf = build / 'dpd-qemu.elf'
    command = ['clang', '-target', 'armv7em-none-eabi', '-mcpu=cortex-m4', '-O1',
               '-fno-builtin', '-ffreestanding', '-nostdlib', '-fuse-ld=lld', '-Iinclude',
               '-Wl,--gc-sections', '-Wl,-T,' + str(build / 'dpd-qemu.ld'), str(build / 'dpd-qemu.S'),
               str(build / 'dpd-qemu.c'), str(build / 'arm/w3/dpd.o'), str(build / 'arm/w3/clock.o'),
               str(build / 'arm/rtkit/irq.o'), str(build / 'arm/w3/trace.o'), str(build / 'arm/libc/string.o'), '-o', str(elf)]
    subprocess.run(command, cwd=ROOT, check=True)
    qemu = ['qemu-system-arm', '-M', 'virt', '-cpu', 'cortex-a15', '-m', '512M',
            '-nographic', '-semihosting-config', 'enable=on,target=native', '-device',
            f'loader,file={reference},addr=0,force-raw=on', '-device', f'loader,file={elf},cpu-num=0']
    try:
        run = subprocess.run(qemu, capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired as exc:
        (build / 'dpd-qemu-timeout.txt').write_bytes((exc.stdout or b'') + (exc.stderr or b''))
        run = subprocess.CompletedProcess(qemu, 124, '', 'QEMU timeout; see dpd-qemu-timeout.txt')
    summary = re.search(r'CASES (\d+) FAILED (\d+)', run.stdout + run.stderr)
    expected_count = 5 * 6 * 2 * 2 * 4 + 4 + 3 * 4
    executed = int(summary[1]) if summary else None
    failures = int(summary[2]) if summary else None
    complete = executed == expected_count and failures == 0 and run.returncode == 0
    report = {'scope': __doc__, 'reference_sha256': REFERENCE_SHA256,
              'assumptions': ['Cortex-A15 scalar Thumb execution; nominal device apertures are Normal writable RAM',
                              'SCS virtual section 0xE0000000 maps to Normal RAM at physical 0x4D100000; no real NVIC/SysTick behavior',
                              'device progress, interrupts and concurrent aperture writes are absent; no physical-peripheral proof'],
              'compiled_case_count': expected_count, 'executed_case_count': executed,
              'failures': failures, 'complete': complete, 'exit_code': run.returncode,
              'comparison_elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
              'dpd_object_sha256': hashlib.sha256((build / 'arm/w3/dpd.o').read_bytes()).hexdigest(),
              'build_command': command, 'qemu_command': qemu, 'stdout': run.stdout, 'stderr': run.stderr}
    (build / 'dpd-qemu.json').write_text(json.dumps(report, indent=2) + '\n')
    print(run.stdout + run.stderr, end='')
    return 0 if complete else 1


if __name__ == '__main__':
    raise SystemExit(main())
