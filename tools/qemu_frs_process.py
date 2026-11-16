#!/usr/bin/env python3
"""Independent hook-free FRS result calibration and bounded scans in QEMU.

The reference ROM is unmodified. This Cortex-A15 scalar fixture compares
ordinary-memory results. Scans use reload=0 (delays clamp to zero), settled AGC
and an idle correlator; SCS maps to Normal RAM. It does not validate real devices, M-profile faults,
or arbitrary aliases. --selector-zero separately checks explicit preexisting
stack-slot values; their hardware reachability remains unknown.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path
from elftools.elf.elffile import ELFFile

from arm_semantics import ROOT, REFERENCE_SHA256
from qemu_formatter import ENTRY

PROGRAM = r"""
#include <stdint.h>
#include <stddef.h>
#include "btp/frs.h"
void write_text(const char *);
void enable_memory(uint32_t *);
int call_scan_seeded(int16_t *, const uint8_t *, int, int, uintptr_t, uint32_t, uintptr_t);
static uint32_t sections[4096] __attribute__((aligned(16384)));
static unsigned char scratch[0x3000], snapshot[0x10000];
_Static_assert(0x6C8 + sizeof scratch + 0x400 + 0x1B00 + 0xD00 + 12 <= sizeof snapshot, "snapshot capacity");
static unsigned tested, failed;
static void number(unsigned n) {
    char text[12]; unsigned pos = 11; text[pos] = 0;
    do { text[--pos] = (char)('0' + n % 10); n /= 10; } while (n);
    write_text(text + pos);
}
static void put(uintptr_t a, uint32_t value) { *(volatile uint32_t *)a = value; }
static void seed(unsigned band, unsigned passes, unsigned trim, unsigned span, unsigned pattern) {
    const int16_t values[] = {-32768, -30000, -500, -441, -440, -300, -1, 0, 1, 2, 3, 4, 32767};
    for (unsigned i = 0; i < 0x6C8; ++i) *(volatile unsigned char *)(0x4D28A300 + i) = 0;
    for (unsigned i = 0; i < sizeof scratch; ++i) scratch[i] = 0xA5;
    *(volatile unsigned char *)0x4D28A300 = (unsigned char)band;
    *(volatile unsigned char *)0x4D28A5D8 = (unsigned char)passes;
    *(volatile unsigned char *)0x4D28A9C4 = (unsigned char)span;
    put(0x4D28A5C8, (uintptr_t)scratch + 0x100); put(0x4D28A5CC, 3);
    scratch[0x100] = 8; scratch[0x101] = 24; scratch[0x102] = 40;
    put(0x4D28CF18, trim ? (uintptr_t)scratch + 0x200 : 0);
    for (unsigned i = 0; i < 24; ++i) scratch[0x200 + i] = (unsigned char)(i * 37);
    for (unsigned row = 0; row < 3; ++row) for (unsigned i = 0; i < 152; ++i) {
        *(volatile int16_t *)(0x4D28A5DA + row * 304 + i * 2) = values[(i + row + pattern * 3) % 13];
        *(volatile unsigned char *)(0x4D28A400 + row * 152 + i) =
            (unsigned char)(pattern ? (i * 17 + row * 13) : (i % 2 ? 44 : 45));
    }
}
static unsigned capture(unsigned compare) {
    unsigned pos = 0, match = 1;
    const uintptr_t bases[] = {0x4D28A300, (uintptr_t)scratch, 0x4D000000, 0x4D150000, 0x4D154000, 0x4D289900};
    const unsigned sizes[] = {0x6C8, sizeof scratch, 0x400, 0x1B00, 0xD00, 12};
    for (unsigned region = 0; region < 6; ++region) for (unsigned i = 0; i < sizes[region]; ++i) {
        unsigned char value = *(volatile unsigned char *)(bases[region] + i);
        if (compare) { if (snapshot[pos] != value) match = 0; }
        else snapshot[pos] = value;
        ++pos;
    }
    return match;
}
static void check(unsigned band, unsigned passes, unsigned trim, unsigned span, unsigned pattern) {
    typedef uint32_t (*process_fn)(int16_t *, uint8_t *);
    seed(band, passes, trim, span, pattern);
    uint32_t a = ((process_fn)(uintptr_t)0x11B5)((int16_t *)(scratch + 0x1000), scratch + 0x2000);
    capture(0);
    seed(band, passes, trim, span, pattern);
    uint32_t b = frs_process_results((int16_t *)(scratch + 0x1000), scratch + 0x2000);
    ++tested;
    if (!capture(1) || a != b) { ++failed; write_text("FRS_PROCESS_DIVERGENCE case="); number(tested); write_text("\n"); }
}
static void seed_scan(unsigned band, int offset, unsigned span, unsigned center,
                      unsigned single, unsigned forced, unsigned selector, unsigned profile) {
    seed(band, 1, 0, span, 0);
    const uintptr_t bases[] = {0x4D000000, 0x4D150000, 0x4D154000, 0x4D289900};
    const unsigned sizes[] = {0x400, 0x1B00, 0xD00, 12};
    for (unsigned region = 0; region < 4; ++region) for (unsigned i = 0; i < sizes[region]; ++i)
        *(volatile unsigned char *)(bases[region] + i) = 0;
    *(volatile uint16_t *)0x4D28A302 = (uint16_t)forced;
    *(volatile unsigned char *)0x4D28A304 = 45;
    *(volatile unsigned char *)0x4D28A9C5 = (unsigned char)single;
    *(volatile unsigned char *)0x4D28A9C6 = (unsigned char)center;
    put(0xE000E018, 0);
    put(0x4D000100, profile == 2 ? 0x40000 : 0); put(0x4D000250, profile == 2 ? 0x40000 : 0);
    put(0x4D150090, 0x40404000); put(0x4D150030, (3 << 22) | (selector << 14) | (45U << 25));
    put(0x4D150AF8, profile == 1 ? 0x100 : 0x2AB);
    for (unsigned table = 0; table < 3; ++table) for (unsigned i = 0; i < 64; ++i)
        put(0x4D150500 + table * 0x100 + i * 4, i * 0x1337 + table * 0x12345678);
    for (unsigned i = 0; i < 8; ++i)
        put(0x4D150AD8 + i * 4, ((i * 0x37 + 0x123) & 0x3FF) | (((i * 0x53 + 0x234) & 0x3FF) << 16));
}
static void scan(unsigned band, int offset, unsigned span, unsigned center,
                 unsigned single, unsigned forced, unsigned selector, unsigned profile) {
    typedef int (*scan_fn)(int16_t *, const uint8_t *, int, int);
    seed_scan(band, offset, span, center, single, forced, selector, profile);
    int a = ((scan_fn)(uintptr_t)0xD7D)((int16_t *)(scratch + 0x2100), scratch + 0x100, 3, offset);
    capture(0);
    seed_scan(band, offset, span, center, single, forced, selector, profile);
    int b = frs_scan_pass((int16_t *)(scratch + 0x2100), scratch + 0x100, 3, offset);
    ++tested;
    if (!capture(1) || a != b) { ++failed; write_text("FRS_SCAN_DIVERGENCE case="); number(tested); write_text("\n"); }
}
static void scan_seeded(uint32_t base, uintptr_t table) {
    seed_scan(0, 0, 8, 1, 1, 0, 0, 0);
    int a = call_scan_seeded((int16_t *)(scratch + 0x2100), scratch + 0x100, 3, 0, 0xD7D, base, table);
    uint32_t original_gain_force = *(volatile uint32_t *)0x4D15013C;
    capture(0);
    seed_scan(0, 0, 8, 1, 1, 0, 0, 0);
    int b = call_scan_seeded((int16_t *)(scratch + 0x2100), scratch + 0x100, 3, 0,
                             (uintptr_t)frs_scan_pass, base, table);
    uint32_t rebuilt_gain_force = *(volatile uint32_t *)0x4D15013C;
    ++tested;
    if (!capture(1) || a != b) {
        ++failed; write_text("FRS_SELECTOR_ZERO_DIVERGENCE case="); number(tested);
        write_text(" gain_force_reference="); number(original_gain_force);
        write_text(" gain_force_rebuilt="); number(rebuilt_gain_force); write_text("\n");
    }
}
int test(void) {
    for (unsigned i = 0; i < 4096; ++i) sections[i] = (i << 20) | 0x1C02;
    sections[0xE00] = 0x4D100000 | 0x1C02;
    enable_memory(sections);
#ifdef SELECTOR_ZERO_DIAGNOSTIC
    for (unsigned base = 64; base <= 80; base += 16) for (unsigned table = 0; table < 3; ++table)
        scan_seeded(base, 0x4D150500 + table * 0x100);
    scan_seeded(0, 0x4D150500);
#else
    for (unsigned band = 0; band < 8; ++band)
    for (unsigned passes = 1; passes <= 3; ++passes)
    for (unsigned trim = 0; trim < 2; ++trim)
    for (unsigned span = 0; span < 2; ++span)
    for (unsigned pattern = 0; pattern < 2; ++pattern)
        check(band, passes, trim, span ? 8 : 0, pattern);
    for (unsigned band = 0; band < 8; ++band) for (unsigned trim = 0; trim < 2; ++trim)
        check(band, 0, trim, 8, 0);
    for (unsigned band = 0; band < 2; ++band)
    for (unsigned passes = 0; passes <= 3; ++passes)
    for (unsigned trim = 0; trim < 2; ++trim)
        check(band ? 255 : 8, passes, trim, 8, 0);
    for (unsigned band = 0; band < 2; ++band)
    for (int offset = -1; offset <= 1; ++offset)
    for (unsigned span = 0; span < 3; ++span)
    for (unsigned center = 0; center < 2; ++center)
    for (unsigned single = 0; single < 2; ++single)
    for (unsigned forced = 0; forced < 2; ++forced)
    for (unsigned selector = 1; selector <= 3; ++selector)
    for (unsigned profile = 0; profile < 3; ++profile)
        scan(band ? 2 : 0, offset, span == 2 ? 8 : span, center, single, forced, selector, profile);
#endif
    write_text("CASES "); number(tested); write_text(" FAILED "); number(failed); write_text("\n");
    return failed != 0;
}
"""

SEEDED_CALL = r"""
.syntax unified
.thumb
.text
.thumb_func
.global call_scan_seeded
call_scan_seeded:
    /* The original callee allocates 112 bytes. Its SP+8/SP+12 slots are
       entry-SP-104/entry-SP-100; seed them before the call, without hooks. */
    push {r4, lr}
    ldr r4, [sp, #8]
    ldr ip, [sp, #12]
    str ip, [sp, #-104]
    ldr ip, [sp, #16]
    str ip, [sp, #-100]
    blx r4
    pop {r4, pc}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--selector-zero", action="store_true")
    args = parser.parse_args()
    build = args.build.resolve()
    reference = ROOT.parent / "phyb"
    if hashlib.sha256(reference.read_bytes()).hexdigest() != REFERENCE_SHA256:
        raise ValueError("reference firmware hash mismatch")
    stem = "frs-selector-zero-qemu" if args.selector_zero else "frs-process-qemu"
    (build / (stem + ".c")).write_text(("#define SELECTOR_ZERO_DIAGNOSTIC\n" if args.selector_zero else "") + PROGRAM)
    (build / (stem + ".S")).write_text(ENTRY + SEEDED_CALL)
    (build / (stem + ".ld")).write_text("""ENTRY(start)
PHDRS {
 /* Bound to the unchanged reference ROM; do not load it a second time. */
 reference_data PT_NULL FLAGS(4);
 code PT_LOAD FLAGS(5);
 data PT_LOAD FLAGS(6);
 state PT_LOAD FLAGS(6);
}
SECTIONS {
 .frs.scan_log 0x7E48 : { *(.image.rom.frs_scan_log) } :reference_data
 . = 0x41000000;
 .text : { *(.text*) *(.rodata*) } :code
 .ARM.exidx : { *(.ARM.exidx*) } :code
 .data : { *(.data*) } :data
 .bss (NOLOAD) : { *(.bss*) } :data
 .state.clock 0x4D289900 (NOLOAD) : { *(.state.clock) } :state
 .state.frs 0x4D28A300 (NOLOAD) : { *(.state.frs) } :state
 .state.syscfg 0x4D28CB24 (NOLOAD) : { *(.state.syscfg) } :state
}
""")
    elf = build / (stem + ".elf")
    command = ["clang", "-target", "armv7em-none-eabi", "-mcpu=cortex-m4", "-O1",
               "-fno-builtin", "-ffreestanding", "-nostdlib", "-fuse-ld=lld", "-Iinclude",
               "-Wl,--gc-sections", "-Wl,-T," + str(build / (stem + ".ld")),
               str(build / (stem + ".S")), str(build / (stem + ".c")),
               str(build / "arm/w3/frs.o"), str(build / "arm/w3/syscfg.o"),
               str(build / "arm/w3/clock.o"), str(build / "arm/w3/trace.o"),
               str(build / "arm/libc/string.o"), "-o", str(elf)]
    subprocess.run(command, cwd=ROOT, check=True)
    overlaps = []
    reference_bytes = reference.read_bytes()
    with elf.open("rb") as stream:
        image = ELFFile(stream)
        log = image.get_section_by_name(".frs.scan_log")
        if log is None or log["sh_addr"] != 0x7E48 or log.data() != reference_bytes[0x7E48:0x7E73]:
            raise ValueError("scan messages do not match their bound reference ROM")
        for segment in image.iter_segments():
            start = segment["p_vaddr"]
            end = min(start + segment["p_filesz"], len(reference_bytes))
            if segment["p_type"] == "PT_LOAD" and start < end:
                identical = segment.data()[:end - start] == reference_bytes[start:end]
                overlaps.append({"address": hex(start), "byte_count": end - start, "identical": identical})
                if not identical:
                    raise ValueError("comparison ELF changes original reference ROM bytes")
    qemu = ["qemu-system-arm", "-M", "virt", "-cpu", "cortex-a15", "-m", "512M",
            "-nographic", "-semihosting-config", "enable=on,target=native", "-device",
            f"loader,file={reference},addr=0,force-raw=on", "-device", f"loader,file={elf},cpu-num=0"]
    try:
        run = subprocess.run(qemu, capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired as exc:
        (build / (stem + "-timeout.txt")).write_bytes((exc.stdout or b"") + (exc.stderr or b""))
        run = subprocess.CompletedProcess(qemu, 124, "", f"QEMU timeout; see {stem}-timeout.txt")
    summary = re.search(r"CASES (\d+) FAILED (\d+)", run.stdout + run.stderr)
    expected = 7 if args.selector_zero else 8 * 3 * 2 * 2 * 2 + 8 * 2 + 2 * 4 * 2 + 2 * 3 * 3 * 2 * 2 * 2 * 3 * 3
    executed = int(summary[1]) if summary else None
    failures = int(summary[2]) if summary else None
    complete = executed == expected and failures == 0 and run.returncode == 0
    report = {"scope": __doc__, "reference_sha256": REFERENCE_SHA256,
              "selector_zero_diagnostic": args.selector_zero,
              "reference_rom_overlaps": overlaps,
              "bound_reference_messages": {"address": "0x7e48", "byte_count": 43,
                                           "identical": True, "loaded_again": False},
              "selector_zero_stack_seeds": ([{"base": base, "table": hex(0x4D150500 + table * 0x100)}
                                              for base in (64, 80) for table in range(3)] +
                                             [{"base": 0, "table": "0x4d150500"}]) if args.selector_zero else None,
              "compiled_case_count": expected, "executed_case_count": executed,
              "failures": failures, "complete": complete, "exit_code": run.returncode,
              "comparison_elf_sha256": hashlib.sha256(elf.read_bytes()).hexdigest(),
              "frs_object_sha256": hashlib.sha256((build / "arm/w3/frs.o").read_bytes()).hexdigest(),
              "build_command": command, "qemu_command": qemu, "stdout": run.stdout, "stderr": run.stderr}
    (build / (stem + ".json")).write_text(json.dumps(report, indent=2) + "\n")
    print(run.stdout + run.stderr, end="")
    return int(not complete)


if __name__ == "__main__":
    raise SystemExit(main())
