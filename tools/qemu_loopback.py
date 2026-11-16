#!/usr/bin/env python3
"""Independent finite loopback execution in QEMU Normal memory, without memory hooks.

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
#include "btp/loopback.h"
#include "btp/nvv.h"
void write_text(const char *);
void enable_memory(uint32_t *);
struct nvv_tables nvv __attribute__((section(".state.nvv")));
static uint32_t sections[4096] __attribute__((aligned(16384)));
static unsigned char scratch[0x6000];
static unsigned char snapshot[0x30000];
static unsigned tested, failed;
_Static_assert(0x400u + 0x20140u + 0x3EE0u + sizeof scratch + 0x34u + 8u <= sizeof snapshot,
               "snapshot covers every compared byte");
/* These cases use valid readback arguments and active=1. Unexpected errors
   are a fixture failure; no original error-path instructions are replaced. */
void trace_log_error(uint32_t a, uint32_t b, uint32_t c) {
    ++failed;
    write_text("UNEXPECTED_ERROR_PATH\n");
}
static void number(unsigned n) {
    char text[12]; unsigned pos = 11; text[pos] = 0;
    do { text[--pos] = (char)('0' + n % 10); n /= 10; } while (n);
    write_text(text + pos);
}
static void put(uintptr_t address, uint32_t value) { *(volatile uint32_t *)address = value; }
static void seed(unsigned op, uintptr_t stats, uintptr_t control, unsigned count,
                 unsigned mode, unsigned profile) {
    for (unsigned i = 0; i < 0x400; i += 4) put(0x4D000000 + i, 0xA5A55A5A);
    for (unsigned i = 0; i < 0x20140; i += 4) put(0x4D140000 + i, 0xA5A55A5A);
    for (unsigned i = 0; i < 0x3EE0; i += 4) put(0x4D289298 + i, 0);
    for (unsigned i = 0; i < sizeof scratch; ++i) scratch[i] = 0;
    put(0x4D2895B4, stats); put(0x4D2895B8, control);
    put(stats, count);
    for (unsigned i = 0; i < 4; ++i) {
        put(stats + 4 + 4 * i, i % 2 ? 0x80000000 : 0x7FFFFFFF);
        put(stats + 0x14 + 4 * i, 100); put(stats + 0x24 + 4 * i, 0xFFFFFF9C);
    }
    put(control, mode); put(control + 4, 0);
    *(volatile uint8_t *)0x4D2899A8 = 1;
    put((uintptr_t)scratch + 0x100, mode); put((uintptr_t)scratch + 0x104, 0);
    static const uint32_t results[4][2] = {{0,0}, {0x7FFFFF,0x800000}, {1,0xFFFFFF}, {0xFFFFFFFF,0xFFFFFFFF}};
    for (unsigned i = 0; i < 4; ++i) put(0x4D1411E0 + 4 * i, results[profile][i % 2]);
}
static uint32_t run(unsigned original, unsigned op) {
    if (op == 0) {
        typedef btp_status_t (*fn)(void *, uint32_t *, uint32_t);
        fn call = original ? (fn)(uintptr_t)0x48DD : loopback_read_stats;
        return call(scratch + 0x1000, (uint32_t *)(scratch + 0x1800), 0x34);
    }
    if (op == 1 || op == 2) {
        typedef void (*fn)(void);
        fn call = original ? (fn)(uintptr_t)(op == 1 ? 0x44ED : 0x4581)
                           : (op == 1 ? loopback_corr0_done_isr : loopback_corr1_done_isr);
        call(); return 0;
    }
    if (op == 3) {
        typedef btp_status_t (*fn)(const uint32_t *);
        fn call = original ? (fn)(uintptr_t)0x46A5 : loopback_configure;
        return call((const uint32_t *)(scratch + 0x100));
    }
    typedef void (*fn)(unsigned, bool);
    fn call = original ? (fn)(uintptr_t)0x43A9 : loopback_setup_tx_measurement;
    call(0, 0); return 0;
}
static const uintptr_t bases[] = {0x4D000000, 0x4D140000, 0x4D289298};
static const unsigned sizes[] = {0x400, 0x20140, 0x3EE0};
static unsigned capture(unsigned compare, uintptr_t stats, uintptr_t control) {
    unsigned pos = 0, match = 1;
    for (unsigned region = 0; region < 6; ++region) {
        uintptr_t base = region < 3 ? bases[region] : region == 3 ? (uintptr_t)scratch : region == 4 ? stats : control;
        unsigned size = region < 3 ? sizes[region] : region == 3 ? sizeof scratch : region == 4 ? 0x34 : 8;
        for (unsigned i = 0; i < size; ++i) {
            unsigned char value = *(volatile unsigned char *)(base + i);
            if (compare) { if (snapshot[pos] != value) match = 0; }
            else snapshot[pos] = value;
            ++pos;
        }
    }
    return match;
}
int test(void) {
    for (unsigned i = 0; i < 4096; ++i) sections[i] = (i << 20) | 0x1C02;
    enable_memory(sections);
    static const unsigned counts[] = {0, 1, 254, 255};
    static const unsigned modes[] = {0, 2, 3, 0x03021F03};
    for (unsigned op = 0; op < 5; ++op)
    for (unsigned s = 0; s < 2; ++s)
    for (unsigned c = 0; c < 2; ++c)
    for (unsigned n = 0; n < 4; ++n)
    for (unsigned m = 0; m < 4; ++m)
    for (unsigned profile = 0; profile < 4; ++profile) {
        uintptr_t stats = s ? 0x4D160000 : (uintptr_t)scratch + 0x3000;
        uintptr_t control = c ? 0x4D160100 : (uintptr_t)scratch + 0x3100;
        seed(op, stats, control, counts[n], modes[m], profile);
        uint32_t a = run(1, op); capture(0, stats, control);
        seed(op, stats, control, counts[n], modes[m], profile);
        uint32_t b = run(0, op);
        unsigned match = capture(1, stats, control) && a == b;
        ++tested;
        if (!match) { ++failed; write_text("LOOPBACK_DIVERGENCE case="); number(tested); write_text("\n"); }
    }
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
    (build / 'loopback-qemu.c').write_text(PROGRAM)
    (build / 'loopback-qemu.S').write_text(ENTRY)
    (build / 'loopback-qemu.ld').write_text('''ENTRY(start)
SECTIONS {
 . = 0x41000000;
 .text : { *(.text*) *(.rodata*) }
 .data : { *(.data*) }
 .bss (NOLOAD) : { *(.bss*) }
 .image.cells 0x4D2895B4 : { *(.image.data.loopback_stats_cell) *(.image.data.loopback_control_cell) }
 .state.loopback 0x4D28996C (NOLOAD) : { *(.state.loopback) }
 .state.lb_saved 0x4D28CF1C (NOLOAD) : { *(.state.lb_saved) }
 .state.nvv 0x4D28CF48 (NOLOAD) : { *(.state.nvv) }
}
''')
    elf = build / 'loopback-qemu.elf'
    command = ['clang', '-target', 'armv7em-none-eabi', '-mcpu=cortex-m4', '-O1',
               '-fno-builtin', '-ffreestanding', '-nostdlib', '-fuse-ld=lld', '-Iinclude',
               '-Wl,-T,' + str(build / 'loopback-qemu.ld'), str(build / 'loopback-qemu.S'),
               str(build / 'loopback-qemu.c'), str(build / 'arm/w3/loopback.o'),
               str(build / 'arm/libc/string.o'), '-o', str(elf)]
    subprocess.run(command, cwd=ROOT, check=True)
    qemu = ['qemu-system-arm', '-M', 'virt', '-cpu', 'cortex-a15', '-m', '512M',
            '-nographic', '-semihosting-config', 'enable=on,target=native', '-device',
            f'loader,file={reference},addr=0,force-raw=on', '-device', f'loader,file={elf},cpu-num=0']
    try:
        run = subprocess.run(qemu, capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired as exc:
        (build / 'loopback-qemu-timeout.txt').write_bytes((exc.stdout or b'') + (exc.stderr or b''))
        run = subprocess.CompletedProcess(qemu, 124, '', 'QEMU timeout; see loopback-qemu-timeout.txt')
    summary = re.search(r'CASES (\d+) FAILED (\d+)', run.stdout + run.stderr)
    expected_count = 5 * 2 * 2 * 4 * 4 * 4
    executed = int(summary[1]) if summary else None
    failures = int(summary[2]) if summary else None
    complete = executed == expected_count and failures == 0 and run.returncode == 0
    report = {'scope': __doc__, 'reference_sha256': REFERENCE_SHA256,
              'assumptions': ['Cortex-A15 scalar Thumb execution with Normal writable memory at nominal device addresses',
                              'calibration pointers are zero; active byte is 1; readback arguments are valid',
                              'pointer cells installed directly; accepted NVV record installation is covered separately by arm_semantics.py',
                              'interrupts and changing peripheral responses are absent; no error-path equivalence claim'],
              'compiled_case_count': expected_count, 'executed_case_count': executed,
              'failures': failures, 'complete': complete, 'exit_code': run.returncode,
              'comparison_elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
              'loopback_object_sha256': hashlib.sha256((build / 'arm/w3/loopback.o').read_bytes()).hexdigest(),
              'build_command': command, 'qemu_command': qemu, 'stdout': run.stdout, 'stderr': run.stderr}
    (build / 'loopback-qemu.json').write_text(json.dumps(report, indent=2) + '\n')
    print(run.stdout + run.stderr, end='')
    return 0 if complete else 1


if __name__ == '__main__':
    raise SystemExit(main())
