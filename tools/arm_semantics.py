#!/usr/bin/env python3
"""Execute reference Thumb instructions and the rebuilt ARM ELF.

This is an instruction-level differential harness, not a hardware emulator or
an equivalence proof. Peripheral values are supplied by each case. Every case
states which return values, RAM bytes and peripheral transactions it observes.
Dependencies: unicorn==2.1.4, pyelftools==0.33, capstone==5.0.9.
"""
from __future__ import annotations

import argparse
import io
from functools import lru_cache
import hashlib
import json
import re
import struct
from pathlib import Path

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn import UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE, UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
    UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_C1_C0_2,
    UC_ARM_REG_FPEXC,
    UC_ARM_REG_MSP, UC_ARM_REG_PSP, UC_ARM_REG_CONTROL, UC_ARM_REG_R7,
    UC_CPU_ARM_CORTEX_M4,
    UC_ARM_REG_BASEPRI, UC_ARM_REG_PRIMASK,
    UC_ARM_REG_R4, UC_ARM_REG_R11, UC_ARM_REG_S16, UC_ARM_REG_IPSR,
)

ROOT = Path(__file__).resolve().parents[1]
REFERENCE_SHA256 = "15f3ce1c30b82e3f87c45974284ccfa68e10928fea0f09a1a46376ac04cd50d4"
TEST = 0x20000000
STACK = TEST + 0x8000
STOP = 0x1F000
ARG_REGS = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)


@lru_cache(maxsize=4)
def elf_bytes(path: Path):
    """Freeze each input ELF for the duration of a comparison process."""
    return path.read_bytes()


class Machine:
    def __init__(self, reference: bool, elf: Path):
        self.reference = reference
        self.cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.cpu.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M4)
        for base, size in ((0, 0x20000), (TEST, 0x10000),
                           (0x4D000000, 0x400000), (0xE0000000, 0x10000)):
            self.cpu.mem_map(base, size)
        self.symbols = {}
        if reference:
            raw = (ROOT.parent / "phyb").read_bytes()
            if hashlib.sha256(raw).hexdigest() != REFERENCE_SHA256:
                raise ValueError("reference firmware hash mismatch")
            self.write(0, raw[:0x88D4])
            # The fwsg file padding is absent from the runtime reload mapping.
            self.write(0x88D2, raw[0x88E0:0x88E0 + 0x98BD])
            self.write(0x4D280000, raw[0x88E0:0x88E0 + 0x98BD])
        else:
            with io.BytesIO(elf_bytes(elf)) as stream:
                image = ELFFile(stream)
                for seg in image.iter_segments():
                    if seg["p_type"] == "PT_LOAD" and seg["p_filesz"]:
                        self.write(seg["p_vaddr"], seg.data())
                        if seg["p_vaddr"] >= 0x4D280000 and seg["p_paddr"] < 0x20000:
                            self.write(seg["p_paddr"], seg.data())
                table = image.get_section_by_name(".symtab")
                for sym in table.iter_symbols():
                    if sym.name:
                        self.symbols[sym.name] = sym["st_value"]
        self.transactions = []
        self.read_plan = {}
        self.read_counts = {}
        # Unicorn 2.1.4 can corrupt Thumb IT-condition flags with generic RAM
        # memory hooks. Observe only the peripheral ranges we actually report.
        self.observer_handles = [
            self.cpu.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, self._memory,
                              begin=0x4D000000, end=0x4D27FFFF),
            self.cpu.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, self._memory,
                              begin=0xE000E000, end=0xE000FFFF),
        ]
        self.cpu.reg_write(UC_ARM_REG_C1_C0_2, 0xF00000)
        self.cpu.reg_write(UC_ARM_REG_FPEXC, 0x40000000)

    def _memory(self, cpu, access, address, size, value, user_data):
        # Captures externally visible accesses, excluding shared RAM and ROM.
        if 0x4D000000 <= address < 0x4D280000 or 0xE000E000 <= address < 0xE0010000:
            from unicorn import UC_MEM_WRITE
            kind = "write" if access == UC_MEM_WRITE else "read"
            if kind == "read":
                ordinal = self.read_counts.get(address, 0) + 1
                self.read_counts[address] = ordinal
                response = self.read_plan.get(address, {}).get(ordinal)
                if response is not None:
                    if isinstance(response, tuple):
                        current = int.from_bytes(cpu.mem_read(address, size), "little")
                        mask, replacement = response
                        response = (current & ~mask) | (replacement & mask)
                    cpu.mem_write(address, int(response).to_bytes(size, "little"))
                value = int.from_bytes(cpu.mem_read(address, size), "little")
            self.transactions.append((kind, address, size, value))

    def write(self, address: int, data: bytes):
        self.cpu.mem_write(address, bytes(data))

    def word(self, address: int, value: int):
        self.write(address, struct.pack("<I", value & 0xFFFFFFFF))

    def read(self, address: int, size: int) -> bytes:
        return bytes(self.cpu.mem_read(address, size))

    def call(self, target: int | str, args=(), limit=200000):
        if isinstance(target, str):
            target = self.symbols[target]
        self.cpu.reg_write(UC_ARM_REG_SP, STACK)
        self.cpu.reg_write(UC_ARM_REG_LR, STOP | 1)
        for reg in ARG_REGS:
            self.cpu.reg_write(reg, 0)
        for index, arg in enumerate(args):
            if index < 4:
                self.cpu.reg_write(ARG_REGS[index], arg & 0xFFFFFFFF)
            else:
                self.word(STACK + 4 * (index - 4), arg)
        self.transactions.clear()
        self.read_counts.clear()
        self.cpu.emu_start(target | 1, STOP, count=limit)
        if self.cpu.reg_read(UC_ARM_REG_PC) != STOP:
            raise RuntimeError(f"instruction limit at {self.cpu.reg_read(UC_ARM_REG_PC):#x}")
        return self.cpu.reg_read(UC_ARM_REG_R0)


def format_cases():
    yield "%d", [42]
    yield "%s:%d:%u:%x:%#x", [TEST + 0x300, -7, 42, 42, 42]
    for conversion in ("d", "i", "u", "x", "X"):
        for length in ("", "h", "hh", "l", "ll", "z"):
            for value in (0, 1, 0x7F, 0xFF, 0x7FFF, 0xFFFF, 0x7FFFFFFF, 0xFFFFFFFF):
                words = [0, value, 0x12345678] if length == "ll" else [value, 0x12345678]
                yield "%" + length + conversion, words
    for fmt in ("%05d", "%+05d", "%#08x", "%-8d", "%8.4d", "%.0d",
                "%.32d", "%.64d", "%*.*d", "%c", "%5c", "%s", "%.2s",
                "%q", "%%", "%", "%a", "%e", "%f", "%g", "%A", "%E", "%F", "%G"):
        if fmt.endswith("s"):
            args = [TEST + 0x300]
        elif fmt == "%*.*d":
            args = [8, 4, 42]
        elif fmt[-1:] in "aefgAEFG":
            # The fourth argument consumes R3; the 8-byte double is aligned
            # to the stack by the target AAPCS variadic convention.
            args = [0, 0, 0x3FF80000]
        else:
            args = [0]
        yield fmt, args
    for conversion in ("d", "i", "u", "x", "X", "p"):
        for flags in ("+", "#", "+#0", "0-", "-0", "#-"):
            for length in ("", "h", "hh", "hl", "lh", "ll", "hhl", "lhh"):
                words = [0, 0xFFFFFFFF, 0x80000000] if length == "ll" else [0xFFFFFFFF, 0x12345678]
                yield "%" + flags + "8.4" + length + conversion, words
    for fmt in ("%n", "%n:%d", "%5n:%u", "%h+5d", "%5-hd", "%...8d", "%05s", "%8.2s"):
        yield fmt, [TEST + 0x300] if fmt.endswith("s") else [42, 17, 0]


def check_formatters(elf: Path):
    results = []
    for fmt, args in format_cases():
        for capacity in (0, 1, 5, 80):
            observation = []
            for reference in (True, False):
                m = Machine(reference, elf)
                m.write(TEST, b"\xa5" * 128)
                m.write(TEST + 0x200, fmt.encode() + b"\0")
                m.write(TEST + 0x300, b"abcdefg\0")
                try:
                    ret = m.call(0x69E6 if reference else "snprintf",
                                 [TEST, capacity, TEST + 0x200, *args])
                    observation.append({"return": ret, "buffer": m.read(TEST, 128).hex()})
                except (UcError, RuntimeError) as exc:
                    observation.append({"error": str(exc)})
            results.append({"format": fmt, "args": args, "capacity": capacity,
                            "reference": observation[0], "rebuilt": observation[1],
                            "match": observation[0] == observation[1]})
    return results


