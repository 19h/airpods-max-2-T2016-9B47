#!/usr/bin/env python3
"""NVV-installed clock callbacks: fixed code address and callback-state controls.

IRQ5 is called as a scalar routine. A synthetic callback in otherwise unused
modeled ROM records entry registers; original firmware bytes are not patched.
No hardware IRQ delivery or physical callback reachability is established.
"""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

from elftools.elf.elffile import ELFFile
from unicorn import UcError
import unicorn.arm_const as uarm

from arm_semantics import Machine, REFERENCE_SHA256, ROOT, TEST, UC_HOOK_CODE, elf_bytes
from instruction_observer import InstructionObserver

FIXTURE = """.syntax unified
.thumb
.text
.global callback_observe
.thumb_func
callback_observe:
 push.w {r12}
 ldr.w r12, callback_destination
 stm.w r12!, {r0-r11}
 ldr r0, [sp]
 str.w r0, [r12, #0]
 add r0, sp, #4
 str.w r0, [r12, #4]
 str.w lr, [r12, #8]
 mrs r0, xpsr
 str.w r0, [r12, #12]
 pop.w {r12}
 bx lr
 .p2align 2
callback_destination:
 .word 0x20000300
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    assembly = args.build / "callback-observe.S"
    obj = args.build / "callback-observe.o"
    assembly.write_text(FIXTURE)
    subprocess.run(["clang", "-target", "armv7em-none-eabi", "-mcpu=cortex-m4", "-c",
                    str(assembly), "-o", str(obj)], cwd=ROOT, check=True)
    with obj.open("rb") as stream:
        code = ELFFile(stream).get_section_by_name(".text").data()
    cases = []
    for pointer in (0, 0x41D, 0x1E001):
        for seed in (0, 1, 0x12345678, 0xFFFFFFFF):
            modes = []
            for observe in (True, False):
                observations = []
                for reference in (True, False):
                    m = Machine(reference, args.elf)
                    for handle in m.observer_handles:
                        m.cpu.hook_del(handle)
                    if observe:
                        m.cpu.hook_add(UC_HOOK_CODE, InstructionObserver(m))
                    m.write(0x1E000, code)
                    record = struct.pack("<HHII", 0xC80B, 8, 0x4D289900, pointer)
                    blob = struct.pack("<HHI", len(record), 0x4E56, 0) + record
                    m.write(TEST + 0x100, blob)
                    m.call(0x50B0 if reference else "nvv_parse_blob", [TEST + 0x100, len(blob)])
                    if m.read(0x4D289900, 4) != struct.pack("<I", pointer):
                        raise RuntimeError("NVV did not install the callback")
                    m.word(0x4D159014, seed)
                    for index in range(4, 13):
                        m.cpu.reg_write(getattr(uarm, "UC_ARM_REG_R" + str(index)),
                                        (seed + index * 0x1234567) & 0xFFFFFFFF)
                    error = None
                    try:
                        m.call(0x28C if reference else "clk_wakeup_isr", [seed] * 4)
                    except (UcError, RuntimeError) as exc:
                        error = type(exc).__name__ + ": " + str(exc)
                    observations.append({"error": error, "pc": hex(m.cpu.reg_read(uarm.UC_ARM_REG_PC)),
                                         "callback_cell": m.read(0x4D289900, 4).hex(),
                                         "callback_entry_words": list(struct.unpack("<16I", m.read(TEST + 0x300, 64))),
                                         "state": m.read(0x4D280000, 0xD178).hex(),
                                         "clock_irq": m.read(0x4D159014, 4).hex(),
                                         "nvic_disable": m.read(0xE000E180, 4).hex(),
                                         "transactions": list(m.transactions)})
                match = observations[0] == observations[1] and all(result["error"] is None for result in observations)
                if observations[0]["state"] == observations[1]["state"]:
                    digest = hashlib.sha256(bytes.fromhex(observations[0]["state"])).hexdigest()
                    for result in observations:
                        result["state"] = {"sha256": digest, "byte_count": 0xD178}
                cases.append({"pointer": hex(pointer), "seed": seed, "observer": observe,
                              "match": match, "reference": observations[0], "rebuilt": observations[1]})
                modes.append([{key: value for key, value in result.items() if key != "transactions"}
                              for result in observations])
            if all(result["error"] is None for mode in modes for result in mode) and modes[0] != modes[1]:
                raise RuntimeError("observer changed a returned callback state")
    frozen = elf_bytes(args.elf)
    if frozen != args.elf.read_bytes():
        raise RuntimeError("ELF changed during callback comparisons")
    failures = sum(not case["match"] for case in cases)
    report = {"scope": __doc__, "reference_sha256": REFERENCE_SHA256,
              "elf_sha256": hashlib.sha256(frozen).hexdigest(),
              "fixture_hex": code.hex(), "fixture_address": "0x1e000",
              "case_count": len(cases), "divergences": failures, "complete": failures == 0,
              "cases": cases}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Clock callbacks: {len(cases)} cases, {failures} divergences")
    return int(failures != 0)


if __name__ == "__main__":
    raise SystemExit(main())
