#!/usr/bin/env python3
"""Independent diagnostic-alias comparison in QEMU Normal memory.

Reference instructions are unmodified. This Cortex-A15 fixture uses mapped
ordinary RAM, six-argument AAPCS calls and no hooks; it establishes no physical
peripheral, fault-delivery, timing or arbitrary stack-alias behavior.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from arm_semantics import ROOT, REFERENCE_SHA256
from probe_diag_alias import cases
from qemu_formatter import ENTRY

PROGRAM = r'''
#include <stdint.h>
#include <stddef.h>
#include "btp/status.h"
void write_text(const char *);
void enable_memory(uint32_t *);
static uint32_t sections[4096] __attribute__((aligned(16384)));
static unsigned char snapshot[0x200];
static unsigned unexpected;
void trace_log_error(uint32_t a, uint32_t b, uint32_t c) { ++unexpected; }
struct test_case { unsigned index; int delta; uint32_t offset, requested, length, checksum; };
static const struct test_case cases[] = { @@CASES@@ };
static void number(unsigned n) {
    char text[12]; unsigned pos = 11; text[pos] = 0;
    do { text[--pos] = (char)('0' + n % 10); n /= 10; } while (n);
    write_text(text + pos);
}
static void put(uintptr_t address, uint32_t value) { *(volatile uint32_t *)address = value; }
static void seed(const struct test_case *c) {
    for (unsigned i = 0; i < 0x200; ++i) *(volatile unsigned char *)(0x4D28A2C0 + i) = 0x5A;
    for (unsigned i = 0; i < 512; ++i) {
        *(volatile unsigned char *)(0x4D161000 + i) = (unsigned char)i;
        *(volatile unsigned char *)(0x4D162000 + i) = (unsigned char)(255 - i);
    }
    uintptr_t descriptor = 0x4D28A2D0 + 16 * c->index;
    put(descriptor, 1); put(descriptor + 4, 0x4D161000);
    put(descriptor + 8, c->length); put(descriptor + 12, c->checksum);
    put(0x4D163000, 0xD00DFEED);
}
static unsigned capture(unsigned compare) {
    unsigned match = 1;
    for (unsigned i = 0; i < sizeof snapshot; ++i) {
        unsigned char byte = *(volatile unsigned char *)(0x4D28A2C0 + i);
        if (compare) { if (snapshot[i] != byte) match = 0; }
        else snapshot[i] = byte;
    }
    return match;
}
int test(void) {
    for (unsigned i = 0; i < 4096; ++i) sections[i] = (i << 20) | 0x1C02;
    enable_memory(sections);
    unsigned tested = 0, failed = 0;
    typedef btp_status_t (*reader)(uint8_t *, uint32_t *, uint32_t, uint32_t, uint32_t, uint32_t);
    reader original = (reader)(uintptr_t)0x4DBD;
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        const struct test_case *c = &cases[i];
        uint8_t *out = (uint8_t *)(0x4D28A2D0 + 16 * c->index + c->delta);
        seed(c);
        uint32_t a = original(out, (uint32_t *)0x4D163000, c->index, c->offset, c->requested, 0xA6);
        uint32_t copied = *(volatile uint32_t *)0x4D163000;
        capture(0);
        seed(c);
        uint32_t b = diag_buffer_read(out, (uint32_t *)0x4D163000, c->index, c->offset, c->requested, 0xA6);
        unsigned match = capture(1) && a == b && copied == *(volatile uint32_t *)0x4D163000;
        ++tested;
        if (!match) {
            ++failed;
            if (failed <= 8) { write_text("DIAG_ALIAS_DIVERGENCE case="); number(tested); write_text("\n"); }
        }
    }
    failed += unexpected;
    write_text("CASES "); number(tested); write_text(" FAILED "); number(failed); write_text("\n");
    return failed != 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--status-object', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    build = args.build.resolve()
    build.mkdir(parents=True, exist_ok=True)
    status = (args.status_object or build / 'arm/w3/status.o').resolve()
    strings = build / 'arm/libc/string.o'
    reference = ROOT.parent / 'phyb'
    raw = reference.read_bytes()
    if hashlib.sha256(raw).hexdigest() != REFERENCE_SHA256:
        raise ValueError('reference firmware checksum mismatch')
    matrix = list(cases())
    source = PROGRAM.replace('@@CASES@@', ',\n'.join('{' + ','.join(str(v) + ('' if i == 1 else 'u')
                                   for i, v in enumerate(c)) + '}' for c in matrix))
    (build / 'diag-alias-qemu.c').write_text(source)
    (build / 'diag-alias-qemu.S').write_text(ENTRY)
    linker = build / 'diag-alias-qemu.ld'
    linker.write_text('''ENTRY(start)
SECTIONS {
 . = 0x41000000;
 .text : { *(.text*) *(.rodata*) }
 .data : { *(.data*) }
 .bss (NOLOAD) : { *(.bss*) }
 .state.diag 0x4D28A2C8 (NOLOAD) : { *(.state.diag) }
}
''')
    elf = build / 'diag-alias-qemu.elf'
    command = ['clang', '-target', 'armv7em-none-eabi', '-mcpu=cortex-m4', '-O1',
               '-fno-builtin', '-ffreestanding', '-ffunction-sections', '-fdata-sections',
               '-nostdlib', '-fuse-ld=lld', '-Iinclude', '-Wl,--gc-sections',
               '-Wl,-T,' + str(linker), str(build / 'diag-alias-qemu.S'),
               str(build / 'diag-alias-qemu.c'), str(status), str(strings), '-o', str(elf)]
    subprocess.run(command, cwd=ROOT, check=True)
    qemu = ['qemu-system-arm', '-M', 'virt', '-cpu', 'cortex-a15', '-m', '512M',
            '-nographic', '-semihosting-config', 'enable=on,target=native', '-device',
            f'loader,file={reference},addr=0,force-raw=on', '-device', f'loader,file={elf},cpu-num=0']
    try:
        run = subprocess.run(qemu, capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired as error:
        (build / 'diag-alias-qemu-timeout.txt').write_bytes((error.stdout or b'') + (error.stderr or b''))
        run = subprocess.CompletedProcess(qemu, 124, '', 'QEMU timed out')
    summary = re.search(r'CASES (\d+) FAILED (\d+)', run.stdout + run.stderr)
    executed = int(summary[1]) if summary else None
    failures = int(summary[2]) if summary else None
    complete = executed == len(matrix) and failures == 0 and run.returncode == 0
    report = {'scope': __doc__, 'reference_sha256': REFERENCE_SHA256,
              'compiled_case_count': len(matrix), 'executed_case_count': executed,
              'failures': failures, 'exit_code': run.returncode, 'complete': complete,
              'comparison_elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
              'status_object_sha256': hashlib.sha256(status.read_bytes()).hexdigest(),
              'string_object_sha256': hashlib.sha256(strings.read_bytes()).hexdigest(),
              'build_command': command, 'qemu_command': qemu,
              'stdout': run.stdout, 'stderr': run.stderr}
    (args.output or build / 'diag-alias-qemu.json').write_text(json.dumps(report, indent=2) + '\n')
    print(run.stdout + run.stderr, end='')
    return not complete


if __name__ == '__main__':
    raise SystemExit(main())