def check_reset(elf: Path):
    states = []
    for reference in (True, False):
        m = Machine(reference, elf)
        stop = 0x18C if reference else m.symbols["rtk_boot"] & ~1
        def at_router(cpu, address, size, user_data):
            if address == stop:
                cpu.emu_stop()
        m.cpu.hook_add(UC_HOOK_CODE, at_router)
        # This deliberately starts with the image's vector-table value, zero.
        m.cpu.reg_write(UC_ARM_REG_MSP, 0)
        m.cpu.reg_write(UC_ARM_REG_CONTROL, 0)
        try:
            m.cpu.emu_start((0x200 if reference else m.symbols["Reset_Handler"]) | 1,
                            STOP, count=10000)
            states.append({
                "reached_router": m.cpu.reg_read(UC_ARM_REG_PC) == stop,
                "msp": m.cpu.reg_read(UC_ARM_REG_MSP),
                "psp": m.cpu.reg_read(UC_ARM_REG_PSP),
                "control": m.cpu.reg_read(UC_ARM_REG_CONTROL),
                "r7": m.cpu.reg_read(UC_ARM_REG_R7),
                "stacks": m.read(0x4D289AC8, 0x800).hex(),
            })
        except (UcError, RuntimeError) as exc:
            states.append({"error": str(exc)})
    return {"name": "reset from zero MSP through boot router",
            "reference": states[0], "rebuilt": states[1], "match": states[0] == states[1]}


def check_tx(elf: Path, band=0, packet_type=0, alternate=False, power_mode=0,
             calibration=0, register_seed=0, analog=0, hf=False, channel=0, alt_rate=0, hf_stages=None, cmd0_reads=None, dynamic_pattern=False, override_flag=0, dpd_profile=None, dpd_armed=1, dpd_tables=True, dpd_integrated=False, dpd_gtb_plan=None, dpd_actuation_pointer=None, dpd_observe=True):
    observations = []
    if hf_stages is not None:
        baseline = check_tx(elf, band, packet_type, alternate, power_mode,
                            calibration, register_seed, analog, hf, channel, alt_rate)
        count = sum(t[0] == "read" and t[1] == 0x4D154038
                    for t in baseline["reference"]["transactions"])

    for reference in (True, False):
        m = Machine(reference, elf)
        if dpd_profile is not None:
            for handle in m.observer_handles:
                m.cpu.hook_del(handle)
            if dpd_observe:
                from instruction_observer import InstructionObserver
                m.cpu.hook_add(UC_HOOK_CODE, InstructionObserver(m))
        m.call(0x3A68 if reference else "txpwr_init")
        for address, size in ((0x4D000000, 0x400), (0x4D140000, 0x1000),
                              (0x4D141000, 0x700), (0x4D150000, 0x1B00),
                              (0x4D154000, 0xD00)):
            m.write(address, struct.pack("<I", register_seed) * (size // 4))
        m.write(0x4D289000, bytes(0x200))
        m.word(0x4D289930, power_mode)
        m.word(0x4D2892C4, power_mode)
        if override_flag:
            # A valid NVV record makes noncanonical flag bytes firmware-reachable.
            record = struct.pack("<HHII", 0xC80B, 8, 0x4D289938, override_flag)
            blob = struct.pack("<HHI", len(record), 0x4E56, 0) + record
            m.write(TEST + 0x100, blob)
            m.call(0x50B0 if reference else "nvv_parse_blob", [TEST + 0x100, len(blob)])
            if m.read(0x4D289938, 1) != bytes([override_flag]):
                raise RuntimeError("NVV fixture did not install the requested TX flag byte")
        m.write(0x4D289934, bytes([not hf]))
        m.word(0x4D154030, (band << 14) | (int(alternate) << 8) | channel | (alt_rate << 17))
        m.word(0x4D154034, packet_type << 18)
        table = TEST + 0x1000
        m.write(table, bytes(((i * 37 + 11 + (i >> 8) * 53) & 255) if dynamic_pattern else 0
                             for i in range(0x1000)))
        for offset in (0x18, 0x1C, 0x2C, 0x30, 0x34):
            m.word(0x4D28CF48 + offset, table)
        if calibration:
            offset = {1: 0x10, 2: 0xC, 3: 8, 4: 0x14}[calibration]
            m.word(0x4D28CF48 + offset, table + 0x800)
            m.write(table + 0x800, struct.pack("<32H", *[0x100 + i * 37 for i in range(32)]))
        if analog:
            m.word(0x4D28CF48, TEST + 0x2000)
            m.word(0x4D28CF4C, TEST + 0x2200)
            m.write(TEST + 0x2000, struct.pack("<128H", *[2048 + i * 3 for i in range(128)]))
            m.write(TEST + 0x2200, bytes(4 + i * 3 for i in range(16)))
            if analog in (2, 4):
                m.word(0x4D28CF48 + 0x44, TEST + 0x2300)
                m.write(TEST + 0x2300, bytes(80 + i * 7 for i in range(16)))
            if analog in (3, 4):
                m.word(0x4D28CF48 + 0x40, TEST + 0x2400)
                m.write(TEST + 0x2400, bytes(96 + i * 5 for i in range(16)))
        if hf:
            m.word(0x4D28CF48 + 0x20, TEST + 0x3000)
            m.write(TEST + 0x3000, bytes((i * 37 + 11) & 255 for i in range(0x1000)))
        setup_transactions = []
        if dpd_profile is not None:
            m.write(TEST + 0x5000, bytes((i * 17 + 3) & 255 for i in range(0x3C0)))
            m.write(TEST + 0x6000, bytes((i * 29 + 7) & 255 for i in range(0x780)))
            words = [TEST + 0x6000 if dpd_tables else 0, int(bool(dpd_profile & 1)), 1,
                     TEST + 0x5000 if dpd_tables else 0, 3 if dpd_profile & 4 else 0,
                     int(bool(dpd_profile & 2)), 2, dpd_armed, 0, 0]
            m.write(0x4D28AA88, struct.pack("<10I", *words))
            m.write(0x4D28AAB0, bytes([0xA5]) * 0xBC0)
            m.word(0x4D28B670, 3)
            m.write(0x4D148000, bytes((i * 37 + 11) & 255 for i in range(0xBC0)))
            m.word(0x4D141604, 10)
            if dpd_integrated:
                if dpd_tables:
                    act = bytes((i * 17 + 3) & 255 for i in range(0x3C0))
                    test = bytes((i * 29 + 7) & 255 for i in range(0x780))
                    records = struct.pack("<HH", 0xD176, len(act)) + act + struct.pack("<HH", 0xD176, len(test)) + test
                    blob = struct.pack("<HHI", len(records), 0x4E56, 0) + records
                    m.write(TEST + 0x4000, blob)
                    m.call(0x50B0 if reference else "nvv_parse_blob", [TEST + 0x4000, len(blob)])
                    setup_transactions.extend(m.transactions)
                for bit, command in ((1, (1, 1, 1, 0)), (2, (2, 1, 2, 0)), (4, (0, 2, 3, 3))):
                    if dpd_profile & bit:
                        m.write(TEST + 0x200, struct.pack("<4I", *command))
                        m.word(TEST + 0x300, 8)
                        m.call(0x3684 if reference else "dpd_handle_command", [TEST + 0x200, TEST + 0x280, TEST + 0x300])
                        setup_transactions.extend(m.transactions)
            if dpd_actuation_pointer is not None:
                record = struct.pack("<HHII", 0xC80B, 8, 0x4D28AA94,
                                     dpd_actuation_pointer)
                blob = struct.pack("<HHI", len(record), 0x4E56, 0) + record
                m.write(TEST + 0x4000, blob)
                m.call(0x50B0 if reference else "nvv_parse_blob",
                       [TEST + 0x4000, len(blob)])
                setup_transactions.extend(m.transactions)
                if m.read(0x4D28AA94, 4) != struct.pack("<I", dpd_actuation_pointer):
                    raise RuntimeError("NVV fixture did not install the DPD actuation pointer")
        if dpd_gtb_plan is not None:
            m.read_plan[0x4D141604] = {i + 1: value for i, value in enumerate(dpd_gtb_plan)}
        if hf_stages is not None:
            m.read_plan[0x4D154038] = {
                count - 1: (0x3000, hf_stages[0] << 12),
                count: (0x3000, hf_stages[1] << 12)}
        if cmd0_reads is not None:
            m.read_plan[0x4D154030] = {i + 1: value for i, value in enumerate(cmd0_reads)}
        try:
            ret = m.call(0x2BB8 if reference else "tx_setup_isr")
            transactions = list(m.transactions)
            result_status = None
            result_response = None
            if dpd_integrated:
                m.write(TEST + 0x200, struct.pack("<4I", 3, 0, 0, 0))
                m.word(TEST + 0x300, 8)
                result_status = m.call(0x3684 if reference else "dpd_handle_command", [TEST + 0x200, TEST + 0x280, TEST + 0x300])
                transactions.extend(m.transactions)
                result_response = m.read(TEST + 0x280, 8).hex()
            observations.append({
                "return": ret,
                "transactions": transactions, "setup_transactions": setup_transactions,
                "result_status": result_status, "result_response": result_response,
                "state": m.read(0x4D280000, 0xD178).hex(),
                "test_memory": m.read(TEST, 0x7000 if dpd_profile is not None else 0x5000).hex(),
                "hardware_buffer": None if dpd_profile is None else m.read(0x4D148000, 0xBC0).hex(),
                "trace": m.read(0x4D289000, 0x200).hex(),
                "telemetry": m.read(0x4D287008, 0x2C).hex(),
            })
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc)})
    case = {"name": "tx_setup", "band": band, "packet_type": packet_type,
            "alternate": alternate, "power_mode": power_mode,
            "calibration": calibration, "register_seed": register_seed,
            "analog": analog, "hf": hf, "channel": channel, "alt_rate": alt_rate,
            "hf_stages": hf_stages, "cmd0_reads": cmd0_reads,
            "dynamic_pattern": dynamic_pattern, "override_flag": override_flag,
            "dpd_profile": dpd_profile, "dpd_armed": dpd_armed, "dpd_tables": dpd_tables,
            "dpd_integrated": dpd_integrated, "dpd_gtb_plan": dpd_gtb_plan,
            "dpd_actuation_pointer": dpd_actuation_pointer, "dpd_observe": dpd_observe,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1] and "error" not in observations[0]}
    if case["match"]:
        for side in ("reference", "rebuilt"):
            for key in ("state", "test_memory"):
                data = bytes.fromhex(case[side][key])
                case[side][key] = {"sha256": hashlib.sha256(data).hexdigest(), "byte_count": len(data)}
    return case


