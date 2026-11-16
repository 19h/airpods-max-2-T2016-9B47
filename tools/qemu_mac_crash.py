#!/usr/bin/env python3
"""Finite crash-notification overlap comparison in QEMU Normal memory without hooks.

The unmodified reference ROM executes at address zero. Nominal device locations
are writable Cortex-A15 RAM; physical peripheral/interrupt behavior is not modeled.
Pointer cells are installed directly here; accepted NVV installation is checked
separately by arm_semantics.py. R0 carries the same descriptor address on both sides.
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
#include "btp/mac.h"
void write_text(const char *);
void enable_memory(uint32_t *);
static uint32_t sections[4096] __attribute__((aligned(16384)));
static unsigned char storage[0x1000], snapshot[0x1000];
_Static_assert(0x800 + 20 + 4 + 4 <= sizeof snapshot, "snapshot capacity");
static void number(unsigned n) {
    char text[12]; unsigned pos = 11; text[pos] = 0;
    do { text[--pos] = (char)('0' + n % 10); n /= 10; } while (n);
    write_text(text + pos);
}
static void put(uintptr_t a, uint32_t value) { *(volatile uint32_t *)a = value; }
static void seed(uintptr_t box, uintptr_t desc, uint32_t doorbell) {
    for (unsigned j = 0; j < 0x800; ++j) *(volatile unsigned char *)(box + j) = 0xA5;
    for (unsigned j = 0; j < 20; ++j) *(volatile unsigned char *)(desc + j) = (unsigned char)(j * 37 + 11);
    put(0x4D28929C, box);
    put(0x4D154C60, doorbell);
}
static unsigned capture(unsigned compare, uintptr_t box, uintptr_t desc) {
    unsigned pos = 0, match = 1;
    const uintptr_t bases[] = {box, desc, 0x4D28929C, 0x4D154C60};
    const unsigned sizes[] = {0x800, 20, 4, 4};
    for (unsigned region = 0; region < 4; ++region)
        for (unsigned j = 0; j < sizes[region]; ++j) {
            unsigned char byte = *(volatile unsigned char *)(bases[region] + j);
            if (compare) { if (snapshot[pos] != byte) match = 0; }
            else snapshot[pos] = byte;
            ++pos;
        }
    return match;
}
int test(void) {
    for (unsigned i = 0; i < 4096; ++i) sections[i] = (i << 20) | 0x1C02;
    enable_memory(sections);
    unsigned tested = 0, failed = 0;
    const uintptr_t boxes[] = {0x4D280000, 0x4D160000, (uintptr_t)storage, 0x4D28929C - 0x5C1};
    const uint32_t doorbells[] = {0, 0xA5A55A5A, 0xFFFFFFFF};
    typedef void (*notify_fn)(const struct crash_descriptor *);
    notify_fn original = (notify_fn)(uintptr_t)0x4ED;
    for (unsigned box = 0; box < 4; ++box)
    for (unsigned align = 0; align < 4; ++align)
    for (unsigned choice = 0; choice < 45; ++choice)
    for (unsigned d = 0; d < 3; ++d) {
        uintptr_t base = boxes[box] + align;
        uintptr_t desc = choice < 41 ? base + 0x5C1 + (int)choice - 20
                                    : 0x4D161000 + choice - 41;
        seed(base, desc, doorbells[d]);
        original((const struct crash_descriptor *)desc); capture(0, base, desc);
        seed(base, desc, doorbells[d]);
        mac_send_crash_notification((const struct crash_descriptor *)desc);
        unsigned match = capture(1, base, desc);
        ++tested;
        if (!match) { ++failed; write_text("MAC_CRASH_DIVERGENCE case="); number(tested); write_text("\n"); }
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
    (build / 'mac-crash-qemu.c').write_text(PROGRAM)
    (build / 'mac-crash-qemu.S').write_text(ENTRY)
    (build / 'mac-crash-qemu.ld').write_text('''ENTRY(start)
SECTIONS {
 . = 0x41000000;
 .text : { *(.text*) *(.rodata*) }
 .data : { *(.data*) }
 .bss (NOLOAD) : { *(.bss*) }
 .image.cell 0x4D28929C : { *(.image.data.mac_mbox) }
}
''')
    elf = build / 'mac-crash-qemu.elf'
    command = ['clang', '-target', 'armv7em-none-eabi', '-mcpu=cortex-m4', '-O1',
               '-fno-builtin', '-ffreestanding', '-ffunction-sections', '-fdata-sections',
               '-nostdlib', '-fuse-ld=lld', '-Iinclude', '-Wl,--gc-sections',
               '-Wl,-T,' + str(build / 'mac-crash-qemu.ld'), str(build / 'mac-crash-qemu.S'),
               str(build / 'mac-crash-qemu.c'), str(build / 'arm/w3/mac.o'), '-o', str(elf)]
    subprocess.run(command, cwd=ROOT, check=True)
    qemu = ['qemu-system-arm', '-M', 'virt', '-cpu', 'cortex-a15', '-m', '512M',
            '-nographic', '-semihosting-config', 'enable=on,target=native', '-device',
            f'loader,file={reference},addr=0,force-raw=on', '-device', f'loader,file={elf},cpu-num=0']
    try:
        run = subprocess.run(qemu, capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired as exc:
        (build / 'mac-crash-qemu-timeout.txt').write_bytes((exc.stdout or b'') + (exc.stderr or b''))
        run = subprocess.CompletedProcess(qemu, 124, '', 'QEMU timeout; see mac-crash-qemu-timeout.txt')
    summary = re.search(r'CASES (\d+) FAILED (\d+)', run.stdout + run.stderr)
    expected = 4 * 4 * 45 * 3
    executed = int(summary[1]) if summary else None
    failures = int(summary[2]) if summary else None
    complete = executed == expected and failures == 0 and run.returncode == 0
    report = {'scope': __doc__, 'reference_sha256': REFERENCE_SHA256,
              'compiled_case_count': expected, 'executed_case_count': executed,
              'failures': failures, 'complete': complete, 'exit_code': run.returncode,
              'comparison_elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
              'mac_object_sha256': hashlib.sha256((build / 'arm/w3/mac.o').read_bytes()).hexdigest(),
              'build_command': command, 'qemu_command': qemu, 'stdout': run.stdout, 'stderr': run.stderr}
    (build / 'mac-crash-qemu.json').write_text(json.dumps(report, indent=2) + '\n')
    print(run.stdout + run.stderr, end='')
    return 0 if complete else 1


if __name__ == '__main__':
    raise SystemExit(main())
