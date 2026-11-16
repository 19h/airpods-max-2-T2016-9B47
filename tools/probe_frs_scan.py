#!/usr/bin/env python3
"""Finite FRS scan, calibration and complete entry/scan/process/exit workflows.

The default matrix uses AGC table selectors 1..3 and explicit polling responses.
--selector-zero diagnoses the original path's uninitialized stack-slot reads.
Arbitrary aliases, interrupts and physical
AGC/correlator behavior remain outside these comparisons; no full equivalence
claim follows. The separate ROM-observability gate retains its known failure.
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import struct
from pathlib import Path

from arm_semantics import (ARG_REGS, Machine, REFERENCE_SHA256, TEST,
                           UC_ARM_REG_PC, UC_HOOK_CODE, elf_bytes)
from instruction_observer import InstructionObserver
from unicorn import UC_HOOK_MEM_INVALID
import unicorn.arm_const as uarm


def check(elf, action="scan", band=0, offset=0, half_span=8, center=0,
          single=0, forced=True, profile=0, selector=1, passes=1,
          changing=False, timer_step=1000, defaults=False, trim=False,
          selector_sequence=None, expected_fault=False):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        failed_accesses = []

        def failed_access(cpu, kind, address, size, value, unused):
            failed_accesses.append({"kind": kind, "address": hex(address),
                                    "size": size, "value": value})
            return False

        if selector == 0 or (selector_sequence is not None and 0 in selector_sequence):
            m.cpu.hook_add(UC_HOOK_MEM_INVALID, failed_access)
        for handle in m.observer_handles:
            m.cpu.hook_del(handle)
        m.cpu.hook_add(UC_HOOK_CODE, InstructionObserver(m))
        m.write(TEST, bytes([0xA5]) * 0x6000)
        m.write(TEST + 0x7000, bytes(0x1000))
        m.write(0x4D28A300, bytes(0x6C8))
        m.write(0x4D28A300, bytes([band]))
        m.write(0x4D28A302, struct.pack("<HB", int(forced), 45))
        m.write(0x4D28A9C4, bytes([half_span, single, center]))
        m.write(0x4D28A5D8, bytes([passes]))
        m.word(0x4D28A5C8, TEST + 0x100)
        m.word(0x4D28A5CC, 3)
        m.write(TEST + 0x100, bytes([8, 24, 40]))
        for row in range(3):
            values = [(-32768, -30000, -500, -441, -440, -300, -1, 0, 1, 2, 3, 4, 32767)[(i + row) % 13]
                      for i in range(152)]
            m.write(0x4D28A5DA + row * 304, struct.pack("<152h", *values))
            m.write(0x4D28A400 + row * 152, bytes((i * 17 + row * 13) & 255 for i in range(152)))
        if trim:
            m.word(0x4D28CF18, TEST + 0x200)
            m.write(TEST + 0x200, bytes((i * 37) & 255 for i in range(24)))
        for table in range(3):
            m.write(0x4D150500 + table * 0x100,
                    struct.pack("<64I", *[(i * 0x1337 + table * 0x12345678) & 0xFFFFFFFF for i in range(64)]))
        m.word(0x4D150090, 0x40404000)
        m.word(0x4D150030, ((0 if profile == 2 else 3) << 22) | (selector << 14) | (45 << 25))
        m.word(0x4D150AD4, 2 if profile == 3 else 0)
        m.word(0x4D150AF8, 0x100 if profile == 4 else 0x2AB)
        for i in range(8):
            m.word(0x4D150AD8 + i * 4, ((i * 0x37 + 0x123) & 0x3FF) |
                   (((i * 0x53 + 0x234) & 0x3FF) << 16))
        for address in (0x4D000100, 0x4D000250):
            m.word(address, 0x40000 if profile == 1 else 0)
        m.read_plan[0xE000E018] = {i: (0xFFFFFF - i * timer_step) & 0xFFFFFF
                                   for i in range(1, 20000)}
        if changing:
            m.read_plan[0x4D154038] = {i: (i * 0x1234567) & 0xFFFFFFFF for i in range(1, 200)}
            m.read_plan[0x4D150AF8] = {i: (0x200 + i * 17) for i in range(1, 200)}
            for address in (0x4D000100, 0x4D000250):
                m.read_plan[address] = {i: (0x20000 if i & 1 else 0x40000) for i in range(1, 200)}
        if selector_sequence is not None:
            m.read_plan[0x4D150030] = {i + 1: (3 << 22) | (value << 14) | (45 << 25)
                                        for i, value in enumerate(selector_sequence)}
        if action == "process":
            steps = [(0x11B4, "frs_process_results", [TEST + 0x1000, TEST + 0x2000])]
        elif action == "workflow":
            steps = [(0x15E0, "frs_enter", [0 if forced else 0x7F, TEST + 0x1000, TEST + 0x2000]),
                     (0x1120, "frs_run_scan", [0 if defaults else TEST + 0x100, 3]),
                     (0x11B4, "frs_process_results", [TEST + 0x1000, TEST + 0x2000]),
                     (0x1A1C, "frs_exit", [])]
        else:
            steps = [(0xD7C, "frs_scan_pass", [TEST + 0x3100, TEST + 0x100, 3, offset])]
        try:
            returns, transactions = [], []
            for rva, name, arguments in steps:
                returns.append(m.call(rva if reference else name, arguments))
                transactions.append(list(m.transactions))
            observations.append({"returns": returns, "transactions": transactions,
                                 "state": m.read(0x4D280000, 0xD178).hex(),
                                 "scratch": m.read(TEST, 0x6000).hex()})
        except Exception as exc:
            transactions.append(list(m.transactions))
            pc = m.cpu.reg_read(UC_ARM_REG_PC)
            scan_entry = 0xD7C if reference else (m.symbols["frs_scan_pass"] & ~1)
            observations.append({"error": type(exc).__name__ + ": " + str(exc),
                                 "pc": hex(pc), "scan_pc_offset": hex(pc - scan_entry),
                                 "failed_accesses": failed_accesses,
                                 "r0_r3": [hex(m.cpu.reg_read(reg)) for reg in ARG_REGS],
                                 "registers": {name: hex(m.cpu.reg_read(getattr(uarm, "UC_ARM_REG_" + name.upper())))
                                               for name in [*["r" + str(i) for i in range(13)], "sp", "lr", "xpsr"]},
                                 "active_frame": m.read(m.cpu.reg_read(uarm.UC_ARM_REG_SP), 112).hex(),
                                 "returns": returns, "transactions": transactions,
                                 "state": m.read(0x4D280000, 0xD178).hex(),
                                 "scratch": m.read(TEST, 0x6000).hex()})
    equal = observations[0] == observations[1]
    matched_fault = (expected_fault and equal and
                     observations[0].get("error", "").startswith("UcError:") and
                     bool(observations[0].get("failed_accesses")))
    match = equal and (("error" not in observations[0] and not expected_fault) or matched_fault)
    fault_access_match = False
    if all(result.get("failed_accesses") for result in observations):
        fields = ("error", "scan_pc_offset", "failed_accesses", "r0_r3", "returns",
                  "transactions", "state", "scratch", "registers", "active_frame")
        fault_access_match = all(observations[0][key] == observations[1][key] for key in fields)
    if all("state" in result for result in observations):
        for key in ("state", "scratch"):
            if observations[0][key] == observations[1][key]:
                value = bytes.fromhex(observations[0][key])
                for result in observations:
                    result[key] = {"sha256": hashlib.sha256(value).hexdigest(), "byte_count": len(value)}
    return {"action": action, "band": band, "offset": offset, "half_span": half_span,
            "center": center, "single": single, "forced": forced, "profile": profile,
            "selector": selector, "passes": passes, "changing": changing,
            "timer_step": timer_step, "defaults": defaults, "trim": trim,
            "selector_sequence": selector_sequence,
            "expected_memory_fault": expected_fault, "matched_memory_fault": matched_fault,
            "relocated_fault_access_match": fault_access_match,
            "match": match, "reference": observations[0], "rebuilt": observations[1]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--selector-zero", action="store_true")
    args = parser.parse_args()
    if args.selector_zero:
        cases = [check(args.elf, band=band, single=1, forced=False, selector=0, expected_fault=True)
                 for band in (0, 2)]
        cases.extend(check(args.elf, band=band, single=1, forced=False,
                           selector_sequence=sequence)
                     for band, sequence in itertools.product(
                         (0, 2), ((1, 0, 1), (2, 0, 3), (3, 0, 0))))
    else:
        cases = [check(args.elf, band=band, offset=offset, half_span=span,
                       center=center, single=single, forced=forced, profile=profile)
                 for band, offset, span, center, single, forced, profile in itertools.product(
                     range(8), (-1, 0, 1), (0, 1, 8), (0, 1), (0, 1), (False, True), range(5))]
        cases.extend(check(args.elf, band=band, single=single, forced=False, center=1,
                           selector=selector, changing=True, timer_step=step)
                     for band, single, selector, step in itertools.product(
                         range(8), (0, 2, 255), (1, 2, 3), (1, 1000)))
        cases.extend(check(args.elf, "process", band=band, passes=passes, trim=trim)
                     for band, passes, trim in itertools.product(range(8), (1, 2, 3), (False, True)))
        cases.extend(check(args.elf, "workflow", band=band, passes=passes, center=center,
                           single=single, forced=forced, defaults=defaults, trim=True)
                     for band, passes, center, single, forced, defaults in itertools.product(
                         range(8), (1, 2, 3), (0, 1), (0, 1), (False, True), (False, True)))
    if args.elf.read_bytes() != elf_bytes(args.elf):
        raise RuntimeError("ELF changed during comparisons")
    failures = sum(not case["match"] for case in cases)
    report = {"scope": __doc__, "reference_sha256": REFERENCE_SHA256,
              "elf_sha256": hashlib.sha256(elf_bytes(args.elf)).hexdigest(),
              "entry_stack_zeroed": True, "selector_zero_diagnostic": args.selector_zero,
              "fault_observation_scope": "matching failed accesses, absolute PCs, general registers/xPSR, active frame and pre-fault effects; exception delivery is not executed",
              "case_count": len(cases), "divergences": failures,
              "relocated_fault_access_matches": sum(case["relocated_fault_access_match"] for case in cases),
              "complete": failures == 0, "cases": cases}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"FRS scanning: {len(cases)} cases, {failures} divergences")
    return int(failures != 0)


if __name__ == "__main__":
    raise SystemExit(main())