def check_memory_observer(elf: Path):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        payload = bytes(range(52))
        m.write(TEST + 0x1000, bytes([0xA5]) * 256)
        m.write(TEST + 0x2001, payload)
        try:
            result = m.call(0x5C70 if reference else "memmove", [TEST + 0x1000, TEST + 0x2001, 52])
            observations.append({"return": result, "output": m.read(TEST + 0x1000, 256).hex(),
                                 "transactions": m.transactions})
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc)})
    return {"name": "memory observer preserves Thumb IT copy/return behavior",
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1]
                     and observations[0].get("return") == TEST + 0x1000
                     and observations[0].get("output") == (bytes(range(52)) + bytes([0xA5]) * 204).hex()
                     and observations[0].get("transactions") == []}


def check_exception_entry(elf: Path, exc_return: int, svc=False):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        m.write(0x4D289AC8, bytes((i * 37 + 11) & 255 for i in range(0x800)))
        m.cpu.reg_write(UC_ARM_REG_MSP, 0x4D28A288)
        m.cpu.reg_write(UC_ARM_REG_PSP, 0x4D289E48)
        m.cpu.reg_write(UC_ARM_REG_IPSR, 11 if svc else 3)
        m.cpu.reg_write(UC_ARM_REG_LR, exc_return)
        m.cpu.reg_write(UC_ARM_REG_R0, 0x13579BDF)
        for i in range(8):
            m.cpu.reg_write(UC_ARM_REG_R4 + i, 0x44440000 + i * 0x1111)
        for i in range(16):
            m.cpu.reg_write(UC_ARM_REG_S16 + i, 0x3F800000 + i * 0x10000)
        original_stop = 0x624C if svc and exc_return & 4 else 0x6314
        stop = original_stop if reference else m.symbols[
            "rtk_pendsv_switch" if svc and exc_return & 4 else "rtk_handle_exception"] & ~1
        def boundary(cpu, address, size, user_data):
            if address == stop:
                cpu.emu_stop()
        m.cpu.hook_add(UC_HOOK_CODE, boundary)
        try:
            target = (0x5C04 if svc else 0x5BB4) if reference else m.symbols[
                "rtk_svcall_entry" if svc else "rtk_exception_entry"]
            m.cpu.emu_start(target | 1, STOP, count=10000)
            observations.append({
                "reached_boundary": m.cpu.reg_read(UC_ARM_REG_PC) == stop,
                "frame": m.cpu.reg_read(UC_ARM_REG_R0),
                "exception": None if svc and exc_return & 4 else m.cpu.reg_read(UC_ARM_REG_R1),
                "msp": m.cpu.reg_read(UC_ARM_REG_MSP),
                "psp": m.cpu.reg_read(UC_ARM_REG_PSP),
                "stacks": m.read(0x4D289AC8, 0x800).hex(),
            })
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc), "pc": m.cpu.reg_read(UC_ARM_REG_PC)})
    return {"name": "svc entry" if svc else "fault entry", "exc_return": exc_return,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1]
                     and observations[0].get("reached_boundary", False)}


def check_rx(elf: Path, band=0, packet_type=0, alternate=False, calibration=False):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        for address, size in ((0x4D000000, 0x400), (0x4D150000, 0x1B00), (0x4D154000, 0xD00)):
            m.write(address, struct.pack("<I", 0xA5A55A5A) * (size // 4))
        if calibration:
            table = TEST + 0x1000
            m.write(table, bytes((i * 17 + 19) & 255 for i in range(0x1000)))
            for offset in (0x64, 0x6C, 0x70, 0x74, 0x78, 0x7C, 0x80, 0x84, 0x88, 0x8C, 0x90):
                m.word(0x4D28CF48 + offset, table)
        try:
            m.call(0x269C if reference else "rx_init")
            m.write(0x4D289000, bytes(0x200))
            m.word(0x4D154030, (band << 14) | (int(alternate) << 8))
            m.word(0x4D154034, packet_type << 18)
            ret = m.call(0x1C10 if reference else "rx_setup_isr")
            observations.append({
                "return": ret, "transactions": m.transactions,
                "state": m.read(0x4D28A9C8, 0xC0).hex(),
                "trace": m.read(0x4D289000, 0x200).hex(),
                "telemetry": m.read(0x4D287008, 0x2C).hex(),
            })
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc)})
    return {"name": "rx_setup", "band": band, "packet_type": packet_type,
            "alternate": alternate, "calibration": calibration,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1]
                     and "error" not in observations[0]}


def check_readback(elf: Path, name: str, capacity: int, null_argument=0, status_base=None, tx_count=3, rx_count=2, cfo_sum=-65):
    original, rebuilt = {
        "version": (0x4C9C, "version_info_read"),
        "telemetry": (0x4BC4, "telemetry_read_and_reset"),
        "loopback": (0x48DC, "loopback_read_stats"),
    }[name]
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        m.write(TEST, b"\xA5" * 0x200)
        m.write(TEST + 0x300, b"DAUG\0")
        m.word(0x4D28CFA8, TEST + 0x300)
        m.word(0x4D28CF44, 4)
        m.word(0x4D289A38, 0x12345678)
        m.word(0x4D289A3C, 0x9ABCDEF0)
        if status_base is not None:
            m.write(status_base, bytes(0x2100))
            record = struct.pack("<HHII", 0xC80B, 8, 0x4D2895C4, status_base)
            blob = struct.pack("<HHI", len(record), 0x4E56, 0) + record
            m.write(TEST + 0x800, blob)
            m.call(0x50B0 if reference else "nvv_parse_blob", [TEST + 0x800, len(blob)])
            if m.read(0x4D2895C4, 4) != struct.pack("<I", status_base):
                raise RuntimeError("status NVV fixture did not install the requested cell")
        m.word(0x4D28B674, tx_count)
        m.word(0x4D28A9E4, rx_count)
        m.word(0x4D28A9D4, cfo_sum)
        m.write(0x4D28A9E8, struct.pack("<4I", 1, 2, 3, 4))
        m.write(0x4D28A9DC, struct.pack("<2i", 511, -511))
        m.write(0x4D28996C, struct.pack("<13i", 2, -100, 50, 100, -50,
                                    10, 20, 30, 40, -10, -20, -30, -40))
        m.write(0x4D2899A8, b"\x01")
        try:
            result = m.call(original if reference else rebuilt,
                            [0 if null_argument == 1 else TEST,
                             0 if null_argument == 2 else TEST + 0x180, capacity])
            observations.append({
                "return": result, "transactions": m.transactions,
                "relocated_status": None if status_base is None else m.read(status_base, 0x2100).hex(),
                "output": m.read(TEST, 0x200).hex(),
                "shared_status": m.read(0x4D287008, 0x100).hex(),
                "trace": m.read(0x4D289000, 0x200).hex(),
                "loopback": m.read(0x4D28996C, 0x34).hex(),
                "rx": m.read(0x4D28A9C8, 0xC0).hex(),
                "tx": m.read(0x4D28B674, 8).hex(),
            })
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc)})
    return {"name": name + " readback", "capacity": capacity, "null_argument": null_argument,
            "status_base": status_base, "tx_count": tx_count, "rx_count": rx_count, "cfo_sum": cfo_sum,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1] and "error" not in observations[0]}






