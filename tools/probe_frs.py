#!/usr/bin/env python3
"""Finite FRS entry/exit comparisons with scripted SysTick and MMIO responses.

No physical AGC/correlator, autonomous writes or interrupt interleavings are
modeled. The instruction observer is the same conditional-transfer model used
by the MAC and DPD probes. Complete scans and arbitrary aliases remain open.
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import struct
from pathlib import Path

from arm_semantics import Machine, REFERENCE_SHA256, TEST, UC_HOOK_CODE, elf_bytes
from instruction_observer import InstructionObserver


def check(elf, action, band=0, single=0, gain=0x7F, calibration=0, seed=0,
          changing=False, outputs=True, timer_step=1000):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        for handle in m.observer_handles:
            m.cpu.hook_del(handle)
        m.cpu.hook_add(UC_HOOK_CODE, InstructionObserver(m))
        for address, size in ((0x4D000000, 0x400), (0x4D140000, 0x1000),
                              (0x4D150000, 0x1B00), (0x4D154000, 0xD00)):
            m.write(address, struct.pack("<I", seed) * (size // 4))
        m.write(TEST, bytes([0xA5]) * 0x6000)
        m.write(0x4D28A300, bytes(0x6C8))
        m.write(0x4D28A300, bytes([band]))
        m.write(0x4D28A9C5, bytes([single]))
        m.write(TEST + 0x200, bytes([8, 24, 40]))
        m.word(0x4D28A5C8, TEST + 0x200)
        m.word(0x4D28A5CC, 1)
        m.write(0x4D28A96C, struct.pack("<22I", *[
            (seed + i * 0x1234567) & 0xFFFFFFFF for i in range(22)]))
        records = b""
        if calibration & 1:
            words = [((0, 1496, 1500, 4092)[i % 4] << 18) & 0xFFFFFFFF for i in range(16)]
            records += struct.pack("<HH16I", 0xD17E, 64, *words)
        if calibration & 2:
            for tag, base in ((0xD1D8, 0x1234), (0xD1D9, 0x5678)):
                records += struct.pack("<HH16H", tag, 32, *[
                    (base + i * 0x137) & 0xFFFF for i in range(16)])
        if records:
            blob = struct.pack("<HHI", len(records), 0x4E56, 0) + records
            m.write(TEST + 0x4000, blob)
            m.call(0x50B0 if reference else "nvv_parse_blob", [TEST + 0x4000, len(blob)])
        m.read_plan[0xE000E018] = {i: (0xFFFFFF - (i - 1) * timer_step) & 0xFFFFFF
                                   for i in range(1, 1000)}
        if changing:
            for address in (0x4D154038, 0x4D154008, 0x4D150AD0, 0x4D150168,
                            0x4D15084C, 0x4D150940, 0x4D0002AC, 0x4D00015C):
                m.read_plan[address] = {i: (seed ^ (i * 0x1234567)) & 0xFFFFFFFF
                                        for i in range(1, 32)}
        try:
            returns = []
            transactions = []
            for step in (("enter", "exit") if action == "enter-exit" else (action,)):
                target = (0x15E0 if step == "enter" else 0x1A1C) if reference else "frs_" + step
                args = [gain, TEST + 0x1000 if outputs else 0,
                        TEST + 0x2000 if outputs else 0] if step == "enter" else []
                returns.append(m.call(target, args))
                transactions.append(list(m.transactions))
            observations.append({"returns": returns, "transactions": transactions,
                                 "state": m.read(0x4D280000, 0xD178).hex(),
                                 "scratch": m.read(TEST, 0x6000).hex()})
        except Exception as exc:
            observations.append({"error": type(exc).__name__ + ": " + str(exc)})
    match = observations[0] == observations[1] and "error" not in observations[0]
    if all("error" not in result for result in observations):
        for key in ("state", "scratch"):
            if observations[0][key] == observations[1][key]:
                value = bytes.fromhex(observations[0][key])
                for result in observations:
                    result[key] = {"sha256": hashlib.sha256(value).hexdigest(), "byte_count": len(value)}
    return {"action": action, "band": band, "single": single, "gain": gain,
            "calibration": calibration, "seed": seed, "changing": changing,
            "outputs": outputs, "timer_step": timer_step,
            "match": match, "reference": observations[0], "rebuilt": observations[1]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    cases = [check(args.elf, action, band, single, gain, cal, seed)
             for action, band, single, gain, cal, seed in itertools.product(
                 ("enter", "enter-exit"), range(8), (0, 1), (0, 6, 12, 13, 0x7F, 0xFFFFFFFF),
                 range(4), (0, 0x12345678))]
    cases.extend(check(args.elf, "exit", single=single, seed=seed)
                 for single, seed in itertools.product((0, 1, 2, 255), (0, 0x12345678, 0xFFFFFFFF)))
    cases.extend(check(args.elf, "enter-exit", band, single, calibration=cal,
                       seed=0xA5A55A5A, changing=True, outputs=outputs, timer_step=timer_step)
                 for band, single, cal, outputs, timer_step in itertools.product(
                     range(8), (0, 2, 255), (0, 3), (False, True), (1, 1000)))
    if args.elf.read_bytes() != elf_bytes(args.elf):
        raise RuntimeError("ELF changed during comparisons")
    failures = sum(not case["match"] for case in cases)
    report = {"scope": __doc__, "reference_sha256": REFERENCE_SHA256,
              "elf_sha256": hashlib.sha256(elf_bytes(args.elf)).hexdigest(),
              "case_count": len(cases), "divergences": failures,
              "complete": failures == 0, "cases": cases}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"FRS entry/exit: {len(cases)} cases, {failures} divergences")
    return int(failures != 0)


if __name__ == "__main__":
    raise SystemExit(main())
