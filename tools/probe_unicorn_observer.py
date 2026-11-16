#!/usr/bin/env python3
"""Reproduce the generic-RAM memory-hook discrepancy without patching firmware."""
import hashlib
import json
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.arm_const import (UC_CPU_ARM_CORTEX_M4, UC_ARM_REG_SP, UC_ARM_REG_LR,
                               UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                               UC_ARM_REG_PC, UC_ARM_REG_CPSR)

raw = (Path(__file__).resolve().parents[2] / "phyb").read_bytes()
expected = "15f3ce1c30b82e3f87c45974284ccfa68e10928fea0f09a1a46376ac04cd50d4"
assert hashlib.sha256(raw).hexdigest() == expected
results = []
for observer in ("none", "generic RAM", "peripheral ranges"):
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    cpu.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M4)
    cpu.mem_map(0, 0x20000)
    cpu.mem_map(0x20000000, 0x10000)
    cpu.mem_write(0, raw[:0x88D4])
    cpu.mem_write(0x20002001, bytes(range(52)))
    cpu.reg_write(UC_ARM_REG_SP, 0x20008000)
    cpu.reg_write(UC_ARM_REG_LR, 0x1F001)
    for register, value in ((UC_ARM_REG_R0, 0x20001000),
                            (UC_ARM_REG_R1, 0x20002001), (UC_ARM_REG_R2, 52)):
        cpu.reg_write(register, value)
    if observer != "none":
        begin, end = (1, 0) if observer == "generic RAM" else (0x4D000000, 0x4D27FFFF)
        cpu.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, lambda *args: None,
                     begin=begin, end=end)
    cpu.emu_start(0x5C71, 0x1F000, count=10000)
    results.append({"observer": observer, "pc": hex(cpu.reg_read(UC_ARM_REG_PC)),
                    "remaining_length": hex(cpu.reg_read(UC_ARM_REG_R2)),
                    "cpsr": hex(cpu.reg_read(UC_ARM_REG_CPSR)),
                    "copied_bytes_match": bytes(cpu.mem_read(0x20001000, 52)) == bytes(range(52))})
print(json.dumps({"reference_sha256": expected, "results": results}, indent=2))
assert all(r["pc"] == "0x1f000" and r["copied_bytes_match"]
           for r in results if r["observer"] != "generic RAM")