def check_dpd(elf: Path, operation, opcode=0, command_address=TEST + 0x100,
              out_address=TEST + 0x400, length_address=TEST + 0x600,
              capacity=8, capture_gtb=0, mode=3):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        for handle in m.observer_handles:
            m.cpu.hook_del(handle)
        from instruction_observer import InstructionObserver
        m.cpu.hook_add(UC_HOOK_CODE, InstructionObserver(m))
        m.write(TEST, bytes([0xA5]) * 0x7000)
        m.write(0x4D28AA88, bytes(0xBEC))
        m.word(0x4D28AAA8, TEST + 0x900)
        m.word(0x4D28AAAC, 0x2F0)
        m.word(0x4D28AA98, capture_gtb)
        m.word(0x4D28B670, mode)
        m.write(0x4D148000, bytes((i * 37 + 11) & 255 for i in range(0xBC0)))
        m.word(0x4D141000, 0xA5A55A5A)
        m.word(0x4D141604, 10)
        m.word(0x4D141608, 0x12345678)
        m.word(0xE000E018, 0x123456)
        if length_address:
            m.word(length_address, capacity)
        m.write(command_address, struct.pack("<4I", opcode, 2, 7, 3))
        try:
            result = m.call(0x3684 if reference else "dpd_handle_command",
                            [command_address, out_address, length_address]) if operation == "command" else m.call(
                                0x3598 if reference else "dpd_capture_done_isr")
            observations.append({"return": result if operation == "command" else None,
                                 "transactions": m.transactions,
                                 "state": m.read(0x4D280000, 0xD178).hex(),
                                 "test_memory": m.read(TEST, 0x7000).hex(),
                                 "command": m.read(command_address, 16).hex(),
                                 "out": None if out_address == 0 else m.read(out_address, 8).hex(),
                                 "length": None if length_address == 0 else m.read(length_address, 4).hex(),
                                 "hardware_buffer": m.read(0x4D148000, 0xBC0).hex()})
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc)})
    return {"name": "DPD " + operation, "opcode": opcode, "command_address": command_address,
            "out_address": out_address, "length_address": length_address, "capacity": capacity,
            "capture_gtb": capture_gtb, "mode": mode,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1] and "error" not in observations[0]}


def check_mac_crash(elf: Path, mailbox_base, descriptor_delta=None, source_address=None):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        m.write(mailbox_base, bytes([0xA5]) * 0x800)
        descriptor = TEST + 0x3000 if source_address is None else source_address
        if descriptor_delta is not None:
            descriptor = mailbox_base + 0x5C1 + descriptor_delta
        m.write(descriptor, struct.pack("<5I", 0x12345678, 0x23456789, 0x3456789A, 0x456789AB, 0x56789ABC))
        record = struct.pack("<HHII", 0xC80B, 8, 0x4D28929C, mailbox_base)
        blob = struct.pack("<HHI", len(record), 0x4E56, 0) + record
        m.write(TEST + 0x800, blob)
        m.call(0x50B0 if reference else "nvv_parse_blob", [TEST + 0x800, len(blob)])
        if m.read(0x4D28929C, 4) != struct.pack("<I", mailbox_base):
            raise RuntimeError("MAC NVV fixture did not install the requested cell")
        m.word(0x4D154C60, 0xA5A55A5A)
        try:
            m.call(0x4EC if reference else "mac_send_crash_notification", [descriptor])
            observations.append({"transactions": m.transactions,
                                 "mailbox": m.read(mailbox_base, 0x800).hex(),
                                 "descriptor": m.read(descriptor, 20).hex(),
                                 "state": m.read(0x4D280000, 0xD178).hex()})
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc)})
    return {"name": "MAC crash notification", "mailbox_base": mailbox_base,
            "descriptor_delta": descriptor_delta, "source_address": source_address,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1] and "error" not in observations[0]}


def check_trace(elf: Path, ring_address=TEST + 0x1000, cursor=0, event_offset=None,
                null_event=False, freeze=0, primask=0, changing_cursor=False, event_address=None, site=0x148):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        m.write(ring_address - 0x100, bytes([0xA5]) * 0x1000)
        m.word(ring_address, cursor)
        payload = struct.pack("<6I", 1, 7, 0x1234, 0x5678, 0x9ABC, 0xDEF0)
        event = TEST + 0x3000 if event_address is None else event_address
        if event_offset is None:
            m.write(event, payload)
        else:
            event = ring_address + event_offset
        m.word(0x4D28A2C8, freeze)
        m.word(0xE000E018, 0x123456)
        m.cpu.reg_write(UC_ARM_REG_PRIMASK, primask)
        record = struct.pack("<HHII", 0xC80B, 8, 0x4D289298, ring_address)
        blob = struct.pack("<HHI", len(record), 0x4E56, 0) + record
        m.write(TEST + 0x800, blob)
        m.call(0x50B0 if reference else "nvv_parse_blob", [TEST + 0x800, len(blob)])
        if m.read(0x4D289298, 4) != struct.pack("<I", ring_address):
            raise RuntimeError("trace NVV fixture did not install the requested cell")
        if changing_cursor:
            m.read_plan[ring_address] = {i: i - 1 for i in range(1, 9)}
        try:
            m.call(0x314 if reference else "trace_log",
                   [((0x4D28D178 + site) & 0xFFFFFFFF) if reference else site,
                    0 if null_event else event])
            observations.append({"transactions": m.transactions,
                                 "primask": m.cpu.reg_read(UC_ARM_REG_PRIMASK),
                                 "ring_window": m.read(ring_address - 0x100, 0x1000).hex(),
                                 "state": m.read(0x4D280000, 0xD178).hex(),
                                 "event": m.read(event, 24).hex()})
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc)})
    return {"name": "trace_log", "ring_address": ring_address, "cursor": cursor,
            "event_offset": event_offset, "null_event": null_event, "freeze": freeze,
            "primask": primask, "changing_cursor": changing_cursor, "event_address": event_address,
            "site_offset": site, "original_site_argument": (0x4D28D178 + site) & 0xFFFFFFFF,
            "rebuilt_site_argument": site,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1] and "error" not in observations[0]}


def check_status_binding(elf: Path, operation, status_base, state=0, count=0, stop_early=False):
    observations = []
    original, rebuilt = {"get": (0x4D58, "warm_boot_detected"),
                         "set": (0x4D94, "warm_boot_set_state"),
                         "restore": (0x5824, "modem_restore_registers")}[operation]
    for reference in (True, False):
        m = Machine(reference, elf)
        m.write(status_base, bytes(0x2100))
        m.write(0x4D2899AC, struct.pack("<Q", 0xE91D98A54DBCC725))
        m.word(0x4D285000, state)
        m.word(status_base, 0 if state else 7)
        m.word(status_base + 4, count)
        for i in range(count):
            address = status_base + 4 if stop_early and i == 0 else 0x4D140020 + i * 4
            value = 0 if stop_early and i == 0 else 0x12345678 + i
            m.write(status_base + 8 + i * 8, struct.pack("<II", address, value))
        record = struct.pack("<HHII", 0xC80B, 8, 0x4D2895C4, status_base)
        blob = struct.pack("<HHI", len(record), 0x4E56, 0) + record
        m.write(TEST + 0x800, blob)
        m.call(0x50B0 if reference else "nvv_parse_blob", [TEST + 0x800, len(blob)])
        if m.read(0x4D2895C4, 4) != struct.pack("<I", status_base):
            raise RuntimeError("status NVV fixture did not install the requested cell")
        try:
            result = m.call(original if reference else rebuilt, [] if operation != "set" else [state])
            observations.append({"return": result if operation != "set" else None,
                                 "transactions": m.transactions,
                                 "state": m.read(0x4D280000, 0xD178).hex(),
                                 "relocated_status": m.read(status_base, 0x2100).hex()})
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc)})
    return {"name": "status binding " + operation, "status_base": status_base,
            "state": state, "count": count, "stop_early": stop_early,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1] and "error" not in observations[0]}


def check_boot(elf: Path, warm: bool, reload: bool, regions: int, separate: int, ccr: int):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        m.write(0x4D289900, bytes([0x5A]) * (0xD178 - 0x9900))
        m.word(0xE000ED14, ccr)
        m.word(0xE000ED90, (regions << 8) | separate)
        m.write(0x4D2898AC, bytes([reload]))
        stop = 0x58F0 if reference else m.symbols["btp_main"] & ~1
        def boundary(cpu, address, size, user_data):
            if address == stop:
                cpu.emu_stop()
        m.cpu.hook_add(UC_HOOK_CODE, boundary)
        m.cpu.reg_write(UC_ARM_REG_SP, STACK)
        m.cpu.reg_write(UC_ARM_REG_LR, STOP | 1)
        target = (0x124 if warm else 0x605C) if reference else m.symbols[
            "rtk_warm_boot" if warm else "rtk_cold_boot"]
        try:
            m.cpu.emu_start(target | 1, STOP, count=200000)
            observations.append({"reached_main": m.cpu.reg_read(UC_ARM_REG_PC) == stop,
                                 "failure_pc": None if m.cpu.reg_read(UC_ARM_REG_PC) == stop else m.cpu.reg_read(UC_ARM_REG_PC),
                                 "failure_r0_r1_r2": None if m.cpu.reg_read(UC_ARM_REG_PC) == stop else [m.cpu.reg_read(r) for r in ARG_REGS[:3]],
                                 "transactions": m.transactions,
                                 "state": m.read(0x4D289900, 0xD178 - 0x9900).hex(),
                                 "shared_memory": m.read(0x4D280000, 0x9900).hex(),
                                 "primask": m.cpu.reg_read(UC_ARM_REG_PRIMASK),
                                 "basepri": m.cpu.reg_read(UC_ARM_REG_BASEPRI)})
        except UcError as exc:
            observations.append({"error": str(exc), "pc": m.cpu.reg_read(UC_ARM_REG_PC)})
    return {"name": "warm boot" if warm else "cold boot", "reload": reload,
            "regions": regions, "separate": separate, "ccr": ccr,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1]
                     and observations[0].get("reached_main", False)}


def check_loopback(elf: Path, operation, relocated=False, control=0, tone=0, band=0, high=0, stats_offset=0, control_offset=0, stats_address=None, control_address=None, sample_count=2, sum_words=None, active=1, result_words=None, response_changes=False, request_address=None, read_destination=None, copied_address=None, capacity=0x34):
    observations = []
    original, rebuilt = {"configure": (0x46A4, "loopback_configure"),
                         "read": (0x48DC, "loopback_read_stats"),
                         "corr0": (0x44EC, "loopback_corr0_done_isr"),
                         "corr1": (0x4580, "loopback_corr1_done_isr"),
                         "setup": (0x43A8, "loopback_setup_tx_measurement")}[operation]
    for reference in (True, False):
        m = Machine(reference, elf)
        m.write(TEST, bytes(0x6000))
        for address, size in ((0x4D000000, 0x1000), (0x4D140000, 0x20000)):
            m.write(address, struct.pack("<I", 0xA5A55A5A) * (size // 4))
        stats = TEST + 0x3000 + stats_offset if relocated else 0x4D28996C
        controls = TEST + 0x3100 + control_offset if relocated else 0x4D2899A0
        if stats_address is not None:
            stats = stats_address
        if control_address is not None:
            controls = control_address
        if relocated:
            payload = struct.pack("<4I", 0x4D2895B4, stats, 0x4D2895B8, controls)
            record = struct.pack("<HH", 0xC80B, len(payload)) + payload
            blob = struct.pack("<HHI", len(record), 0x4E56, 0) + record
            m.write(TEST + 0x200, blob)
            m.call(0x50B0 if reference else "nvv_parse_blob", [TEST + 0x200, len(blob)])
        sums = (100, -100, 50, -50) if sum_words is None else sum_words
        m.write(stats, struct.pack("<13I", sample_count,
                                  *(value & 0xFFFFFFFF for value in sums),
                                  100, 100, 100, 100, *([0xFFFFFF9C] * 4)))
        m.write(controls, struct.pack("<2I", control, tone))
        m.write(0x4D2899A8, bytes([active]))
        if result_words is not None:
            # Literal bases 0x4574/0x4604 plus result offsets from 0x44F0/0x4584.
            for address, value in zip((0x4D1411E0, 0x4D1411E4, 0x4D1411E8, 0x4D1411EC), result_words):
                m.word(address, value)
        request = TEST + 0x100 if request_address is None else request_address
        m.write(request, struct.pack("<2I", control, tone))
        if relocated and (m.read(0x4D2895B4, 4) != struct.pack("<I", stats)
                          or m.read(0x4D2895B8, 4) != struct.pack("<I", controls)):
            raise RuntimeError("NVV fixture did not install the requested loopback cells")
        if response_changes:
            m.read_plan[controls + 1] = {1: 1, 2: 31}
            m.read_plan[controls + 2] = {1: 1, 2: 2}
            m.read_plan[controls + 3] = {1: 3, 2: 14}
            m.read_plan[controls + 6] = {1: 2530}

        m.write(TEST + 0x4000, struct.pack("<32H", *[0x10 + 7 * i for i in range(32)]))
        for offset in (0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C):
            m.word(0x4D28CF48 + offset, TEST + 0x4000)
        destination = TEST + 0x1000 if read_destination is None else read_destination
        copied = TEST + 0x1800 if copied_address is None else copied_address
        args = {"configure": [request], "read": [destination, copied, capacity],
                "corr0": [], "corr1": [], "setup": [band, high]}[operation]
        try:
            result = m.call(original if reference else rebuilt, args)
            observations.append({"return": result if operation in ("configure", "read") else None,
                                 "transactions": m.transactions,
                                 "state": m.read(0x4D280000, 0xD178).hex(),
                                 "test_memory": m.read(TEST, 0x6000).hex(),
                                 "stats": m.read(stats, 0x34).hex(),
                                 "control_memory": m.read(controls, 8).hex()})
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc)})
    return {"name": "loopback " + operation, "relocated": relocated,
            "control": control, "tone": tone, "band": band, "high": high,
            "stats_offset": stats_offset, "control_offset": control_offset,
            "stats_address": stats_address, "control_address": control_address,
            "sample_count": sample_count, "sum_words": sum_words, "active": active,
            "result_words": result_words, "response_changes": response_changes,
            "request_address": request_address, "read_destination": read_destination,
            "copied_address": copied_address, "capacity": capacity,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1] and "error" not in observations[0]}


def nvv_cases():
    # Full long-form selector domain plus the two exceptional FRS selectors.
    for tag in list(range(0x115B, 0x11A0)) + [0x11D8, 0x11D9, 0x080E, 0x0857, 0x1006]:
        payload = bytes((i * 37 + 11) & 255 for i in range(64))
        record = struct.pack("<HH", 0xC000 | tag, len(payload)) + payload
        yield f"long tag {tag:#x}", struct.pack("<HHI", len(record), 0x4E56, 0) + record
    for tag in (0x080B, 0x0821, 0x0836):
        if tag == 0x080B:
            payload = struct.pack("<4I", 0x4D140020, 0x12345678, 0x4D154024, 0x89ABCDEF)
        else:
            payload = struct.pack("<6I", 0x4D140020, 0x00FF00FF, 0x12345678,
                                   0x4D154024, 0xFF00FF00, 0x89ABCDEF)
        for length in (0, 1, len(payload) - 1, len(payload)):
            record = struct.pack("<HH", 0xC000 | tag, length) + payload
            # Keep rounded register records readable beyond declared length.
            yield f"register tag {tag:#x} length {length}", struct.pack("<HHI", 4 + length, 0x4E56, 0) + record
    for length in range(8):
        record = struct.pack("<H", 0x8000 | (length << 11) | 0x123) + bytes([0x11] * length)
        yield f"short record length {length}", struct.pack("<HHI", len(record), 0x4E56, 0) + record
    yield "one-byte records", struct.pack("<HHI", 4, 0x4E56, 0) + bytes([1, 2, 3, 4])
    yield "empty", b""
    yield "empty container", struct.pack("<HHI", 0, 0x4E56, 0)
    yield "bad magic", bytes(8)
    yield "two containers", struct.pack("<HHI", 2, 0x4E56, 0) + bytes([1, 2]) + struct.pack("<HHI", 0, 0x4E56, 0)


def check_nvv(elf: Path, name, blob, seed):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        m.write(0x4D140000, struct.pack("<I", seed) * (0x20000 // 4))
        m.write(TEST, bytes(0x2000))
        m.write(TEST, blob)
        try:
            result = m.call(0x50B0 if reference else "nvv_parse_blob", [TEST, len(blob)])
            observations.append({"return": result, "transactions": m.transactions,
                                 "state": m.read(0x4D280000, 0xD178).hex(),
                                 "test_memory": m.read(TEST, 0x2000).hex()})
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc)})
    return {"name": "NVV " + name, "blob_hex": blob.hex(), "seed": seed,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1] and "error" not in observations[0]}


def power_calibration(profile):
    counts, entries = [], []
    for band in range(8):
        channels = {1: [], 2: [0], 3: [0, 63, 127], 4: [3, 17, 80],
                    5: [0, 0, 127], 6: [127, 63, 0]}[profile]
        if profile == 4:
            channels = channels[:band % 4]
        counts.append(len(channels))
        for index, channel in enumerate(channels):
            values = [(30 + band * 7 + stage * 31 + index * (7 if band & 1 else -5)) & 255
                      for stage in range(3)]
            entries.append((channel << 24) | values[0] | (values[1] << 8) | (values[2] << 16))
    return bytes(counts) + struct.pack("<" + "I" * len(entries), *entries)


def check_power_init(elf: Path, profile, workspace=None, calibration_offset=0):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        m.write(0x4D28B700, bytes([0xA5]) * 0x1424)
        if profile:
            m.write(TEST + 0x1000 + calibration_offset, power_calibration(profile))
            m.word(0x4D28CF48 + 0x94, TEST + 0x1000 + calibration_offset)
        if workspace is not None:
            # The firmware's NVV register-write path can change this cell.
            record = struct.pack("<HHII", 0xC80B, 8, 0x4D2895A8, workspace)
            blob = struct.pack("<HHI", len(record), 0x4E56, 0) + record
            m.write(TEST + 0x100, blob)
            m.call(0x50B0 if reference else "nvv_parse_blob", [TEST + 0x100, len(blob)])
        try:
            result = m.call(0x3A68 if reference else "txpwr_init", limit=500000)
            observations.append({"return": result, "transactions": m.transactions,
                                 "state": m.read(0x4D289298, 0xD178 - 0x9298).hex(),
                                 "shared_memory": m.read(0x4D284000, 4).hex(),
                                 "test_memory": m.read(TEST, 0x5000).hex()})
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc)})
    return {"name": "TX power calibration expansion", "profile": profile, "workspace": workspace,
            "calibration_offset": calibration_offset,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1] and "error" not in observations[0]}


def mac_cases():
    for command in (0, 3):
        yield command, 0, b"", 0, 0
    for index in range(28):
        for enabled in (0, 1, 2, 127, 255):
            for value in (0, 0x817F):
                yield 1, 0x15, struct.pack("<BBH", index, enabled, value), 4, 8
    for sub in (0x0B, 0x0E, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x21, 0xFFFF):
        for length in (0, 1, 3):
            yield 1, sub, bytes([2, 1, 127, 128, 255, 0, 0]), length, 3
    for sub in (0x12, 0x14, 0x21):
        for value in (0, 1, 2, 255):
            yield 1, sub, bytes([value]), 1, 5


def check_mac(elf: Path, command, sub, payload, length, outer, seed, mailbox_base=0x4D280000, observe=True, observer_kind="instruction"):
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        if not observe or observer_kind == "instruction":
            for handle in m.observer_handles:
                m.cpu.hook_del(handle)
        if observe and observer_kind == "instruction":
            from instruction_observer import InstructionObserver
            observer = InstructionObserver(m)
            m.cpu.hook_add(UC_HOOK_CODE, observer)
        for address, size in ((0x4D000000, 0x1000), (0x4D140000, 0x20000)):
            m.write(address, struct.pack("<I", seed) * (size // 4))
        m.word(0x4D154C68, 1 << 15)
        # ACK consumption is a supplied peripheral response, not a silicon claim.
        def acknowledge(cpu, access, address, size, value, user_data):
            if address == 0x4D154C6C:
                pending = int.from_bytes(cpu.mem_read(0x4D154C68, 4), "little")
                cpu.mem_write(0x4D154C68, struct.pack("<I", pending & ~(value & 0xFFFF)))
        if observe and observer_kind == "instruction":
            m.write_response = lambda cpu, address, size, value: acknowledge(cpu, 0, address, size, value, None)
        else:
            m.cpu.hook_add(UC_HOOK_MEM_WRITE, acknowledge,
                           begin=0x4D154C6C, end=0x4D154C6F)
        request = bytearray(0xC4)
        struct.pack_into("<H", request, 2, command)
        request[5] = outer
        struct.pack_into("<HH", request, 8, sub, length)
        request[12:12 + len(payload)] = payload
        if mailbox_base != 0x4D280000:
            m.write(mailbox_base, struct.pack("<I", seed) * (0x800 // 4))
            record = struct.pack("<HHII", 0xC80B, 8, 0x4D28929C, mailbox_base)
            blob = struct.pack("<HHI", len(record), 0x4E56, 0) + record
            m.write(TEST + 0x800, blob)
            m.call(0x50B0 if reference else "nvv_parse_blob", [TEST + 0x800, len(blob)])
            if m.read(0x4D28929C, 4) != struct.pack("<I", mailbox_base):
                raise RuntimeError("MAC request NVV fixture did not install the requested cell")
        m.write(mailbox_base, request)
        try:
            m.call(0x524 if reference else "mac_isr")
            observations.append({"transactions": m.transactions,
                                 "mailbox": m.read(mailbox_base, 0x800).hex(),
                                 "state": m.read(0x4D280000, 0xD178).hex()})
        except (UcError, RuntimeError) as exc:
            observations.append({"error": str(exc), "pc": m.cpu.reg_read(UC_ARM_REG_PC)})
    return {"name": "MAC command", "command": command, "sub": sub,
            "payload": payload.hex(), "length": length, "outer": outer, "seed": seed, "mailbox_base": mailbox_base, "observer_enabled": observe, "observer_kind": observer_kind if observe else "none",
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1] and "error" not in observations[0]}


def leaf_cases():
    """Defined scalar ABI inputs; no arbitrary pointers or running counters."""
    for name in ("clk_enable", "clk_enter_panic_mode", "aux_timer_isr", "systick_isr",
                 "coex_enable_rx_gain_backoff", "coex_disable_rx_gain_backoff"):
        yield name, [], False
    for name in ("systick_now", "coex_is_active", "radio_revision_get", "warm_boot_detected",
                 "trace_ring_base", "trace_ring_size"):
        yield name, [], True
    for value in range(256):
        yield "clk_set_prescaler", [value], False
    for value in (0, 1, 4, 8, 12, 15, 16, 31, 32, 64, 255, 256, 0xFFFFFFFF):
        yield "rtk_set_irq_priority_level", [value], True
        yield "rtk_configure_irq_priority", [value], True
        yield "warm_boot_set_state", [value], False
        for name in ("nvic_enable_irq", "nvic_disable_irq", "nvic_clear_pending_irq"):
            yield name, [value], True
    for value in (0, 1, 0xFFFFFF, 0x1000000, 0xFFFFFFFF):
        for interrupt in (0, 1):
            for core in (0, 1):
                yield "systick_init", [value, interrupt, core], False
    for band in range(8):
        for high in (0, 1):
            for tx in (0, 1):
                yield "phy_set_synth_band", [high, tx, band], True
    for enabled in (0, 1, 2, 127, 255):
        yield "modem_set_register_unlock", [enabled], True
        for name in ("tx_set_slope_cal_bypass", "tx_set_hf_coeff_bypass", "rf_test_set_correlator_magic"):
            yield name, [enabled], True
        for value in (0, 1, 0x7FFF, 0xFFFF):
            # int16_t parameters are sign-extended by the AAPCS caller.
            signed = value if value < 0x8000 else value - 0x10000
            for name in ("rx_set_rssi_cal_override", "tx_set_df1_mantissa_override"):
                yield name, [enabled, signed], True
            for name in ("rx_set_mxdc_i_override", "rx_set_mxdc_q_override"):
                yield name, [enabled, value], True
            for param in range(6):
                yield "rx_set_iqmm_override", [enabled, param, signed], True
        for value in (0, 1, 127, 255):
            yield "rx_set_filter_bw_override", [enabled, value], True
            yield "txpwr_set_pa_gain_override", [enabled, value], True
            yield "txpwr_set_global_offset", [enabled, value], True
            for band in range(8):
                yield "txpwr_set_band_override", [band, enabled, value], True
    for address in (0, 0x4D000000, 0x4D140000, 0x42400000, 0x44440000, 0xFFFFFFFF):
        yield "nvv_validate_address", [address], True
    for count in (0, 1, 2, 3, 16, 127):
        yield "xor_checksum_words", [TEST, count], True


def check_leaf(elf: Path, name: str, args, returns: bool, seed: int):
    mappings = {}
    for path in (ROOT / "src").rglob("*.c"):
        pending = None
        for line in path.read_text().splitlines():
            match = re.search(r"@provenance RVA (0x[0-9A-Fa-f]+)", line)
            if match and pending is None:
                pending = int(match[1], 16)
            function = re.match(r"[\w *]+\b(\w+)\([^;]*\)$", line)
            if function and pending is not None:
                mappings[function[1]] = pending
                pending = None
    observations = []
    for reference in (True, False):
        m = Machine(reference, elf)
        for address, length in ((0x4D000000, 0x1000), (0x4D100000, 0x10000),
                                (0x4D140000, 0x20000), (0xE000E000, 0x1000)):
            m.write(address, struct.pack("<I", seed) * (length // 4))
        m.write(0x4D289900, bytes([0x5A]) * (0xD178 - 0x9900))
        m.write(0x4D285000, bytes([0x5A]) * (0x9298 - 0x5000))
        m.write(TEST, bytes(range(256)) * 2)
        try:
            result = m.call(mappings[name] if reference else name, args)
            observations.append({"return": result if returns else None,
                                 "peripheral_accesses": m.transactions,
                                 "state": m.read(0x4D289900, 0xD178 - 0x9900).hex(),
                                 "test_memory": m.read(TEST, 512).hex(),
                                 "shared_memory": m.read(0x4D285000, 0x9298 - 0x5000).hex()})
        except (UcError, RuntimeError, KeyError) as exc:
            observations.append({"error": str(exc)})
    return {"name": name, "args": args, "seed": seed,
            "reference": observations[0], "rebuilt": observations[1],
            "match": observations[0] == observations[1] and "error" not in observations[0]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--suite", choices=("architecture", "packets", "tx-calibration", "tx-volatile", "tx-modes", "loopback-alias", "status-bindings", "trace", "mac-alias", "dpd", "dpd-workflow", "power", "nvv", "loopback", "boot", "boot-diagnostic", "mac", "leaves", "formatter-diagnostic"),
                        default="architecture")
    args = parser.parse_args()
    frozen_elf = elf_bytes(args.elf)
    if args.suite == "formatter-diagnostic":
        # Unicorn 2.1.4 disagrees with QEMU for some IT-return/prefix paths.
        # These results are diagnostic; tools/qemu_formatter.py is independent.
        cases = check_formatters(args.elf)
    elif args.suite in ("boot", "boot-diagnostic"):
        cases = [check_boot(args.elf, False, reload, regions, separate, ccr)
                 for reload in (False, True) for regions in (0, 1, 8, 16)
                 for separate in (0, 1) for ccr in (0, 0xFFFFFFFF)]
        cases.extend(check_boot(args.elf, True, False, 0, 0, ccr)
                     for ccr in (0, 0xFFFFFFFF))
    elif args.suite == "dpd-workflow":
        cases = [check_tx(args.elf, 0, 0, dpd_profile=profile,
                          dpd_armed=armed, dpd_tables=tables)
                 for profile in range(8) for armed in (0, 1, 2) for tables in (False, True)]
        cases.extend(check_tx(args.elf, 0, 0, dpd_profile=profile, dpd_armed=0,
                              dpd_tables=tables, dpd_integrated=True)
                     for profile in range(8) for tables in (False, True))
        cases.extend(check_tx(args.elf, 0, 0, dpd_profile=profile,
                              dpd_gtb_plan=[0, 0, 1, 1, 2, 2, 3, 3, 10])
                     for profile in range(1, 8))
        cases.append(check_tx(args.elf, 0, 0, dpd_profile=7, dpd_integrated=True,
                              dpd_gtb_plan=[0, 0, 1, 1, 2, 2, 3, 3, 10]))
    elif args.suite == "dpd":
        cases = [check_dpd(args.elf, "command", opcode, command_address=command,
                           out_address=out, length_address=length, capacity=capacity)
                 for opcode in (0, 1, 2, 3, 0xFFFFFFFF)
                 for command in (TEST + 0x100, 0x4D160000, 0x4D28AA84, 0x4D28AA88, 0x4D28AAA4, 0x4D28B664)
                 for out in (TEST + 0x400, 0x4D160100)
                 for length in (TEST + 0x600, 0x4D160200)
                 for capacity in (0, 7, 8, 0xFFFFFFFF)]
        cases.extend(check_dpd(args.elf, "command", 3, out_address=out, length_address=length)
                     for out, length in ((0, TEST + 0x600), (TEST + 0x400, 0),
                                         (0x4D28AAAC, TEST + 0x600), (TEST + 0x400, 0x4D28AAA8)))
        cases.extend(check_dpd(args.elf, "irq", capture_gtb=gtb, mode=mode)
                     for gtb in (0, 1, 0xFFFFFFFF) for mode in (0, 1, 7, 15))
    elif args.suite == "mac-alias":
        cases = [check_mac_crash(args.elf, base, delta)
                 for base in (0x4D280000, TEST + 0x1000, 0x4D160000)
                 for delta in (None, -20, -16, -4, -1, 0, 1, 4, 16, 20)]
        cases.extend(check_mac_crash(args.elf, base, source_address=source)
                     for base in (TEST + 0x1000, 0x4D160000, 0x4D28929C - 0x5C1)
                     for source in (0x4D161000, 0x4D161001, 0x4D161002, 0x4D161003))
        cases.extend(check_mac(args.elf, command, 0, b"", 0, 0, seed, mailbox_base=base)
                     for command in (0, 3) for base in (TEST + 0x1000, 0x4D160000)
                     for seed in (0, 0xA5A55A5A))
        cases.extend(check_mac(args.elf, 1, 0x15, struct.pack("<BBH", index, 2, 0x817F),
                               4, 8, seed, mailbox_base=base)
                     for index in range(28) for base in (TEST + 0x1000, 0x4D160000)
                     for seed in (0, 0xA5A55A5A))
    elif args.suite == "trace":
        cases = [check_trace(args.elf, ring, cursor, offset, null, freeze, primask)
                 for ring in (TEST + 0x1000, 0x4D160000)
                 for cursor in (0, 1, 17, 18, 0xFFFFFFFF)
                 for offset in (None, 0, 4, 8, 16, 20, 28)
                 for null in (False, True) for freeze in (0, 1, 2, 0xFFFFFFFF)
                 for primask in (0, 1)]
        # Wrapped record addresses remain in the mapped ring window. Payload
        # values keep all subsequent re-indexed writes in that window too.
        cases.extend(check_trace(args.elf, ring, cursor, null_event=null, freeze=freeze, primask=mask)
                     for ring in (TEST + 0x1000, 0x4D160000)
                     for cursor in (0x12492492, 0x09249249)
                     for null in (False, True) for freeze in (0, 2) for mask in (0, 1))
        cases.extend(check_trace(args.elf, ring, site=site)
                     for ring in (TEST + 0x1000, 0x4D160000)
                     for site in (0, 0x148, 0xFFFFFFFF, 0xB2D72E88))
        cases.extend(check_trace(args.elf, 0x4D160000, changing_cursor=True,
                                 null_event=null, event_address=0x4D161000)
                     for null in (False, True))
    elif args.suite == "status-bindings":
        cases = [check_readback(args.elf, op, capacity, null, status_base=base)
                 for op, capacity in (("telemetry", 0), ("telemetry", 43), ("telemetry", 44),
                                      ("telemetry", 0xFFFFFFFF), ("version", 0), ("version", 126),
                                      ("version", 127), ("version", 0xFFFFFFFF))
                 for base in (TEST + 0x4000, 0x4D160000) for null in (0, 1, 2)]
        cases.extend(check_readback(args.elf, "telemetry", 44, status_base=TEST + 0x4000,
                                   tx_count=0xFFFFFFFF, rx_count=count, cfo_sum=value)
                     for count in (0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF)
                     for value in (0x80000000, -1, 0, 1, 0x7FFFFFFF))
        cases.extend(check_status_binding(args.elf, op, base, state)
                     for op in ("get", "set") for base in (TEST + 0x4000, 0x4D160000)
                     for state in (0, 1, 2, 255, 0xFFFFFFFF))
        cases.extend(check_status_binding(args.elf, "restore", base, count=count, stop_early=early)
                     for base in (TEST + 0x4000, 0x4D160000)
                     for count in (0, 1, 3) for early in (False, True))
    elif args.suite == "loopback-alias":
        cases = [check_loopback(args.elf, op, True, control=control,
                               stats_address=stats, control_address=controls,
                               sample_count=count, active=active,
                               sum_words=(0x7FFFFFFF, 0x80000000, 0x7FFFFF00, 0x80000100))
                 for op in ("read", "corr0", "corr1", "configure", "setup")
                 for stats in (TEST + 0x3000, 0x4D160000)
                 for controls in (TEST + 0x3100, 0x4D160100)
                 for control in (0, 2, 3, 0x03021F03)
                 for count in (0, 1, 254, 255) for active in (0, 1, 2, 255)]
        cases.extend(check_loopback(args.elf, op, True, sample_count=count,
                                    stats_address=stats, result_words=results,
                                    sum_words=(0x7FFFFFFF, 0x80000000, 0x7FFFFFFF, 0x80000000))
                     for op in ("corr0", "corr1") for stats in (TEST + 0x3000, 0x4D160000)
                     for count in (0, 1, 254, 255)
                     for results in ((0, 0, 0, 0), (0x7FFFFF, 0x800000, 0x7FFFFF, 0x800000),
                                     (1, 0xFFFFFF, 1, 0xFFFFFF), (0xFFFFFFFF,) * 4))
        cases.extend(check_loopback(args.elf, "configure", True, control=control,
                                    tone=(2350 << 16) | 1, control_address=address,
                                    response_changes=address == 0x4D160100)
                     for control in (2, 3, 0xE3, 0x03021F03)
                     for address in (0x4D160100, 0x4D0000BC, 0x4D000308, 0x4D000274))
        cases.extend(check_loopback(args.elf, "configure", True, control=3,
                                    request_address=TEST + 0x3100 + delta)
                     for delta in (-4, 0, 4))
        cases.extend(check_loopback(args.elf, "read", True,
                                    read_destination=TEST + 0x3000 + delta, copied_address=copied)
                     for delta in (-4, 0, 4) for copied in (TEST + 0x1800, TEST + 0x3000, TEST + 0x3004))
        cases.extend(check_loopback(args.elf, "read", True, active=active, capacity=capacity,
                                    read_destination=destination, copied_address=copied)
                     for active in (0, 1, 2, 255) for capacity in (0, 51, 52, 0xFFFFFFFF)
                     for destination, copied in ((0, TEST + 0x1800), (TEST + 0x1000, 0),
                                                  (TEST + 0x1000, TEST + 0x1800)))
    elif args.suite == "loopback":
        cases = [check_loopback(args.elf, "configure", relocated, control, tone)
                 for relocated in (False, True) for control in (0, 1, 2, 3, 4, 0xE3, 0x03021F03)
                 for tone in (0, (2350 << 16) | 1, (2530 << 16) | 1,
                              (2349 << 16) | 1, (2531 << 16) | 1)]
        cases.extend(check_loopback(args.elf, op, relocated)
                     for op in ("read", "corr0", "corr1") for relocated in (False, True))
        cases.extend(check_loopback(args.elf, "setup", relocated, control, band=band, high=high)
                     for relocated in (False, True) for control in (0, 0xE0)
                     for band in range(8) for high in (0, 1))
        cases.extend(check_loopback(args.elf, op, True, stats_offset=offset)
                     for op in ("read", "corr0", "corr1") for offset in (1, 2, 3))
        cases.extend(check_loopback(args.elf, op, True, control=3, control_offset=offset)
                     for op in ("configure", "setup") for offset in (1, 2, 3))
    elif args.suite == "nvv":
        cases = [check_nvv(args.elf, name, blob, seed)
                 for name, blob in nvv_cases() for seed in (0, 0xA5A55A5A)]
    elif args.suite == "power":
        cases = [check_power_init(args.elf, 0)]
        cases.extend(check_power_init(args.elf, profile, calibration_offset=offset)
                     for profile in range(1, 7) for offset in range(4))
        cases.extend(check_power_init(args.elf, 3, workspace=TEST + 0x3000 + offset)
                     for offset in range(4))
    elif args.suite == "mac":
        cases = [check_mac(args.elf, *case, seed)
                 for case in mac_cases() for seed in (0, 0xA5A55A5A)]
    elif args.suite == "leaves":
        cases = [check_leaf(args.elf, name, words, returns, seed)
                 for name, words, returns in leaf_cases()
                 for seed in (0, 0xA5A55A5A, 0xFFFFFFFF)]
    elif args.suite == "tx-modes":
        cases = [check_tx(args.elf, band, packet, alternate, mode,
                          register_seed=0xA5A55A5A, alt_rate=rate,
                          dynamic_pattern=True, override_flag=flag)
                 for band in (0, 1, 3, 7) for packet in (0, 6, 9, 13)
                 for alternate in (False, True) for mode in (0, 1, 2, 3, 249, 255, 256, 257)
                 for rate in (0, 1, 2, 3) for flag in (0, 1, 2, 255)]
    elif args.suite == "tx-volatile":
        cases = [check_tx(args.elf, band, 9, power_mode=mode, hf=True,
                          register_seed=0xA5A55A5A, hf_stages=stages)
                 for band in (1, 3, 7) for mode in (0, 1)
                 for stages in ((3, 2), (1, 2), (2, 3), (0, 3))]
        cases.extend(check_tx(args.elf, band, pkt, calibration=1, analog=4,
                             register_seed=0xA5A55A5A,
                             cmd0_reads=[band << 14, 1 << 8, 2 << 17, 127,
                                         (band << 14) | 23])
                     for band in range(8) for pkt in (0, 6, 8, 9))
    elif args.suite == "tx-calibration":
        cases = [check_tx(args.elf, band, pkt, alternate, mode, register_seed=0xA5A55A5A,
                          analog=profile, channel=channel)
                 for profile in (1, 2, 3, 4) for band in range(8)
                 for pkt in (0, 6, 8, 9, 12) for alternate in (False, True)
                 for mode in (0, 1) for channel in (0, 63, 127)]
        cases.extend(check_tx(args.elf, band, pkt, alternate, mode,
                             register_seed=0xA5A55A5A, hf=True)
                     for band in range(8) for pkt in range(16)
                     for alternate in (False, True) for mode in (0, 1))
    elif args.suite == "packets":
        cases = [check_tx(args.elf, band, pkt, alternate, mode, cal, 0xA5A55A5A)
                 for band in range(8) for pkt in range(16)
                 for alternate in (False, True) for mode in (0, 1) for cal in range(5)]
        cases.extend(check_rx(args.elf, band, pkt, alternate, calibration)
                     for band in range(8) for pkt in range(16)
                     for alternate in (False, True) for calibration in (False, True))
    else:
        cases = [check_reset(args.elf), check_memory_observer(args.elf)]
        cases.extend(check_exception_entry(args.elf, lr, svc)
                     for lr in (0xFFFFFFF9, 0xFFFFFFFD, 0xFFFFFFE9, 0xFFFFFFED)
                     for svc in (False, True))
        reference = (ROOT.parent / "phyb").read_bytes()
        with io.BytesIO(frozen_elf) as stream:
            image = ELFFile(stream)
            for name, address, data in (
                (".rtkit.header", 0xC0, reference[0xC0:0x108]),
                (".image.prefix", 0x4D280000, reference[0x88E0:0x11B78]),
                (".data", 0x4D289298, reference[0x11B78:0x12120]),
                (".rtkit.meta", 0x4D289844, reference[0x12124:0x1219D]),
                (".rtkit.guard", 0x4D289840, reference[0x12120:0x12124]),
            ):
                section = image.get_section_by_name(name)
                cases.append({"name": name, "match": section is not None
                              and section["sh_addr"] == address and section.data() == data})
            symbols = {sym.name: sym for sym in image.get_section_by_name(".symtab").iter_symbols()}
            for name, start, end in (("memmove", 0x5C70, 0x5D7E), ("memset", 0x5D80, 0x5DF8),
                                         ("roundf_half_up", 0x4B54, 0x4B76),
                                         ("strlen", 0x5E00, 0x5E74),
                                         ("strncmp", 0x5E88, 0x5F54),
                                         ("strnlen", 0x6AEA, 0x6AFE),
                                         ("strrchr", 0x6AFE, 0x6B22)):
                sym = symbols[name]
                section = image.get_section(sym["st_shndx"])
                offset = (sym["st_value"] & ~1) - section["sh_addr"]
                data = section.data()[offset:offset + sym["st_size"]]
                cases.append({"name": name + " position-independent instruction bytes",
                              "match": data == reference[start:end], "byte_count": len(data)})
            # Compare relocated calls independently of their encoded displacement.
            def branch_target(data, address):
                first = int.from_bytes(data[:2], "little")
                second = int.from_bytes(data[2:], "little")
                sign = (first >> 10) & 1
                i1 = 1 ^ ((second >> 13) & 1) ^ sign
                i2 = 1 ^ ((second >> 11) & 1) ^ sign
                displacement = ((sign << 24) | (i1 << 23) | (i2 << 22)
                                | ((first & 0x3FF) << 12) | ((second & 0x7FF) << 1))
                if sign:
                    displacement -= 1 << 25
                return (address + 4 + displacement) & 0xFFFFFFFF

            for name, start, end, branches in (
                ("__udivdi3", 0x5F98, 0x605A,
                 ((0x44, "__aeabi_llsr", 0x5F76, True),
                  (0x56, "__aeabi_llsl", 0x5F54, True))),
                ("strlcat", 0x6A60, 0x6AB8,
                 ((0x10, "strnlen", 0x6AEA, True), (0x1E, "strlen", 0x5E00, True),
                  (0x4C, "strlen", 0x5E00, True))),
                ("strlcpy", 0x6AB8, 0x6AEA,
                 ((0x1E, "strlen", 0x5E00, False), (0x2A, "strlen", 0x5E00, True))),
            ):
                sym = symbols[name]
                section = image.get_section(sym["st_shndx"])
                address = sym["st_value"] & ~1
                offset = address - section["sh_addr"]
                actual = bytearray(section.data()[offset:offset + sym["st_size"]])
                expected = bytearray(reference[start:end])
                calls_match = True
                for at, target, original_target, link in branches:
                    a, b = bytes(actual[at:at + 4]), bytes(expected[at:at + 4])
                    # Verify opcode fields as well as destination; do not mask a changed instruction.
                    wanted = 0xD000 if link else 0x9000
                    calls_match &= all((int.from_bytes(code[:2], "little") & 0xF800) == 0xF000
                                       and (int.from_bytes(code[2:], "little") & 0xD000) == wanted
                                       for code in (a, b))
                    calls_match &= branch_target(a, address + at) == (symbols[target]["st_value"] & ~1)
                    calls_match &= branch_target(b, start + at) == original_target
                    actual[at:at + 4] = expected[at:at + 4] = bytes(4)
                cases.append({"name": name + " instruction bytes and decoded call relocations",
                              "match": calls_match and actual == expected, "byte_count": len(actual)})
            # strncmp uses the literal immediately before its entry.
            sym = symbols["strncmp"]
            section = image.get_section(sym["st_shndx"])
            offset = (sym["st_value"] & ~1) - section["sh_addr"]
            cases.append({"name": "strncmp preceding literal pool", "match":
                          section.data()[offset - 4:offset] == reference[0x5E84:0x5E88]})
    # Exact bytes were compared above. Store digests for matched snapshots;
    # retain full byte snapshots for divergences to support diagnosis.
    for case in cases:
        if case["match"]:
            for side in ("reference", "rebuilt"):
                for key, value in list(case.get(side, {}).items()):
                    if key in ("state", "test_memory", "shared_memory") and isinstance(value, str):
                        data = bytes.fromhex(value)
                        case[side][key] = {"sha256": hashlib.sha256(data).hexdigest(),
                                           "byte_count": len(data)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    report = {"evidence": "finite instruction-level differential cases; not exhaustive proof",
              "reference_sha256": REFERENCE_SHA256,
              "elf_sha256": hashlib.sha256(frozen_elf).hexdigest(),
              "source_elf_unchanged": frozen_elf == args.elf.read_bytes(),
              "suite": args.suite, "cases": cases}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    failed = [c for c in cases if not c["match"]]
    print(f"{args.suite}: {len(cases)} cases, {len(failed)} divergences")
    for c in failed[:15]:
        print({k: v for k, v in c.items() if k not in ("reference", "rebuilt")})
    return bool(failed) or not report["source_elf_unchanged"]


if __name__ == "__main__":
    raise SystemExit(main())
