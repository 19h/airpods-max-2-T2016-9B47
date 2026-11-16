#!/usr/bin/env python3
"""Unicorn ARMv7E-M + Capstone differential execution harness for acib.

Executes reconstructed C functions from acib.elf side-by-side against their
exact RVA counterparts in raw ftab acib under identical initial CPU register,
SRAM (0x10000000), MAC<->PHY shared mailbox (0x4D280000), and scripted MMIO
states, verifying zero divergences across return values, special registers,
memory mutations, and ordered peripheral MMIO transactions.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ProcessPoolExecutor
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import sys
from typing import Any

import capstone as cs
from elftools.elf.elffile import ELFFile
from unicorn import (
    UC_ARCH_ARM,
    UC_HOOK_CODE,
    UC_HOOK_MEM_READ,
    UC_HOOK_MEM_WRITE,
    UC_MEM_WRITE,
    UC_MODE_MCLASS,
    UC_MODE_THUMB,
    Uc,
    UcError,
)
from unicorn.arm_const import (
    UC_ARM_REG_BASEPRI,
    UC_ARM_REG_IPSR,
    UC_ARM_REG_LR,
    UC_ARM_REG_PC,
    UC_ARM_REG_PRIMASK,
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_R3,
    UC_ARM_REG_R4,
    UC_ARM_REG_R5,
    UC_ARM_REG_R6,
    UC_ARM_REG_R7,
    UC_ARM_REG_R8,
    UC_ARM_REG_R9,
    UC_ARM_REG_R10,
    UC_ARM_REG_R11,
    UC_ARM_REG_R12,
    UC_ARM_REG_SP,
    UC_CPU_ARM_CORTEX_M4,
)

EXPECTED_RAW_SHA256 = "dda0b224409bb5997dd95374312f13a207591e857edf9f2cff234ae86b66795e"

STACK_TOP = 0x1001BD00
STOP_PC   = 0x000FF000
SCRATCH   = 0x1002E000  # Scratch test buffer above 0x1002D2C4 inside mapped SRAM

AUTO_DIFF_REGIONS: tuple[tuple[int, int], ...] = (
    (0x10000000, 0x0001AF80),  # RAM_DATA + RAM_BSS below stacks
    (0x1001BD80, 0x00011544),  # RAM_BSS above MSP stack .. 0x1002D2C4
    (0x1002E000, 0x00008000),  # 32 KB structured test scratch arena
    (0x20000000, 0x00004000),  # SoC shared SRAM slice
    (0x4D280000, 0x00001000),  # Shared MAC<->PHY Mailbox SRAM
)

ARG_REGS = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)

CALLEE_SAVED_SENTINELS: tuple[tuple[int, int], ...] = (
    (UC_ARM_REG_R4, 0xDEAD0400),
    (UC_ARM_REG_R5, 0xDEAD0500),
    (UC_ARM_REG_R6, 0xDEAD0600),
    (UC_ARM_REG_R7, 0xDEAD0700),
    (UC_ARM_REG_R8, 0xDEAD0800),
    (UC_ARM_REG_R9, 0xDEAD0900),
    (UC_ARM_REG_R10, 0xDEAD0A00),
    (UC_ARM_REG_R11, 0xDEAD0B00),
)

MMIO_RANGES = (
    (0x42030000, 0x4209FFFF),  # Coex / IPC MMIO (0x42034000, 0x42080000, 0x42090000)
    (0x42440000, 0x4244FFFF),  # ACI / RTKPMGR
    (0x4D154000, 0x4D154FFF),  # W3 PHY Sequencer & Doorbells
    (0x520D0000, 0x522DFFFF),  # Chip Cfg, BT Modem, 1 MHz Timer, EvTrig, LC MAC 0x52200000
    (0x52600000, 0x526BFFFF),  # RF / Radio Sequencer MMIO (0x52610000..0x526A0000)
    (0x529C0000, 0x529CFFFF),  # ULLA ADMA MMIO
    (0x52F00000, 0x52F0FFFF),  # Platform MMIO
    (0xE0001000, 0xE000FFFF),  # ARMv7-M DWT, NVIC, SCB, MPU
)

# (raw_rva, symbol_name, abi_kind_or_arity)
PENDING_STUB_SPECS: tuple[tuple[int, str, int | str], ...] = (
    (0x6BDDC, "sub_6BDDC", "panic_log"),
    (0xEAC84, "sub_EAC84", "panic_rec"),
    (0xEA86E, "sub_EA86E", 1),
    (0x7ECD4, "sub_7ECD4", 2),
    (0x7ECC6, "sub_7ECC6", 3),
    (0x7E9F8, "sub_7E9F8", 1),
    (0xDF764, "sub_DF764", 0),
    (0x7EE58, "sub_7EE58", 0),
    (0x74240, "sub_74240", 1),
    (0x74256, "sub_74256", 2),
    (0x74270, "sub_74270", 3),
    (0x74290, "sub_74290", 4),
    (0x742B2, "sub_742B2", 5),
    (0x7DC92, "sub_7DC92", 1),
    (0x6DF3C, "sub_6DF3C", 1),
    (0x6DC88, "sub_6DC88", 0),
    (0x6DD04, "sub_6DD04", 0),
    (0x6DD70, "sub_6DD70", 1),
    (0x6DDB8, "sub_6DDB8", 0),
    (0x6DE10, "sub_6DE10", 0),
    (0x740A8, "sub_740A8", 2),
    (0x740FE, "sub_740FE", 3),
    (0x35354, "sub_35354", "crash_fmt"),
    (0xEBA60, "sub_EBA60", "crash_tag"),
    (0x7513C, "sub_7513C", "str0_u32"),
    (0x7835C, "sub_7835C", 1),
    (0x7D820, "sub_7D820", 1),
    (0xCF6B4, "sub_CF6B4", 9),
    (0x96458, "sub_96458", 2),
    (0x964AC, "sub_964AC", 1),
    (0x96434, "sub_96434", 2),
    (0x707F0, "sub_707F0", 0),
    (0xDECFC, "sub_DECFC", 0),
    (0x02F78, "sub_2F78",  0),
    (0x71540, "sub_71540", 1),
    (0xE696A, "sub_E696A", "str2"),
    (0x71240, "sub_71240", 0),
    (0xEBA64, "sub_EBA64", 1),
    (0x7076C, "sub_7076C", 0),
    (0x77A44, "sub_77A44", 0),
    (0x6C850, "sub_6C850", 1),
    (0x73F20, "sub_73F20", 1),
    (0x7B1C4, "sub_7B1C4", 1),
    (0x74F10, "sub_74F10", 1),
    (0x6AFB0, "sub_6AFB0", 0),
    (0x776E0, "sub_776E0", 4),
    (0x69978, "sub_69978", 1),
    (0x598E8, "sub_598E8", 1),
    (0x133BC, "sub_133BC", 1),
    (0x188B4, "sub_188B4", 1),
    (0x4B5C2, "sub_4B5C2", 1),
    (0x02774, "sub_2774",  0),
    (0x02584, "sub_2584",  0),
    (0x4B2B0, "sub_4B2B0", 1),
    (0xE713C, "sub_E713C", "thread_create"),
    (0xE7C20, "sub_E7C20", 1),
    (0x02F8C, "sub_2F8C",  0),
    (0x59884, "sub_59884", 0),
    (0x05688, "sub_5688",  0),
)


def _count_printf_specs(fmt: str) -> int:
    idx = 0
    specs = 0
    n = len(fmt)
    while idx < n:
        if fmt[idx] == "%":
            if idx + 1 < n and fmt[idx + 1] == "%":
                idx += 2
                continue
            specs += 1
        idx += 1
    return specs


class ArmDiffMachine:
    def __init__(
        self,
        *,
        is_raw: bool,
        raw_bytes: bytes,
        elf_bytes: bytes,
        extra_stubs: list[dict[str, Any]] | None = None,
    ) -> None:
        self.is_raw = is_raw
        self.extra_stubs = list(extra_stubs or [])
        self.cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.cpu.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M4)

        for base, size in (
            (0x00000000, 0x00100000),  # 1 MB ROM + STOP_PC page
            (0x10000000, 0x00040000),  # 256 KB SRAM + test scratch arena
            (0x20000000, 0x00010000),  # SoC shared SRAM slice
            (0x42030000, 0x00070000),  # Coex / IPC MMIO
            (0x42440000, 0x00010000),  # ACI / RTKPMGR MMIO
            (0x4D154000, 0x00001000),  # W3 PHY Sequencer MMIO
            (0x4D280000, 0x00001000),  # Shared MAC<->PHY Mailbox SRAM
            (0x520D0000, 0x00210000),  # 0x520D0000 .. 0x522DFFFF MMIO
            (0x52600000, 0x000C0000),  # 0x52600000 .. 0x526BFFFF RF MMIO
            (0x529C0000, 0x00010000),  # ULLA ADMA MMIO
            (0x52F00000, 0x00010000),  # Platform MMIO
            (0xE0000000, 0x00010000),  # ARMv7-M DWT & SCS MMIO
        ):
            self.cpu.mem_map(base, size)

        self.symbols: dict[str, int] = {}
        self.text_bounds: tuple[int, int] = (0, 0)
        self.base_ram_data = raw_bytes[0xF9860 : 0xF9860 + 0xAC1E]
        self._zero_bss_and_scratch = b"\x00" * (0x40000 - 0xAC1E)
        self._zero_soc_shm = b"\x00" * 0x4000
        self._zero_mbox = b"\x00" * 0x1000

        if is_raw:
            self.cpu.mem_write(0x00000000, raw_bytes[:0xF984C])
            self.cpu.mem_write(0x10000000, self.base_ram_data)
        else:
            with io.BytesIO(elf_bytes) as fp:
                elf = ELFFile(fp)
                for seg in elf.iter_segments():
                    if seg["p_type"] == "PT_LOAD" and seg["p_filesz"] > 0:
                        self.cpu.mem_write(seg["p_vaddr"], seg.data())
                text_sec = elf.get_section_by_name(".text")
                if text_sec is not None:
                    t_start = int(text_sec["sh_addr"])
                    self.text_bounds = (t_start, t_start + int(text_sec["sh_size"]))
                symtab = elf.get_section_by_name(".symtab")
                assert symtab is not None
                for sym in symtab.iter_symbols():
                    if sym.name:
                        self.symbols[sym.name] = sym["st_value"]

        self.transactions: list[tuple[str, str, int, str]] = []
        self.stub_calls: list[tuple[Any, ...]] = []
        self.track_stub_calls: bool = False
        self.panic_null_varargs: int = 3
        self.read_plan: dict[int, dict[int, int]] = {}
        self.read_counts: dict[int, int] = {}
        self.touched_mmio: set[int] = set()
        self.patched_rom: list[tuple[int, bytes]] = []
        self.w1c_4d154c64: bool = False
        self.pending_w1c_4d154c64: int = 0

        # Attach memory hooks ONLY to peripheral MMIO ranges (never SRAM/ROM/mailbox).
        for begin, end in MMIO_RANGES:
            self.cpu.hook_add(
                UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE,
                self._on_mmio,
                begin=begin,
                end=end,
            )

        # Attach exact entry-address hooks to pending_c link stubs for callee/arg verification.
        self._stub_by_addr: dict[int, tuple[str, int | str]] = {}
        all_stub_specs: list[tuple[int, str, int | str]] = list(PENDING_STUB_SPECS)
        for es in self.extra_stubs:
            es_rva = int(es["rva"], 16) if isinstance(es["rva"], str) else int(es["rva"])
            all_stub_specs.append((es_rva, str(es["symbol"]), es.get("arity", 0)))

        for raw_rva, sym_name, abi_spec in all_stub_specs:
            if is_raw:
                hook_addr = raw_rva & ~1
            else:
                if sym_name not in self.symbols:
                    continue
                hook_addr = self.symbols[sym_name] & ~1
            if hook_addr in self._stub_by_addr:
                continue
            self._stub_by_addr[hook_addr] = (sym_name, abi_spec)
            self.cpu.hook_add(
                UC_HOOK_CODE,
                self._on_stub_entry,
                begin=hook_addr,
                end=hook_addr,
            )

    def _read_cstr(self, uc: Uc, ptr: int) -> str | None:
        if ptr == 0:
            return None
        chunk = bytes(uc.mem_read(ptr, 160))
        nul = chunk.find(b"\x00")
        if nul >= 0:
            chunk = chunk[:nul]
        return chunk.decode("latin1")

    def _read_abi_args(self, uc: Uc, count: int) -> list[int]:
        vals: list[int] = []
        sp = uc.reg_read(UC_ARM_REG_SP)
        for idx in range(count):
            if idx < 4:
                vals.append(uc.reg_read(ARG_REGS[idx]))
            else:
                vals.append(int.from_bytes(uc.mem_read(sp + 4 * (idx - 4), 4), "little"))
        return vals

    def _on_stub_entry(self, uc: Uc, address: int, _size: int, _ud: Any) -> None:
        if not self.track_stub_calls:
            return
        info = self._stub_by_addr.get(address & ~1)
        if info is None:
            return
        sym_name, abi_spec = info
        if isinstance(abi_spec, int):
            self.stub_calls.append((sym_name, *self._read_abi_args(uc, abi_spec)))
        elif abi_spec == "panic_log":
            fmt_ptr = uc.reg_read(UC_ARM_REG_R0)
            args_ptr = uc.reg_read(UC_ARM_REG_R1)
            fmt_s = self._read_cstr(uc, fmt_ptr)
            n_va = self.panic_null_varargs if fmt_s is None else _count_printf_specs(fmt_s)
            va = list(struct.unpack(f"<{n_va}I", bytes(uc.mem_read(args_ptr, 4 * n_va)))) if n_va else []
            self.stub_calls.append((sym_name, fmt_s, va))
        elif abi_spec == "panic_rec":
            code = uc.reg_read(UC_ARM_REG_R0)
            sub = uc.reg_read(UC_ARM_REG_R1)
            fmt_ptr = uc.reg_read(UC_ARM_REG_R2)
            args_ptr = uc.reg_read(UC_ARM_REG_R3)
            fmt_s = self._read_cstr(uc, fmt_ptr)
            n_va = self.panic_null_varargs if fmt_s is None else _count_printf_specs(fmt_s)
            va = list(struct.unpack(f"<{n_va}I", bytes(uc.mem_read(args_ptr, 4 * n_va)))) if n_va else []
            self.stub_calls.append((sym_name, code, sub, fmt_s, va))
        elif abi_spec == "str2":
            s0 = self._read_cstr(uc, uc.reg_read(UC_ARM_REG_R0))
            s1 = self._read_cstr(uc, uc.reg_read(UC_ARM_REG_R1))
            self.stub_calls.append((sym_name, s0, s1))
        elif abi_spec == "str0_u32":
            s0 = self._read_cstr(uc, uc.reg_read(UC_ARM_REG_R0))
            r1 = uc.reg_read(UC_ARM_REG_R1)
            self.stub_calls.append((sym_name, s0, r1))
        elif abi_spec == "thread_create":
            args6 = self._read_abi_args(uc, 6)
            self.stub_calls.append((sym_name, self._read_cstr(uc, args6[0]), *args6[1:]))
        elif abi_spec == "crash_fmt":
            args6 = self._read_abi_args(uc, 6)
            in_sram = 0x10000000 <= args6[0] < 0x10030000
            self.stub_calls.append((sym_name, in_sram, *args6[1:]))
        elif abi_spec == "crash_tag":
            args3 = self._read_abi_args(uc, 3)
            in_sram = 0x10000000 <= args3[2] < 0x10030000
            self.stub_calls.append((sym_name, args3[0], args3[1], in_sram))

    def _on_mmio(self, uc: Uc, access: int, address: int, size: int, value: int, _ud: Any) -> None:
        aligned = address & ~3
        self.touched_mmio.add(aligned)
        if access == UC_MEM_WRITE:
            mask = (1 << (8 * size)) - 1
            val_masked = value & mask
            self.transactions.append(("W", hex(address), size, hex(val_masked)))
            if self.w1c_4d154c64 and address == 0x4D154C64 and size == 4:
                self.pending_w1c_4d154c64 |= (val_masked & 0xFFFF)
        else:
            if self.w1c_4d154c64 and address == 0x4D154C64 and self.pending_w1c_4d154c64 != 0:
                cur = int.from_bytes(uc.mem_read(0x4D154C64, 4), "little")
                cleared = (cur & ~self.pending_w1c_4d154c64) & 0xFFFFFFFF
                uc.mem_write(0x4D154C64, cleared.to_bytes(4, "little"))
                self.pending_w1c_4d154c64 = 0
            ord_idx = self.read_counts.get(address, 0) + 1
            self.read_counts[address] = ord_idx
            planned = self.read_plan.get(address, {}).get(ord_idx)
            if planned is not None:
                uc.mem_write(address, int(planned).to_bytes(size, "little"))
                self.touched_mmio.add(aligned)
            actual = int.from_bytes(uc.mem_read(address, size), "little")
            self.transactions.append(("R", hex(address), size, hex(actual)))

    def patch_code(self, addr: int, patch: bytes) -> None:
        orig = bytes(self.cpu.mem_read(addr, len(patch)))
        self.patched_rom.append((addr, orig))
        self.cpu.mem_write(addr, patch)
        self.cpu.ctl_remove_cache(0, 0x00100000)

    def install_pending_link_stubs(self) -> None:
        if not self.is_raw:
            return
        ret0 = bytes.fromhex("00207047")
        ret1 = bytes.fromhex("01207047")
        ret2 = bytes.fromhex("02207047")
        ret_r0 = bytes.fromhex("7047")
        ret_r2 = bytes.fromhex("10467047")
        ea86e = bytes.fromhex("40f20812904207d34ff6a862c0f20e02904201d20120704700207047")

        for rva in (
            0x6BDDC, 0xEAC84, 0x7ECD4, 0xDF764, 0x7EE58, 0x74240, 0x74256,
            0x74270, 0x74290, 0x742B2, 0x7DC92, 0x6DF3C, 0x6DC88, 0x6DD04, 0x6DD70,
            0x6DDB8, 0x6DE10, 0x740A8, 0x740FE,
            0x35354, 0xEBA60, 0x7835C, 0x7D820, 0xCF6B4, 0x96458, 0x964AC, 0x96434,
            0x707F0, 0xDECFC, 0x2F78, 0x71540, 0xE696A, 0x71240, 0xEBA64, 0x7076C,
            0x77A44, 0x73F20, 0x7B1C4, 0x74F10, 0x6AFB0, 0x776E0, 0x69978, 0x4B5C2,
            0x2774, 0x2584, 0x4B2B0, 0x2F8C, 0x59884, 0x5688,
        ):
            self.patch_code(rva, ret0)
        for rva in (0x7513C,):
            self.patch_code(rva, ret1)
        self.patch_code(0x7ECC6, ret2)
        for rva in (0x7E9F8, 0x6C850, 0x598E8, 0x133BC, 0x188B4, 0xE7C20):
            self.patch_code(rva, ret_r0)
        self.patch_code(0xE713C, ret_r2)
        self.patch_code(0xEA86E, ea86e)
        for es in self.extra_stubs:
            es_rva = int(es["rva"], 16) if isinstance(es["rva"], str) else int(es["rva"])
            kind = es.get("ret_kind", "ret0")
            if kind == "ret1":
                self.patch_code(es_rva & ~1, ret1)
            elif kind == "ret2":
                self.patch_code(es_rva & ~1, ret2)
            elif kind == "ret_r0":
                self.patch_code(es_rva & ~1, ret_r0)
            else:
                self.patch_code(es_rva & ~1, ret0)

    def snapshot_auto_regions(self) -> list[bytes]:
        return [bytes(self.cpu.mem_read(addr, length)) for addr, length in AUTO_DIFF_REGIONS]

    def reset_state(self) -> None:
        if self.patched_rom:
            for addr, orig in reversed(self.patched_rom):
                self.cpu.mem_write(addr, orig)
            self.patched_rom.clear()
            self.cpu.ctl_remove_cache(0, 0x00100000)
        self.w1c_4d154c64 = False
        self.pending_w1c_4d154c64 = 0
        self.cpu.mem_write(0x10000000, self.base_ram_data)
        self.cpu.mem_write(0x1000AC1E, self._zero_bss_and_scratch)
        self.cpu.mem_write(0x20000000, self._zero_soc_shm)
        self.cpu.mem_write(0x4D280000, self._zero_mbox)
        for addr in self.touched_mmio:
            self.cpu.mem_write(addr, b"\x00\x00\x00\x00")
        self.touched_mmio.clear()
        self.read_plan.clear()
        self.read_counts.clear()
        self.transactions.clear()
        self.stub_calls.clear()
        self.track_stub_calls = False
        self.panic_null_varargs = 3

        self.cpu.reg_write(UC_ARM_REG_PRIMASK, 0)
        self.cpu.reg_write(UC_ARM_REG_BASEPRI, 0)
        self.cpu.reg_write(UC_ARM_REG_IPSR, 0)
        # Default g_rtk_irq_mask_basepri (0x1002D2BC) = 0x20
        self.write_u32(0x1002D2BC, 0x20)
        # Default g_phy_ipc_state (0x1002268C): req=0x4D280000, resp=0x4D280400
        self.write_u32(0x1002268C, 0x4D280000)
        self.write_u32(0x10022690, 0x4D280400)
        # Default Cortex-M4 MPU_TYPE = 8 regions
        self.write_mmio_u32(0xE000ED90, 0x00000800)

    def write_bytes(self, addr: int, data: bytes) -> None:
        self.cpu.mem_write(addr, data)

    def write_u32(self, addr: int, val: int) -> None:
        self.cpu.mem_write(addr, struct.pack("<I", val & 0xFFFFFFFF))

    def write_u16(self, addr: int, val: int) -> None:
        self.cpu.mem_write(addr, struct.pack("<H", val & 0xFFFF))

    def write_u8(self, addr: int, val: int) -> None:
        self.cpu.mem_write(addr, bytes([val & 0xFF]))

    def write_mmio_u32(self, addr: int, val: int) -> None:
        self.touched_mmio.add(addr & ~3)
        self.cpu.mem_write(addr, struct.pack("<I", val & 0xFFFFFFFF))

    def read_bytes(self, addr: int, length: int) -> bytes:
        return bytes(self.cpu.mem_read(addr, length))

    def run_call(
        self,
        entry_addr: int,
        args: list[int] | tuple[int, ...] = (),
        *,
        primask: int = 0,
        basepri: int = 0,
        ipsr: int = 0,
        sp: int = STACK_TOP,
        until_pc: int = STOP_PC,
        limit: int = 250000,
    ) -> dict[str, Any]:
        self.cpu.reg_write(UC_ARM_REG_PRIMASK, primask)
        self.cpu.reg_write(UC_ARM_REG_BASEPRI, basepri)
        self.cpu.reg_write(UC_ARM_REG_IPSR, ipsr)
        self.cpu.reg_write(UC_ARM_REG_SP, sp)
        self.cpu.reg_write(UC_ARM_REG_LR, STOP_PC | 1)
        self.cpu.reg_write(UC_ARM_REG_R12, 0xDEAD0C00)
        for cs_reg, cs_sentinel in CALLEE_SAVED_SENTINELS:
            self.cpu.reg_write(cs_reg, cs_sentinel)

        for reg in ARG_REGS:
            self.cpu.reg_write(reg, 0)
        for idx, val in enumerate(args):
            if idx < 4:
                self.cpu.reg_write(ARG_REGS[idx], val & 0xFFFFFFFF)
            else:
                self.write_u32(sp + 4 * (idx - 4), val)

        self.transactions.clear()
        self.stub_calls.clear()
        self.read_counts.clear()
        self.cpu.emu_start(entry_addr | 1, until_pc, count=limit)
        end_pc = self.cpu.reg_read(UC_ARM_REG_PC)
        if end_pc != until_pc:
            raise RuntimeError(f"execution did not stop at 0x{until_pc:08x} (pc=0x{end_pc:08x})")

        end_sp = self.cpu.reg_read(UC_ARM_REG_SP)
        callee_saved_ok = True
        if until_pc == STOP_PC:
            callee_saved_ok = (end_sp == sp) and all(
                self.cpu.reg_read(cs_reg) == cs_sentinel
                for cs_reg, cs_sentinel in CALLEE_SAVED_SENTINELS
            )

        return {
            "r0": self.cpu.reg_read(UC_ARM_REG_R0),
            "r1": self.cpu.reg_read(UC_ARM_REG_R1),
            "primask": self.cpu.reg_read(UC_ARM_REG_PRIMASK),
            "basepri": self.cpu.reg_read(UC_ARM_REG_BASEPRI),
            "callee_saved_ok": callee_saved_ok,
            "transactions": list(self.transactions),
            "stub_calls": list(self.stub_calls),
        }


def build_cases() -> list[dict[str, Any]]:
    cases: list[dict[str, Any]] = []

    def add(
        suite: str,
        case_id: str,
        c_symbol: str,
        args: list[int] | tuple[int, ...] = (),
        *,
        setup=None,
        watch: list[tuple[int, int]] = (),
        check_r1: bool = False,
        check_r0: bool = True,
        primask: int = 0,
        basepri: int = 0,
        ipsr: int = 0,
        sp: int = STACK_TOP,
        stub_pending: bool = False,
        stop_at_halt: bool = False,
    ) -> None:
        cases.append({
            "suite": suite,
            "case_id": case_id,
            "c_symbol": c_symbol,
            "args": list(args),
            "setup": setup,
            "watch": list(watch),
            "check_r0": check_r0,
            "check_r1": check_r1,
            "primask": primask,
            "basepri": basepri,
            "ipsr": ipsr,
            "sp": sp,
            "stub_pending": stub_pending,
            "stop_at_halt": stop_at_halt,
        })

    # =========================================================================
    # Suite 1: Boot, 7-Region MPU, CPU Hz Calibration, Clock & Board Props
    # =========================================================================
    add("boot_mpu", "mpu_default_attr", "acib_mpu_default_attr", args=[0xDEAD])
    add("boot_mpu", "mpu_enable", "acib_mpu_enable",
        setup=lambda m: m.write_mmio_u32(0xE000ED94, 0x00000004))
    add("boot_mpu", "mpu_disable", "acib_mpu_disable",
        setup=lambda m: m.write_mmio_u32(0xE000ED94, 0x00000005))
    add("boot_mpu", "mpu_config_region5_mbox", "acib_mpu_config_region",
        args=[5, 0x4D280000, 0x0B, 0, 0x00200000, 1, 1])
    add("boot_mpu", "mpu_config_region_oob", "acib_mpu_config_region",
        args=[9, 0x4D280000, 0x0B, 0, 0x00200000, 1, 1])
    add("boot_mpu", "rtk_early_hw_init_7regions", "rtk_early_hw_init",
        check_r0=False)
    add("boot_mpu", "dwt_cyccnt_read", "acib_dwt_cyccnt_read",
        setup=lambda m: m.write_mmio_u32(0xE0001004, 0x12345678))

    def setup_measure_hz(m: ArmDiffMachine) -> None:
        m.read_plan[0x52160020] = {1: 100, 2: 100, 3: 101, 4: 111, 5: 126}
        m.read_plan[0xE0001004] = {1: 1000, 2: 1200, 3: 3120, 4: 6000}

    add("boot_mpu", "measure_cpu_hz_192mhz", "acib_measure_cpu_hz",
        args=[0x52160000], setup=setup_measure_hz)

    def setup_frozen_timer(m: ArmDiffMachine) -> None:
        m.write_mmio_u32(0x52160020, 0x5555)
        m.write_u32(0x10020968, 160000000)

    add("boot_mpu", "calibrated_hz_frozen_fallback", "acib_get_calibrated_cpu_hz",
        setup=setup_frozen_timer)

    def setup_clock_init_192m(m: ArmDiffMachine) -> None:
        m.read_plan[0x52160020] = {1: 10, 2: 11, 3: 11, 4: 11, 5: 12, 6: 37}
        m.read_plan[0xE0001004] = {1: 500, 2: 5300}

    add("boot_mpu", "clock_timing_init_192mhz", "acib_clock_timing_init",
        setup=setup_clock_init_192m, watch=[(0x10020968, 0x44)])

    def setup_clock_init_96m(m: ArmDiffMachine) -> None:
        m.read_plan[0x52160020] = {1: 10, 2: 11, 3: 11, 4: 11, 5: 12, 6: 37}
        m.read_plan[0xE0001004] = {1: 500, 2: 2900}

    add("boot_mpu", "clock_timing_init_96mhz_lowfreq", "acib_clock_timing_init",
        setup=setup_clock_init_96m, watch=[(0x10020968, 0x44)])

    add("boot_mpu", "clear_sleep_deadline", "acib_clear_sleep_deadline",
        check_r0=False, watch=[(0x10020994, 4)])
    add("boot_mpu", "load_rtkit_board_props", "acib_load_rtkit_board_props",
        watch=[(0x10022984, 0x10)])
    add("boot_mpu", "board_set_flags_none", "acib_board_set_flags",
        setup=lambda m: m.write_u32(0x10022984, 0x00), watch=[(0x1002006C, 40)])
    add("boot_mpu", "board_set_flags_bit1", "acib_board_set_flags",
        setup=lambda m: m.write_u32(0x10022984, 0x02), watch=[(0x1002006C, 40)])
    add("boot_mpu", "board_set_flags_bit2", "acib_board_set_flags",
        setup=lambda m: m.write_u32(0x10022984, 0x04), watch=[(0x1002006C, 40)])
    add("boot_mpu", "board_has_flag_bit0_true", "acib_board_has_flag_bit0",
        setup=lambda m: m.write_u32(0x10022984, 0x01))
    add("boot_mpu", "board_has_flag_bit0_false", "acib_board_has_flag_bit0",
        setup=lambda m: m.write_u32(0x10022984, 0x08))
    add("boot_mpu", "board_get_rev_nibble_7", "acib_board_get_rev_nibble",
        setup=lambda m: m.write_u32(0x10022984, 0x70),
        watch=[(0x1002006C + 36, 4)])
    add("boot_mpu", "board_get_rev_nibble_0", "acib_board_get_rev_nibble",
        setup=lambda m: m.write_u32(0x10022984, 0x00),
        watch=[(0x1002006C + 36, 4)])
    add("boot_mpu", "soc_is_fpga_true", "acib_soc_is_fpga_emulation",
        setup=lambda m: m.write_mmio_u32(0x520DC0D8, 0x1234FC00))
    add("boot_mpu", "soc_is_fpga_false", "acib_soc_is_fpga_emulation",
        setup=lambda m: m.write_mmio_u32(0x520DC0D8, 0x12340500))
    add("boot_mpu", "rtk_get_wake_count", "rtk_get_wake_count",
        setup=lambda m: m.write_u32(0x10000064, 42))
    add("boot_mpu", "rtk_get_sleep_count", "rtk_get_sleep_count",
        setup=lambda m: m.write_u32(0x10000068, 19))
    add("boot_mpu", "stack_watermark_fill_32b", "rtk_stack_watermark_fill",
        args=[SCRATCH, 32], check_r0=False, watch=[(SCRATCH, 32)])

    def setup_wm_check(m: ArmDiffMachine) -> None:
        blob = struct.pack("<8I",
            0x534B5452, 0x4B434154,
            0x534B5452, 0x4B434154,
            0xDEADBEEF, 0x4B434154,
            0x11111111, 0x22222222,
        )
        m.write_bytes(SCRATCH, blob)

    add("boot_mpu", "stack_watermark_check_corrupted", "rtk_stack_watermark_check",
        args=[SCRATCH, 32], setup=setup_wm_check)

    # =========================================================================
    # Suite 2: Exception & Fault Frame Extraction
    # =========================================================================
    add("exception", "ctx_store_fault_arg", "acib_ctx_store_fault_arg",
        args=[SCRATCH, 0xCAFEBABE], check_r0=False, watch=[(SCRATCH + 0x1A0, 4)])

    def setup_fault_frame(exc_lr: int):
        def _fn(m: ArmDiffMachine) -> None:
            ctx = bytearray(0xB0)
            struct.pack_into("<IIII", ctx, 0x10, 0xA0A1A2A3, 0xB0B1B2B3, exc_lr, 0x1001BC40)
            struct.pack_into("<III", ctx, 0x54, 0x0007C821, 0x00084EB8, 0x61000000)
            struct.pack_into("<III", ctx, 0x94, 0x00020A59, 0x0007CD5C, 0x21000000)
            m.write_bytes(SCRATCH, bytes(ctx))
        return _fn

    add("exception", "fault_extract_std_frame", "rtk_fault_extract_frame",
        args=[SCRATCH, SCRATCH + 0x200],
        setup=setup_fault_frame(0xFFFFFFFD),
        check_r0=False, watch=[(SCRATCH + 0x200, 0x38)])
    add("exception", "fault_extract_fpu_frame", "rtk_fault_extract_frame",
        args=[SCRATCH, SCRATCH + 0x200],
        setup=setup_fault_frame(0xFFFFFFED),
        check_r0=False, watch=[(SCRATCH + 0x200, 0x38)])

    # =========================================================================
    # Suite 3: IRQ Masking, NVIC Methods & Priority-Bitmap Scheduler
    # =========================================================================
    add("irq_nvic_sched", "irq_mask_save_from_0", "rtk_irq_mask_save", basepri=0x00)
    add("irq_nvic_sched", "irq_mask_save_from_0x10_higher", "rtk_irq_mask_save", basepri=0x10)
    add("irq_nvic_sched", "irq_mask_save_from_0x20_equal", "rtk_irq_mask_save", basepri=0x20)
    add("irq_nvic_sched", "irq_mask_save_from_0x40_lower", "rtk_irq_mask_save", basepri=0x40)
    add("irq_nvic_sched", "irq_mask_restore", "rtk_irq_mask_restore",
        args=[0x40], basepri=0x20, check_r0=False)
    add("irq_nvic_sched", "primask_disable_from_0", "rtk_primask_disable_save", primask=0)
    add("irq_nvic_sched", "primask_disable_from_1", "rtk_primask_disable_save", primask=1)
    add("irq_nvic_sched", "primask_restore_1", "rtk_primask_restore",
        args=[1], primask=1, check_r0=False)
    add("irq_nvic_sched", "primask_restore_0", "rtk_primask_restore",
        args=[0], primask=1, check_r0=False)
    add("irq_nvic_sched", "irq_enable_all", "rtk_irq_enable_all",
        primask=1, basepri=0x40, check_r0=False)
    add("irq_nvic_sched", "irq_are_enabled_yes", "rtk_irq_are_enabled", primask=0, basepri=0)
    add("irq_nvic_sched", "irq_are_enabled_masked", "rtk_irq_are_enabled", primask=0, basepri=0x20)
    add("irq_nvic_sched", "irq_set_prio_group_invalid", "rtk_irq_set_prio_group", args=[3])
    add("irq_nvic_sched", "irq_set_prio_group_8", "rtk_irq_set_prio_group",
        args=[8], watch=[(0x1002D2BC, 8)])
    add("irq_nvic_sched", "cpu_in_thread_mode_yes", "rtk_cpu_in_thread_mode", ipsr=0)
    add("irq_nvic_sched", "cpu_in_thread_mode_no_isr", "rtk_cpu_in_thread_mode", ipsr=16)
    add("irq_nvic_sched", "outlined_clz_is_zero", "outlined_sub_EF0DC", args=[0])
    add("irq_nvic_sched", "outlined_clz_nonzero", "outlined_sub_EF0DC", args=[5])
    add("irq_nvic_sched", "spinlock_try_enter_free", "rtk_spinlock_try_enter",
        args=[SCRATCH], watch=[(SCRATCH, 4)])
    add("irq_nvic_sched", "spinlock_try_enter_busy", "rtk_spinlock_try_enter",
        args=[SCRATCH], setup=lambda m: m.write_u32(SCRATCH, 1), watch=[(SCRATCH, 4)])

    def setup_sched_switch(m: ArmDiffMachine) -> None:
        t_cur = SCRATCH
        t_nxt = SCRATCH + 0x80
        sp_cur_slot = SCRATCH + 0x100
        sp_nxt_slot = SCRATCH + 0x104
        m.write_u8(t_cur + 0x16, 1)
        m.write_u32(t_cur + 0x28, sp_cur_slot)
        m.write_u8(t_nxt + 0x16, 1)
        m.write_u32(t_nxt + 0x28, sp_nxt_slot)
        m.write_u32(sp_cur_slot, 0x1001A100)
        m.write_u32(sp_nxt_slot, 0x1001A900)
        m.write_u32(0x10002568, t_cur)
        m.write_u8(0x1002D240, 1)         # g_rtk_sched_need_resched = 1
        m.write_u32(0x1002D0C8, 1 << 27)  # prio 27 ready
        m.write_u32(0x1002D0D0 + 4 * (2 * 27), t_nxt)

    add("irq_nvic_sched", "sched_switch_fastpath", "rtk_sched_switch",
        args=[0x1001A200])
    add("irq_nvic_sched", "sched_switch_preempt_p27", "rtk_sched_switch",
        args=[0x1001A200], setup=setup_sched_switch,
        watch=[(0x10002568, 4), (SCRATCH + 0x100, 8), (0x1002D240, 8)])

    def setup_nvic(m: ArmDiffMachine) -> None:
        m.write_u32(0x100002EC, 128)  # max external IRQs
        m.write_mmio_u32(0xE000E200, 1 << 21)

    def setup_nvic_clear(m: ArmDiffMachine) -> None:
        m.write_u32(0x100002EC, 128)
        m.write_mmio_u32(0xE000E200, 0)

    def setup_nvic_systick_pend(m: ArmDiffMachine) -> None:
        m.write_u32(0x100002EC, 128)
        m.write_mmio_u32(0xE000ED04, 1 << 26)

    add("irq_nvic_sched", "nvic_enable_vec37", "rtk_nvic_method_5",
        args=[0, 37, 1], setup=setup_nvic)
    add("irq_nvic_sched", "nvic_register_vec37", "rtk_nvic_method_7",
        args=[0, 37, 0x0007CD5D, 0x1234], setup=setup_nvic,
        watch=[(0x1000EEB4 + 4 * 23, 4), (0x1000F0BC + 4 * 23, 4)])
    add("irq_nvic_sched", "nvic_set_prio_vec37", "rtk_nvic_method_8",
        args=[0, 37, 8], setup=setup_nvic)
    add("irq_nvic_sched", "nvic_is_pending_vec37_true", "IRQ16_Handler",
        args=[0, 37], setup=setup_nvic)
    add("irq_nvic_sched", "nvic_is_pending_vec37_false", "IRQ16_Handler",
        args=[0, 37], setup=setup_nvic_clear)
    add("irq_nvic_sched", "nvic_is_pending_systick_true", "IRQ16_Handler",
        args=[0, 15], setup=setup_nvic_systick_pend)
    add("irq_nvic_sched", "nvic_clear_pending_vec37", "rtk_nvic_method_15",
        args=[0, 37], setup=setup_nvic)
    add("irq_nvic_sched", "nvic_set_pending_vec37", "rtk_nvic_method_6",
        args=[0, 37], setup=setup_nvic)
    add("irq_nvic_sched", "nvic_method_9_zero", "rtk_nvic_method_9", args=[0x1234])

    # =========================================================================
    # Suite 4: Libc Memory/String (Multi-Case) & 64-bit AEABI Shift Helpers
    # =========================================================================
    add("libc_aeabi", "memchr_hit", "acib_memchr",
        args=[SCRATCH, ord("E"), 12],
        setup=lambda m: m.write_bytes(SCRATCH, b"ABCDEFGH1234"))
    add("libc_aeabi", "memchr_miss", "acib_memchr",
        args=[SCRATCH, ord("Z"), 12],
        setup=lambda m: m.write_bytes(SCRATCH, b"ABCDEFGH1234"))
    add("libc_aeabi", "memchr_len0", "acib_memchr",
        args=[SCRATCH, ord("A"), 0],
        setup=lambda m: m.write_bytes(SCRATCH, b"ABCDEFGH"))
    add("libc_aeabi", "memchr_unaligned_highbit", "acib_memchr",
        args=[SCRATCH + 1, 0xFE, 31],
        setup=lambda m: m.write_bytes(SCRATCH + 1, b"\x01" * 19 + b"\xFE" + b"\x80" * 11))

    add("libc_aeabi", "memcmp_lt", "acib_memcmp",
        args=[SCRATCH, SCRATCH + 16, 8],
        setup=lambda m: (m.write_bytes(SCRATCH, b"RTKit-35"), m.write_bytes(SCRATCH + 16, b"RTKit-39")))
    add("libc_aeabi", "memcmp_gt_highbit", "acib_memcmp",
        args=[SCRATCH + 1, SCRATCH + 35, 31],
        setup=lambda m: (
            m.write_bytes(SCRATCH + 1, b"A" * 24 + b"\x80" + b"Z" * 6),
            m.write_bytes(SCRATCH + 35, b"A" * 24 + b"\x7F" + b"Z" * 6),
        ))
    add("libc_aeabi", "memcmp_eq_31b", "acib_memcmp",
        args=[SCRATCH + 1, SCRATCH + 65, 31],
        setup=lambda m: (
            m.write_bytes(SCRATCH + 1, bytes(range(10, 41))),
            m.write_bytes(SCRATCH + 65, bytes(range(10, 41))),
        ))
    add("libc_aeabi", "memcmp_len0", "acib_memcmp",
        args=[SCRATCH, SCRATCH + 16, 0],
        setup=lambda m: (m.write_bytes(SCRATCH, b"\xFF"), m.write_bytes(SCRATCH + 16, b"\x00")))

    add("libc_aeabi", "memcpy_wrapper_0b", "memcpy_wrapper",
        args=[SCRATCH + 32, SCRATCH, 0],
        setup=lambda m: (m.write_bytes(SCRATCH, b"ABCD"), m.write_bytes(SCRATCH + 32, b"WXYZ")),
        watch=[(SCRATCH + 32, 4)])
    add("libc_aeabi", "memcpy_wrapper_1b", "memcpy_wrapper",
        args=[SCRATCH + 32, SCRATCH, 1],
        setup=lambda m: m.write_bytes(SCRATCH, b"\xA5\x5A"),
        watch=[(SCRATCH + 32, 4)])
    add("libc_aeabi", "memcpy_wrapper_4b", "memcpy_wrapper",
        args=[SCRATCH + 32, SCRATCH, 4],
        setup=lambda m: m.write_bytes(SCRATCH, b"RTK5"),
        watch=[(SCRATCH + 32, 8)])
    add("libc_aeabi", "memcpy_wrapper_31b_unaligned", "memcpy_wrapper",
        args=[SCRATCH + 65, SCRATCH + 3, 31],
        setup=lambda m: m.write_bytes(SCRATCH + 3, bytes(range(0x80, 0x80 + 31))),
        watch=[(SCRATCH + 64, 36)])
    add("libc_aeabi", "memcpy_wrapper_64b", "memcpy_wrapper",
        args=[SCRATCH + 128, SCRATCH, 64],
        setup=lambda m: m.write_bytes(SCRATCH, bytes((i * 5 + 3) & 0xFF for i in range(64))),
        watch=[(SCRATCH + 128, 64)])

    add("libc_aeabi", "memmove_overlap_fwd", "memmove",
        args=[SCRATCH + 3, SCRATCH, 31],
        setup=lambda m: m.write_bytes(SCRATCH, bytes(range(0x30, 0x30 + 40))),
        watch=[(SCRATCH, 40)])
    add("libc_aeabi", "memmove_overlap_bwd", "memmove",
        args=[SCRATCH, SCRATCH + 5, 31],
        setup=lambda m: m.write_bytes(SCRATCH, bytes(range(0x60, 0x60 + 40))),
        watch=[(SCRATCH, 40)])
    add("libc_aeabi", "memmove_disjoint_0b", "memmove",
        args=[SCRATCH + 32, SCRATCH, 0],
        setup=lambda m: (m.write_bytes(SCRATCH, b"1234"), m.write_bytes(SCRATCH + 32, b"9876")),
        watch=[(SCRATCH + 32, 4)])
    add("libc_aeabi", "memmove_disjoint_1b", "memmove",
        args=[SCRATCH + 33, SCRATCH + 1, 1],
        setup=lambda m: m.write_bytes(SCRATCH + 1, b"\xFE"),
        watch=[(SCRATCH + 32, 4)])
    add("libc_aeabi", "memmove_disjoint_64b", "memmove",
        args=[SCRATCH + 128, SCRATCH, 64],
        setup=lambda m: m.write_bytes(SCRATCH, bytes((0xFF - i) for i in range(64))),
        watch=[(SCRATCH + 128, 64)])

    add("libc_aeabi", "memset_0b", "memset",
        args=[SCRATCH, 0xAA, 0],
        setup=lambda m: m.write_bytes(SCRATCH, b"\x11\x22\x33\x44"),
        watch=[(SCRATCH, 4)])
    add("libc_aeabi", "memset_1b", "memset",
        args=[SCRATCH + 1, 0xFF, 1], watch=[(SCRATCH, 4)])
    add("libc_aeabi", "memset_4b", "memset",
        args=[SCRATCH, 0x81, 4], watch=[(SCRATCH, 8)])
    add("libc_aeabi", "memset_31b_unaligned", "memset",
        args=[SCRATCH + 3, 0x5A, 31], watch=[(SCRATCH, 40)])
    add("libc_aeabi", "memset_64b_highbit", "memset",
        args=[SCRATCH, 0xE7, 64], watch=[(SCRATCH, 68)])

    add("libc_aeabi", "strlen_empty", "acib_strlen",
        args=[SCRATCH], setup=lambda m: m.write_bytes(SCRATCH, b"\x00XYZ"))
    add("libc_aeabi", "strlen_1b", "acib_strlen",
        args=[SCRATCH], setup=lambda m: m.write_bytes(SCRATCH, b"X\x00"))
    add("libc_aeabi", "strlen_tag", "acib_strlen",
        args=[SCRATCH], setup=lambda m: m.write_bytes(SCRATCH, b"AirPods3,6\x00"))
    add("libc_aeabi", "strlen_31b_unaligned_highbit", "acib_strlen",
        args=[SCRATCH + 3],
        setup=lambda m: m.write_bytes(SCRATCH + 3, b"\x85" * 31 + b"\x00"))

    add("libc_aeabi", "strncmp_prefix_eq", "acib_strncmp",
        args=[SCRATCH, SCRATCH + 16, 5],
        setup=lambda m: (m.write_bytes(SCRATCH, b"b515d_a\x00"), m.write_bytes(SCRATCH + 16, b"b515d_b\x00")))
    add("libc_aeabi", "strncmp_lt", "acib_strncmp",
        args=[SCRATCH, SCRATCH + 16, 8],
        setup=lambda m: (m.write_bytes(SCRATCH, b"b515d_a\x00"), m.write_bytes(SCRATCH + 16, b"b515d_z\x00")))
    add("libc_aeabi", "strncmp_gt_highbit", "acib_strncmp",
        args=[SCRATCH + 1, SCRATCH + 65, 31],
        setup=lambda m: (
            m.write_bytes(SCRATCH + 1, b"abc" * 7 + b"\x90\x00"),
            m.write_bytes(SCRATCH + 65, b"abc" * 7 + b"\x20\x00"),
        ))
    add("libc_aeabi", "strncmp_len0", "acib_strncmp",
        args=[SCRATCH, SCRATCH + 16, 0],
        setup=lambda m: (m.write_bytes(SCRATCH, b"X\x00"), m.write_bytes(SCRATCH + 16, b"Y\x00")))
    add("libc_aeabi", "strncmp_len1_diff", "acib_strncmp",
        args=[SCRATCH, SCRATCH + 16, 1],
        setup=lambda m: (m.write_bytes(SCRATCH, b"M\x00"), m.write_bytes(SCRATCH + 16, b"A\x00")))
    add("libc_aeabi", "strncmp_early_nul", "acib_strncmp",
        args=[SCRATCH, SCRATCH + 16, 31],
        setup=lambda m: (m.write_bytes(SCRATCH, b"rtk\x00AAA"), m.write_bytes(SCRATCH + 16, b"rtk\x00ZZZ")))

    add("libc_aeabi", "aeabi_llsl_0", "acib_aeabi_llsl",
        args=[0x89ABCDEF, 0x01234567, 0], check_r1=True)
    add("libc_aeabi", "aeabi_llsl_13", "acib_aeabi_llsl",
        args=[0x89ABCDEF, 0x01234567, 13], check_r1=True)
    add("libc_aeabi", "aeabi_llsl_32", "acib_aeabi_llsl",
        args=[0x89ABCDEF, 0x01234567, 32], check_r1=True)
    add("libc_aeabi", "aeabi_llsl_39", "acib_aeabi_llsl",
        args=[0x89ABCDEF, 0x01234567, 39], check_r1=True)
    add("libc_aeabi", "aeabi_llsl_64", "acib_aeabi_llsl",
        args=[0x89ABCDEF, 0x01234567, 64], check_r1=True)
    add("libc_aeabi", "aeabi_llsl_95", "acib_aeabi_llsl",
        args=[0x89ABCDEF, 0x01234567, 95], check_r1=True)
    add("libc_aeabi", "aeabi_llsl_96", "acib_aeabi_llsl",
        args=[0x89ABCDEF, 0x01234567, 96], check_r1=True)
    add("libc_aeabi", "aeabi_llsl_256", "acib_aeabi_llsl",
        args=[0x89ABCDEF, 0x01234567, 256], check_r1=True)

    add("libc_aeabi", "aeabi_llsr_0", "acib_aeabi_llsr",
        args=[0x76543210, 0xFEDCBA98, 0], check_r1=True)
    add("libc_aeabi", "aeabi_llsr_11", "acib_aeabi_llsr",
        args=[0x76543210, 0xFEDCBA98, 11], check_r1=True)
    add("libc_aeabi", "aeabi_llsr_32", "acib_aeabi_llsr",
        args=[0x76543210, 0xFEDCBA98, 32], check_r1=True)
    add("libc_aeabi", "aeabi_llsr_41", "acib_aeabi_llsr",
        args=[0x76543210, 0xFEDCBA98, 41], check_r1=True)
    add("libc_aeabi", "aeabi_llsr_64", "acib_aeabi_llsr",
        args=[0x76543210, 0xFEDCBA98, 64], check_r1=True)
    add("libc_aeabi", "aeabi_llsr_95", "acib_aeabi_llsr",
        args=[0x76543210, 0xFEDCBA98, 95], check_r1=True)
    add("libc_aeabi", "aeabi_llsr_96", "acib_aeabi_llsr",
        args=[0x76543210, 0xFEDCBA98, 96], check_r1=True)
    add("libc_aeabi", "aeabi_llsr_256", "acib_aeabi_llsr",
        args=[0x76543210, 0xFEDCBA98, 256], check_r1=True)

    # =========================================================================
    # Suite 5: MAC (acib) <-> W3 PHY (phyb) Shared 0x4D280000 & 0x4D154000 IPC
    # =========================================================================
    add("phy_ipc", "phy_ipc_is_ready_abba", "phy_ipc_is_ready",
        args=[SCRATCH], check_r0=False, watch=[(SCRATCH, 4)])
    add("phy_ipc", "phy_ipc_cmd_set_op2", "phy_ipc_cmd_set_op2",
        check_r0=False, watch=[(0x4D280000, 16)])
    add("phy_ipc", "phy_ipc_cmd_set_op3_u16", "phy_ipc_cmd_set_op3_u16",
        args=[5], check_r0=False, watch=[(0x4D280000, 16)])
    add("phy_ipc", "phy_ipc_noop", "phy_ipc_noop")
    add("phy_ipc", "phy_ipc_cmd_set_op6_mode", "phy_ipc_cmd_set_op6_mode",
        args=[SCRATCH, SCRATCH + 4, SCRATCH + 8],
        setup=lambda m: m.write_bytes(0x4D280400 + 0x1D5, b"\x03\x07\x78\x56\x34\x12"),
        watch=[(SCRATCH, 12)])
    add("phy_ipc", "phy_ipc_copy_cmd_payload", "phy_ipc_clear_cmd_status_flag",
        args=[SCRATCH, 12],
        setup=lambda m: m.write_bytes(SCRATCH, b"PHY_CMD_BODY"),
        watch=[(0x4D280000, 24)])
    add("phy_ipc", "phy_aci_if_enable_gates", "phy_aci_if_enable_gates",
        setup=lambda m: (m.write_mmio_u32(0x42440410, 0x11), m.write_mmio_u32(0x42440418, 0x22)))
    add("phy_ipc", "phy_ipc_cal_bit0_op9_true", "phy_ipc_cmd_op9",
        setup=lambda m: m.write_u8(0x4D280400 + 0x1D5, 0x05))
    add("phy_ipc", "phy_ipc_cal_bit0_op9_false", "phy_ipc_cmd_op9",
        setup=lambda m: m.write_u8(0x4D280400 + 0x1D5, 0x04))
    add("phy_ipc", "phy_ipc_cal_copy_41b_op10", "phy_ipc_cmd_op10",
        args=[SCRATCH],
        setup=lambda m: m.write_bytes(0x4D280400 + 0x1D5, bytes(range(10, 10 + 0x29))),
        watch=[(SCRATCH, 0x29)])
    add("phy_ipc", "phy_modem_slot_a_addr", "phy_ipc_cmd_op11_u8", args=[3])
    add("phy_ipc", "phy_modem_slot_b_addr", "phy_ipc_cmd_op12_u8", args=[2])
    add("phy_ipc", "phy_ipc_frs_prepare_preset", "phy_ipc_frs_prepare_scan",
        args=[2, 6, 8, 5, 1, 0, 0, 0],
        watch=[(0x4D2801E4, 0x18), (0x4D2805C0, 4)])
    add("phy_ipc", "phy_ipc_frs_prepare_custom_bmap", "phy_ipc_frs_prepare_scan",
        args=[0, 3, 12, 2, 1, 0, 0, SCRATCH],
        setup=lambda m: m.write_bytes(SCRATCH, bytes(range(0xA0, 0xB0))),
        watch=[(0x4D2801E4, 0x18), (0x4D2805C0, 4)])

    add("phy_ipc", "phy_ipc_frs_validate_valid_ch5", "phy_ipc_frs_validate_param",
        args=[4, 2, 0x05])
    add("phy_ipc", "phy_ipc_frs_validate_invalid_ch38", "phy_ipc_frs_validate_param",
        args=[4, 2, 0x26], stub_pending=True)
    add("phy_ipc", "phy_ipc_frs_complete_busy", "phy_ipc_frs_is_scan_complete",
        setup=lambda m: m.write_u8(0x4D2805C0, 0xFF))
    add("phy_ipc", "phy_ipc_frs_complete_done", "phy_ipc_frs_is_scan_complete",
        setup=lambda m: m.write_u8(0x4D2805C0, 0x00))
    add("phy_ipc", "phy_ipc_coex_ring_doorbell", "phy_ipc_coex_ring_doorbell",
        args=[SCRATCH],
        setup=lambda m: m.write_u32(SCRATCH, 0xA55A3CC3),
        watch=[(0x4D2801F8, 8)])

    def setup_frs_results(m: ArmDiffMachine) -> None:
        m.write_u8(0x4D2805C0, 0)
        rssi = b"".join(struct.pack("<h", -90 + i) for i in range(16))
        m.write_bytes(0x4D2804C0, rssi)
        m.write_bytes(0x4D2805FE, bytes(range(1, 17)))

    add("phy_ipc", "phy_ipc_frs_copy_results", "phy_ipc_frs_copy_results",
        args=[2, 8, SCRATCH, 10], setup=setup_frs_results,
        watch=[(SCRATCH, 16), (0x100017E7, 12)])
    add("phy_ipc", "phy_ipc_frs_copy_results_busy_err", "phy_ipc_frs_copy_results",
        args=[2, 8, SCRATCH, 10],
        setup=lambda m: m.write_u8(0x4D2805C0, 0xFF),
        stub_pending=True)
    add("phy_ipc", "phy_ipc_frs_read_window", "phy_ipc_frs_read_window",
        args=[10, 6, SCRATCH], setup=setup_frs_results,
        watch=[(SCRATCH, 12), (0x100017E7, 10)])
    add("phy_ipc", "phy_ipc_coex_noop", "phy_ipc_coex_noop")
    add("phy_ipc", "phy_ipc_coex_get_active_1", "phy_ipc_coex_get_active",
        setup=lambda m: m.write_u8(0x10022990, 1))
    add("phy_ipc", "phy_ipc_coex_get_active_0", "phy_ipc_coex_get_active",
        setup=lambda m: m.write_u8(0x10022990, 0))
    add("phy_ipc", "phy_ipc_on_cmd_ack_clear_bit14", "phy_ipc_on_cmd_ack_clear_flags",
        setup=lambda m: (m.write_mmio_u32(0x52205000, 0x4567), m.write_mmio_u32(0x52205028, 0x7654)),
        check_r0=False, watch=[(0x1002A168, 16)])

    # =========================================================================
    # Suite 6: ACI Trace Manager, OSA Mailbox Relay & BT Controller
    # =========================================================================
    add("aci_trace_osa_bt", "tm_cb_init_pool0", "tm_buffer_util_cb_init",
        args=[0],
        setup=lambda m: (m.write_u32(0x1002CF68, 0xFFFFFFFF), m.write_u32(0x1002CF6C, 0xFFFFFFFF)),
        watch=[(0x1002CF68, 8)])
    add("aci_trace_osa_bt", "tm_cb_init_pool1_panic", "tm_buffer_util_cb_init",
        args=[1], stub_pending=True, stop_at_halt=True)
    add("aci_trace_osa_bt", "tm_bind_cb", "tm_buffer_util_bind",
        args=[0x1002CF68, SCRATCH, 128], check_r0=False, watch=[(0x1002CF68, 8)])

    def setup_tm_cb(m: ArmDiffMachine) -> None:
        m.write_u32(0x1002CF68, SCRATCH)
        m.write_u16(0x1002CF6C, 24)
        m.write_u16(0x1002CF6E, 100)

    add("aci_trace_osa_bt", "tm_get_free", "tm_buffer_util_get_free",
        args=[0x1002CF68], setup=setup_tm_cb)
    add("aci_trace_osa_bt", "tm_dec_remaining", "tm_buffer_util_decrement_remaining",
        args=[0x1002CF68, 16], setup=setup_tm_cb, check_r0=False, watch=[(0x1002CF68, 8)])
    add("aci_trace_osa_bt", "tm_get_used", "tm_buffer_util_get_used",
        args=[0x1002CF68], setup=setup_tm_cb)
    add("aci_trace_osa_bt", "tm_reset", "tm_buffer_util_reset",
        args=[0x1002CF68], setup=setup_tm_cb, check_r0=False, watch=[(0x1002CF68, 8)])
    add("aci_trace_osa_bt", "tm_has_storage_true", "tm_buffer_util_has_storage",
        args=[0x1002CF68], setup=setup_tm_cb)
    add("aci_trace_osa_bt", "tm_has_storage_false", "tm_buffer_util_has_storage",
        args=[0x1002CF68], setup=lambda m: m.write_u32(0x1002CF68, 0))
    add("aci_trace_osa_bt", "tm_write_ptr", "tm_buffer_util_write_ptr",
        args=[0x1002CF68], setup=setup_tm_cb)
    add("aci_trace_osa_bt", "tm_base_ptr", "tm_buffer_util_base_ptr",
        args=[0x1002CF68], setup=setup_tm_cb)
    add("aci_trace_osa_bt", "tm_pkt_write_hdr4", "tm_pkt_write_hdr4",
        args=[SCRATCH + 32, SCRATCH],
        setup=lambda m: m.write_bytes(SCRATCH, struct.pack("<IH", 0x11223344, 0x0055)),
        watch=[(SCRATCH + 32, 8)])
    add("aci_trace_osa_bt", "tm_pkt_write_hdr8", "tm_pkt_write_hdr8",
        args=[SCRATCH + 32, SCRATCH, 2],
        setup=lambda m: m.write_bytes(SCRATCH, struct.pack("<IIB", 0xAABBCCDD, 0x55667788, 0x3C)),
        watch=[(SCRATCH + 32, 20)])
    add("aci_trace_osa_bt", "tm_pkt_write_ts_ev_k1", "tm_pkt_write_ts_event",
        args=[SCRATCH + 32, SCRATCH],
        setup=lambda m: m.write_bytes(SCRATCH, b"\x01\x03\x00\x00\x11\x22\x33\x44\x55\x66\x77\x88\x9A"),
        watch=[(SCRATCH + 32, 20)])
    add("aci_trace_osa_bt", "tm_pkt_write_ts_ev_k2", "tm_pkt_write_ts_event",
        args=[SCRATCH + 32, SCRATCH],
        setup=lambda m: m.write_bytes(SCRATCH, b"\x02\x00\x00\x00\x77\x00\x34\x12"),
        watch=[(SCRATCH + 32, 4)])
    add("aci_trace_osa_bt", "tm_pkt_write_ts_ev_k3", "tm_pkt_write_ts_event",
        args=[SCRATCH + 32, SCRATCH],
        setup=lambda m: m.write_bytes(SCRATCH, b"\x03\x00\x00\x00\x77\x00\x34\x12"),
        watch=[(SCRATCH + 32, 4)])
    add("aci_trace_osa_bt", "tm_pkt_write_ts_ev_k4", "tm_pkt_write_ts_event",
        args=[SCRATCH + 32, SCRATCH],
        setup=lambda m: m.write_bytes(SCRATCH, b"\x04\x00\x00\x00\x77\x00\x34\x12"),
        watch=[(SCRATCH + 32, 4)])
    add("aci_trace_osa_bt", "tm_pkt_write_ts_ev_k5", "tm_pkt_write_ts_event",
        args=[SCRATCH + 32, SCRATCH],
        setup=lambda m: m.write_bytes(SCRATCH, b"\x05\x00\x00\x00\xEF\xBE\xAD\xDE\x25\x00"),
        watch=[(SCRATCH + 32, 8)])

    add("aci_trace_osa_bt", "osa_buf_tag_owner", "osa_buf_tag_owner",
        args=[SCRATCH + 4],
        setup=lambda m: m.write_u32(SCRATCH, 0x12345678),
        check_r0=False, watch=[(SCRATCH, 4)])

    def setup_osa_crash(m: ArmDiffMachine) -> None:
        m.write_u8(0x10020B78 + 0x6AD, 3)
        m.write_bytes(0x10020B78 + 28 * 2, bytes(range(1, 29)))
        m.write_bytes(0x10020B78 + 28 * 3, bytes(range(40, 68)))

    add("aci_trace_osa_bt", "osa_crash_copy_events", "osa_crash_copy_recent_events",
        args=[SCRATCH, 64], setup=setup_osa_crash, watch=[(SCRATCH, 60)])
    add("aci_trace_osa_bt", "osa_task_clear_pending", "osa_task_clear_pending_flag",
        args=[5], setup=lambda m: m.write_u8(0x10021228 + 16 * 5 + 12, 1),
        check_r0=False, watch=[(0x10021228 + 16 * 5, 16)])
    add("aci_trace_osa_bt", "osa_mbx_relay_dest_0x0000", "osa_mbx_get_relay_dest",
        args=[0x0000])
    add("aci_trace_osa_bt", "osa_mbx_relay_dest_0x4200", "osa_mbx_get_relay_dest",
        args=[0x4200])
    add("aci_trace_osa_bt", "osa_mbx_relay_dest_invalid_panic", "osa_mbx_get_relay_dest",
        args=[0xB234], stub_pending=True, stop_at_halt=True)
    add("aci_trace_osa_bt", "osa_task_get_msg_count", "osa_task_get_msg_count",
        args=[3], setup=lambda m: m.write_u16(0x10021228 + 16 * 3 + 8, 0x0123))

    add("aci_trace_osa_bt", "hci_ber_opcode_0", "hci_test_mode_get_opcode_from_type", args=[0])
    add("aci_trace_osa_bt", "hci_ber_opcode_4", "hci_test_mode_get_opcode_from_type", args=[4])
    add("aci_trace_osa_bt", "bt_bdaddr_swap", "bt_bdaddr_swap",
        args=[SCRATCH, SCRATCH + 16],
        setup=lambda m: m.write_bytes(SCRATCH, b"\xAA\xBB\xCC\xDD\xEE\xFF"),
        check_r0=False, watch=[(SCRATCH + 16, 6)])

    def setup_ts_ring(m: ArmDiffMachine) -> None:
        m.write_u32(0x10001644, 8)         # cap
        m.write_u32(0x10001648, SCRATCH)   # ring base
        m.write_u32(0x1000164C, 2)         # idx
        m.write_mmio_u32(0x52160020, 0x123456)

    add("aci_trace_osa_bt", "bt_ts_ring_record", "bt_ts_ring_record",
        args=[0x2AA], setup=setup_ts_ring, check_r0=False,
        watch=[(0x1000164C, 8), (SCRATCH, 16)])
    add("aci_trace_osa_bt", "bt_ts_ring_record_alt", "bt_ts_ring_record_alt",
        args=[0x155], setup=setup_ts_ring, check_r0=False,
        watch=[(0x1000164C, 8), (SCRATCH, 16)])
    add("aci_trace_osa_bt", "bt_ctrl_set_mode", "acib_set_bt_ctrl_mode",
        args=[7], check_r0=False, watch=[(0x10020B78 + 0x6AC, 4)])
    add("aci_trace_osa_bt", "bt_ctrl_get_mode", "acib_get_bt_ctrl_mode",
        setup=lambda m: m.write_u8(0x10020B78 + 0x6AC, 9))
    add("aci_trace_osa_bt", "bt_conn_slot_state", "acib_get_conn_slot_state",
        args=[4], setup=lambda m: m.write_u8(0x10020B78 + 28 * 4 + 0x18, 0x33))

    # =========================================================================
    # Suite 7: Full Coverage of Remaining Reconstructed Entry Points (28 fns)
    # =========================================================================
    add("boot_mpu", "rtk_init_stacks_watermark_psp", "rtk_init_stacks_watermark",
        sp=0x1001B500, check_r0=False,
        watch=[(0x1001AF80, 0x400), (0x1001B580, 0x800)])

    add("exception_unwind", "rtk_abort_panic_halt", "rtk_abort_panic",
        args=[SCRATCH, 0x11, 0x22, 0x33],
        setup=lambda m: m.write_bytes(SCRATCH, b"panic %x %x %x\x00"),
        stub_pending=True, stop_at_halt=True)
    add("exception_unwind", "stack_chk_fail_halt", "__stack_chk_fail",
        stub_pending=True, stop_at_halt=True)
    add("exception_unwind", "acib_panic_null_halt", "acib_panic_null",
        args=[0xAA, 0xBB, 0x33, 0x44], stub_pending=True, stop_at_halt=True)
    add("exception_unwind", "acib_panic_code_line_halt", "acib_panic_code_line",
        args=[0x12345, 0x99], stub_pending=True, stop_at_halt=True)
    add("exception_unwind", "acib_panic_code_group_halt", "acib_panic_code_group",
        args=[0x54321, 0x77, 0x88, 0x99], stub_pending=True, stop_at_halt=True)
    add("exception_unwind", "rtk_unwind_adjust_lr_bl", "rtk_unwind_adjust_lr",
        args=[0x000001DF], stub_pending=True)

    def setup_systick_cb(m: ArmDiffMachine) -> None:
        m.patch_code(0x000FE100, bytes.fromhex("2a207047"))
        m.write_u32(SCRATCH + 0x0C, 0x000FE101)
        m.write_u32(0x10002530, SCRATCH)

    add("irq_nvic", "systick_handler_dispatch", "SysTick_Handler",
        args=[0, 15], setup=setup_systick_cb)
    add("irq_nvic", "nvic_method_4_ipsr", "rtk_nvic_method_4",
        args=[0], ipsr=23)

    def setup_nvic_dispatch(m: ArmDiffMachine) -> None:
        m.write_mmio_u32(0xE000E004, 0x00000003)  # ICTR -> 128 IRQs
        m.patch_code(0x000FE110, bytes.fromhex("77207047"))
        slot = 20 - 14
        m.write_u32(0x1000EEB4 + 4 * slot, 0x000FE111)
        m.write_u32(0x1000F0BC + 4 * slot, 0xCAFE0020)

    add("irq_nvic", "nvic_dispatch_irq_vec20", "rtk_nvic_dispatch_irq",
        ipsr=20, setup=setup_nvic_dispatch)

    add("phy_ipc", "phy_hw_trig_frs_start_arm", "phy_hw_trig_frs_start_arm",
        args=[0x521D8000], stub_pending=True)
    add("phy_ipc", "phy_hw_trig_frs_abort_arm", "phy_hw_trig_frs_abort_arm",
        args=[0x521D8000], stub_pending=True)
    add("phy_ipc", "phy_ipc_ring_cmd_doorbell_async", "phy_ipc_ring_cmd_doorbell",
        args=[0, 100])
    add("phy_ipc", "phy_ipc_exec_cmd_async", "phy_ipc_exec_cmd",
        args=[6, 0, 100], watch=[(0x4D280000, 8)])
    add("phy_ipc", "phy_ipc_get_cached_version_sleep", "phy_ipc_get_cached_version",
        watch=[(0x4D280000, 8)])
    add("phy_ipc", "phy_ipc_cmd_op13_pair_m2", "phy_ipc_cmd_op13_pair",
        args=[2, SCRATCH], watch=[(SCRATCH, 4)])

    def setup_w1c_cmd_ack(m: ArmDiffMachine) -> None:
        m.w1c_4d154c64 = True
        m.write_mmio_u32(0x4D154C64, 0x8000)
        m.write_u8(0x4D280408, 1)

    def setup_timer_timeout(m: ArmDiffMachine) -> None:
        m.w1c_4d154c64 = True
        m.write_u32(0x10020974, 1)
        m.read_plan[0xE0001004] = {1: 100, 2: 700, 3: 1400}

    add("phy_ipc", "phy_ipc_wait_and_ack_cmd_ok", "phy_ipc_wait_and_ack_cmd",
        args=[1, 500], setup=setup_w1c_cmd_ack, stub_pending=True)
    add("phy_ipc", "phy_ipc_wait_and_ack_cmd_timeout", "phy_ipc_wait_and_ack_cmd",
        args=[1, 500], setup=setup_timer_timeout, stub_pending=True)
    add("phy_ipc", "phy_ipc_query_op3_ready_ok", "phy_ipc_query_op3_ready",
        args=[0], setup=setup_w1c_cmd_ack, stub_pending=True,
        watch=[(0x4D280000, 8)])
    add("phy_ipc", "phy_ipc_complete_async_cmd_ok", "phy_ipc_complete_async_cmd",
        args=[500, 1], setup=setup_w1c_cmd_ack, stub_pending=True)
    add("phy_ipc", "phy_ipc_complete_async_cmd_timeout", "phy_ipc_complete_async_cmd",
        args=[500, 1], setup=setup_timer_timeout, stub_pending=True)

    def setup_w1c_warm_ack(m: ArmDiffMachine) -> None:
        m.w1c_4d154c64 = True
        m.write_mmio_u32(0x4D154C64, 0x0002)

    add("phy_ipc", "phy_ipc_wait_warm_boot_ack_ok", "phy_ipc_wait_warm_boot_ack",
        args=[500], setup=setup_w1c_warm_ack, stub_pending=True)

    def setup_dekker_sync(m: ArmDiffMachine) -> None:
        m.write_u16(0x4D2800C0, 0)
        m.write_u16(0x4D2800C2, 0)
        m.write_bytes(0x10021EA9, bytes((i * 7) & 0xFF for i in range(0x120)))

    add("phy_ipc", "phy_ipc_sync_dekker_288b", "phy_ipc_sync_shared_table_dekker",
        args=[1], setup=setup_dekker_sync, stub_pending=True,
        watch=[(0x4D2800C0, 0x124)])

    def setup_phy_init(m: ArmDiffMachine) -> None:
        m.w1c_4d154c64 = True
        m.write_mmio_u32(0x4D154C64, 0x8000)
        m.write_u32(0x1002096C, 40000000)
        m.write_bytes(0x4D280408, struct.pack("<4H", 3, 0x5B, 1, 0))
        m.write_bytes(0x10021EA9, bytes(range(0x20)) * 9)

    add("phy_ipc", "phy_ipc_init_b0_handshake", "phy_ipc_init",
        setup=setup_phy_init, stub_pending=True, check_r0=False,
        watch=[(0x1002268C, 0x20), (0x4D280000, 0x1F0), (0x10001868, 4)])

    def setup_phy_irq_crash(m: ArmDiffMachine) -> None:
        m.w1c_4d154c64 = True
        m.write_mmio_u32(0x4D154C64, 0x0001)
        m.write_bytes(0x4D2805C1, struct.pack("<5I", 11, 22, 33, 44, 55))

    add("phy_ipc", "phy_ipc_irq_handler_crash_5d", "phy_ipc_irq_handler",
        setup=setup_phy_irq_crash, stub_pending=True, stop_at_halt=True,
        watch=[(0x10020B60, 4)])

    def setup_frs_run(m: ArmDiffMachine) -> None:
        m.write_u8(0x4D2805C0, 0x00)
        m.write_u8(0x10001752, 1)
        m.write_u8(0x10001754, 0xFF)
        m.write_mmio_u32(0x52160020, 1000)
        m.read_plan[0x52160020] = {1: 1000, 2: 1245}
        rssi = b"".join(struct.pack("<h", -85 + (i % 40)) for i in range(0x7E))
        m.write_bytes(0x4D2804C0, rssi)

    add("phy_ipc", "phy_ipc_frs_run_scan_pack189b", "phy_ipc_frs_run_scan",
        args=[1, 2, 8, 1, 1, SCRATCH, SCRATCH + 0x40, SCRATCH + 0x120],
        setup=setup_frs_run, stub_pending=True,
        watch=[(0x4D2801E4, 0x18), (SCRATCH + 0x40, 0xBD), (SCRATCH + 0x120, 2)])

    add("phy_ipc", "phy_seq_rf_ctrl_apply_tx_mode", "phy_seq_rf_ctrl_apply_tx_mode",
        args=[1, 7],
        setup=lambda m: (m.write_u32(0x100253A0 + 24, 0x2A), m.write_mmio_u32(0x4D154038, 0x20000155)),
        stub_pending=True)

    add("aci_trace_osa_bt", "bt_system_thread_init_seq", "bt_system_thread_init",
        stub_pending=True, check_r0=False,
        watch=[(0x10020310, 16), (0x10021C04, 32)])
    add("aci_trace_osa_bt", "acib_log_build_banner_seq", "acib_log_build_banner_and_start",
        stub_pending=True,
        watch=[(0x10020310, 16)])
    add("aci_trace_osa_bt", "acib_log_build_banner_extra_str", "acib_log_build_banner_and_start",
        setup=lambda m: m.patch_code(0x000F53A0, b"ExtraTag-B0\x00"),
        stub_pending=True,
        watch=[(0x10020310, 16)])
    add("aci_trace_osa_bt", "acib_log_build_banner_empty_date", "acib_log_build_banner_and_start",
        setup=lambda m: m.patch_code(0x000F5360, b"\x00"),
        stub_pending=True,
        watch=[(0x10020310, 16)])
    add("aci_trace_osa_bt", "main_entry_full_boot_seq", "main_entry",
        stub_pending=True,
        watch=[(0x10020310, 16), (0x10021C04, 32)])

    return cases


def _make_default_arena_blob() -> bytes:
    """Deterministic 16 KB scratch arena (0x1002E000..0x10031FFF) with valid nested pointers."""
    buf = bytearray(0x4000)
    # Blocks 0..3 at +0x0000, +0x0800, +0x1000, +0x1800 hold struct/packet headers
    # with valid nested 32-bit pointers into sub-blocks at +0x2000..+0x3C00 whose low
    # 16 bits are small offsets (0x0020..0x03E0) so both 32-bit pointer derefs and
    # 16-bit length/offset reads stay safely in-bounds.
    for blk in range(4):
        base_off = blk * 0x800
        for w in range(16):
            sub_ptr = 0x10030020 + blk * 0x400 + w * 0x20
            struct.pack_into("<I", buf, base_off + w * 4, sub_ptr)
        for i in range(0x40, 0x800):
            buf[base_off + i] = ((i * 13 + blk * 29 + 7) % 120) + 2
    # Populate sub-blocks (+0x2000..+0x3FFF) with realistic non-zero payload bytes
    for i in range(0x2000, 0x4000):
        buf[i] = ((i * 17 + 0x23) % 250) + 2
    # Ensure NUL terminators every 64 bytes in +0x3800..+0x3FFF for C-string safety
    for i in range(0x383F, 0x4000, 64):
        buf[i] = 0
    return bytes(buf)


DEFAULT_ARENA_BLOB = _make_default_arena_blob()


def _apply_declarative_setup(m: ArmDiffMachine, vspec: dict[str, Any]) -> None:
    arena = vspec.get("arena")
    if arena == "default":
        m.write_bytes(SCRATCH, DEFAULT_ARENA_BLOB)
    elif arena == "alt":
        # Variant arena with distinct values & high bits set
        alt = bytes((b ^ 0x5A) for b in DEFAULT_ARENA_BLOB[:0x2000]) + DEFAULT_ARENA_BLOB[0x2000:]
        m.write_bytes(SCRATCH, alt)
        # Re-restore valid nested pointers at +0x0000..+0x183F
        for blk in range(4):
            base = SCRATCH + blk * 0x800
            for w in range(16):
                m.write_u32(base + w * 4, 0x10030220 + blk * 0x400 + w * 0x20)

    for addr_raw, hex_s in vspec.get("mem_hex", ()):
        addr = int(addr_raw, 16) if isinstance(addr_raw, str) else int(addr_raw)
        m.write_bytes(addr, bytes.fromhex(hex_s))
    for addr_raw, val in vspec.get("mem_u32", ()):
        addr = int(addr_raw, 16) if isinstance(addr_raw, str) else int(addr_raw)
        m.write_u32(addr, int(val))
    for addr_raw, val in vspec.get("mem_u16", ()):
        addr = int(addr_raw, 16) if isinstance(addr_raw, str) else int(addr_raw)
        m.write_u16(addr, int(val))
    for addr_raw, val in vspec.get("mem_u8", ()):
        addr = int(addr_raw, 16) if isinstance(addr_raw, str) else int(addr_raw)
        m.write_u8(addr, int(val))
    for addr_raw, val in vspec.get("mmio_u32", ()):
        addr = int(addr_raw, 16) if isinstance(addr_raw, str) else int(addr_raw)
        m.write_mmio_u32(addr, int(val))
    for addr_raw, plan_map in vspec.get("mmio_plan", {}).items():
        addr = int(addr_raw, 16) if isinstance(addr_raw, str) else int(addr_raw)
        m.read_plan[addr] = {int(k): int(v) for k, v in plan_map.items()}
    if vspec.get("w1c_4d154c64"):
        m.w1c_4d154c64 = True


def load_p2_specs(p2_path: Path | None) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    if p2_path is None or not p2_path.is_file():
        return [], []
    doc = json.loads(p2_path.read_text(encoding="utf-8"))
    extra_stubs = list(doc.get("extra_pending_stubs", []))
    cases: list[dict[str, Any]] = []
    for fentry in doc.get("functions", []):
        c_sym = fentry["c_symbol"]
        suite = fentry.get("suite", "p2_reconstructed")
        check_r0 = bool(fentry.get("check_r0", True))
        check_r1 = bool(fentry.get("check_r1", False))
        stub_pending = bool(fentry.get("stub_pending", False))
        stop_at_halt = bool(fentry.get("stop_at_halt", False))
        for idx, vec in enumerate(fentry.get("vectors", [])):
            vid = vec.get("id", f"v{idx}")
            watch_list = [
                (int(a, 16) if isinstance(a, str) else int(a), int(ln))
                for a, ln in vec.get("watch", ())
            ]
            cases.append({
                "suite": suite,
                "case_id": f"{c_sym}_{vid}",
                "c_symbol": c_sym,
                "args": [int(x, 16) if isinstance(x, str) else int(x) for x in vec.get("args", ())],
                "setup": None,
                "decl_setup": vec,
                "watch": watch_list,
                "auto_mem_diff": bool(vec.get("auto_mem_diff", True)),
                "check_r0": bool(vec.get("check_r0", check_r0)),
                "check_r1": bool(vec.get("check_r1", check_r1)),
                "primask": int(vec.get("primask", 0)),
                "basepri": int(vec.get("basepri", 0)),
                "ipsr": int(vec.get("ipsr", 0)),
                "sp": int(vec.get("sp", STACK_TOP)),
                "stub_pending": bool(vec.get("stub_pending", stub_pending)),
                "stop_at_halt": bool(vec.get("stop_at_halt", stop_at_halt)),
            })
    return extra_stubs, cases


# Worker-local state for multi-core ProcessPoolExecutor
_W_RAW_BYTES: bytes = b""
_W_RAW_M: ArmDiffMachine | None = None
_W_ELF_M: ArmDiffMachine | None = None
_W_MD: cs.Cs | None = None
_W_SYM_TO_RVA: dict[str, int] = {}
_W_SYM_TO_SIZE: dict[str, int] = {}
_W_SPECS_BY_SYM: dict[str, list[dict[str, Any]]] = {}


def _init_worker(
    elf_path: str,
    raw_path: str,
    ledger_path: str,
    p2_path_str: str,
) -> None:
    global _W_RAW_BYTES, _W_RAW_M, _W_ELF_M, _W_MD
    global _W_SYM_TO_RVA, _W_SYM_TO_SIZE, _W_SPECS_BY_SYM

    _W_RAW_BYTES = Path(raw_path).read_bytes()
    elf_bytes = Path(elf_path).read_bytes()
    ledger = json.loads(Path(ledger_path).read_text(encoding="utf-8"))

    _W_SYM_TO_RVA = {}
    _W_SYM_TO_SIZE = {}
    for fn in ledger["functions"]:
        if fn["status"] == "reconstructed_c" and "c_symbol" in fn:
            _W_SYM_TO_RVA[fn["c_symbol"]] = int(fn["rva"], 16)
            _W_SYM_TO_SIZE[fn["c_symbol"]] = int(fn["size"])

    p2_path = Path(p2_path_str) if p2_path_str else None
    extra_stubs, p2_cases = load_p2_specs(p2_path)
    all_cases = build_cases() + p2_cases

    _W_SPECS_BY_SYM = {}
    for sp in all_cases:
        _W_SPECS_BY_SYM.setdefault(sp["c_symbol"], []).append(sp)

    _W_RAW_M = ArmDiffMachine(
        is_raw=True, raw_bytes=_W_RAW_BYTES, elf_bytes=elf_bytes, extra_stubs=extra_stubs
    )
    _W_ELF_M = ArmDiffMachine(
        is_raw=False, raw_bytes=_W_RAW_BYTES, elf_bytes=elf_bytes, extra_stubs=extra_stubs
    )
    _W_MD = cs.Cs(cs.CS_ARCH_ARM, cs.CS_MODE_THUMB | cs.CS_MODE_MCLASS)


def _prepare_machine(m: ArmDiffMachine, spec: dict[str, Any], c_sym: str, *, is_raw: bool) -> None:
    m.reset_state()
    if spec["stub_pending"]:
        if is_raw:
            m.install_pending_link_stubs()
        m.track_stub_calls = True
        if c_sym not in ("acib_panic_null", "acib_panic_code_line", "acib_panic_code_group", "rtk_abort_panic"):
            m.panic_null_varargs = 1
    if spec.get("setup") is not None:
        spec["setup"](m)
    if spec.get("decl_setup") is not None:
        _apply_declarative_setup(m, spec["decl_setup"])


def _run_symbol_batch(sym_batch: list[str]) -> dict[str, Any]:
    assert _W_RAW_M is not None and _W_ELF_M is not None and _W_MD is not None
    raw_m = _W_RAW_M
    elf_m = _W_ELF_M
    md = _W_MD

    stubs = (
        ("bx_lr", bytes.fromhex("7047")),
        ("ret_0", bytes.fromhex("00207047")),
        ("ret_1", bytes.fromhex("01207047")),
    )

    batch_results: list[dict[str, Any]] = []
    batch_survivors: list[dict[str, str]] = []
    batch_identities: list[dict[str, str]] = []
    batch_trials = 0
    batch_divergences = 0

    t_lo, t_hi = elf_m.text_bounds

    for c_sym in sym_batch:
        specs = _W_SPECS_BY_SYM.get(c_sym, [])
        if not specs:
            continue
        if c_sym not in _W_SYM_TO_RVA:
            raise KeyError(f"{c_sym} not marked reconstructed_c in ledger.json")
        if c_sym not in elf_m.symbols:
            raise KeyError(f"{c_sym} missing from acib.elf symbol table")

        raw_rva = _W_SYM_TO_RVA[c_sym]
        elf_vma = elf_m.symbols[c_sym] & ~1

        # Enforce that reconstructed_c executes compiled ELF .text (or pinned SysTick_Handler)
        in_valid_elf_code = (t_lo <= elf_vma < t_hi) or (c_sym == "SysTick_Handler" and elf_vma == 0x000E58F0)

        raw_insns = list(md.disasm(_W_RAW_BYTES[raw_rva : raw_rva + 8], raw_rva, count=1))
        elf_insns = list(md.disasm(elf_m.read_bytes(elf_vma, 8), elf_vma, count=1))
        disasm_ok = len(raw_insns) == 1 and len(elf_insns) == 1 and in_valid_elf_code

        sym_exec_records: list[tuple[dict[str, Any], dict[str, Any], list[bytes], list[bytes] | None]] = []

        for spec in specs:
            suite = spec["suite"]
            case_id = spec["case_id"]
            auto_mem = bool(spec.get("auto_mem_diff", False))

            _prepare_machine(raw_m, spec, c_sym, is_raw=True)
            _prepare_machine(elf_m, spec, c_sym, is_raw=False)

            raw_until = 0x06CFF4 if spec["stop_at_halt"] else STOP_PC
            elf_until = (elf_m.symbols["sub_6CFF4"] & ~1) if spec["stop_at_halt"] else STOP_PC

            raw_out = raw_m.run_call(
                raw_rva,
                spec["args"],
                primask=spec["primask"],
                basepri=spec["basepri"],
                ipsr=spec["ipsr"],
                sp=spec["sp"],
                until_pc=raw_until,
            )
            elf_out = elf_m.run_call(
                elf_vma,
                spec["args"],
                primask=spec["primask"],
                basepri=spec["basepri"],
                ipsr=spec["ipsr"],
                sp=spec["sp"],
                until_pc=elf_until,
            )

            mismatches: list[str] = []
            if not disasm_ok:
                mismatches.append("capstone_or_elf_text_vma_failed")
            if not raw_out["callee_saved_ok"] or not elf_out["callee_saved_ok"]:
                mismatches.append(
                    f"callee_saved_or_sp raw_ok={raw_out['callee_saved_ok']} elf_ok={elf_out['callee_saved_ok']}"
                )
            if spec["check_r0"] and raw_out["r0"] != elf_out["r0"]:
                mismatches.append(f"r0 raw=0x{raw_out['r0']:08x} elf=0x{elf_out['r0']:08x}")
            if spec["check_r1"] and raw_out["r1"] != elf_out["r1"]:
                mismatches.append(f"r1 raw=0x{raw_out['r1']:08x} elf=0x{elf_out['r1']:08x}")
            if raw_out["primask"] != elf_out["primask"]:
                mismatches.append(f"primask raw={raw_out['primask']} elf={elf_out['primask']}")
            if raw_out["basepri"] != elf_out["basepri"]:
                mismatches.append(f"basepri raw=0x{raw_out['basepri']:x} elf=0x{elf_out['basepri']:x}")
            if raw_out["transactions"] != elf_out["transactions"]:
                mismatches.append(
                    f"mmio_tx raw({len(raw_out['transactions'])})={raw_out['transactions'][:6]} "
                    f"!= elf({len(elf_out['transactions'])})={elf_out['transactions'][:6]}"
                )
            if spec["stub_pending"] and raw_out["stub_calls"] != elf_out["stub_calls"]:
                mismatches.append(
                    f"stub_calls raw({len(raw_out['stub_calls'])})={raw_out['stub_calls']} "
                    f"!= elf({len(elf_out['stub_calls'])})={elf_out['stub_calls']}"
                )

            raw_watch_blobs: list[bytes] = []
            for w_addr, w_len in spec["watch"]:
                rb = raw_m.read_bytes(w_addr, w_len)
                eb = elf_m.read_bytes(w_addr, w_len)
                raw_watch_blobs.append(rb)
                if rb != eb:
                    mismatches.append(
                        f"mem[0x{w_addr:08x}+0x{w_len:x}] raw={rb.hex()[:32]} != elf={eb.hex()[:32]}"
                    )

            raw_auto_blobs: list[bytes] | None = None
            if auto_mem:
                raw_auto_blobs = raw_m.snapshot_auto_regions()
                elf_auto_blobs = elf_m.snapshot_auto_regions()
                for (r_base, _r_len), rb, eb in zip(AUTO_DIFF_REGIONS, raw_auto_blobs, elf_auto_blobs):
                    if rb != eb:
                        # Pinpoint first differing byte offset
                        diff_off = next(i for i, (x, y) in enumerate(zip(rb, eb)) if x != y)
                        d_addr = r_base + diff_off
                        mismatches.append(
                            f"auto_mem[0x{d_addr:08x}] raw={rb[diff_off:diff_off+8].hex()} "
                            f"!= elf={eb[diff_off:diff_off+8].hex()}"
                        )
                        break

            sym_exec_records.append((spec, raw_out, raw_watch_blobs, raw_auto_blobs))

            ok = len(mismatches) == 0
            if not ok:
                batch_divergences += 1

            batch_results.append({
                "suite": suite,
                "case_id": case_id,
                "c_symbol": c_sym,
                "raw_rva": hex(raw_rva),
                "elf_vma": hex(elf_vma),
                "mmio_transactions": len(raw_out["transactions"]),
                "stub_calls": len(raw_out["stub_calls"]),
                "watched_regions": len(spec["watch"]) + (len(AUTO_DIFF_REGIONS) if auto_mem else 0),
                "passed": ok,
                "mismatches": mismatches,
            })

        # 3-stub mutation audit for c_sym
        raw_fn_bytes = _W_RAW_BYTES[raw_rva : raw_rva + _W_SYM_TO_SIZE[c_sym]]
        for stub_name, stub_bytes in stubs:
            if raw_fn_bytes == stub_bytes:
                batch_identities.append({
                    "c_symbol": c_sym,
                    "raw_rva": hex(raw_rva),
                    "raw_hex": raw_fn_bytes.hex(),
                    "stub": stub_name,
                })
                continue
            batch_trials += 1
            killed = False
            orig_elf_code = elf_m.read_bytes(elf_vma, len(stub_bytes))
            try:
                for spec, raw_out, raw_watch_blobs, raw_auto_blobs in sym_exec_records:
                    _prepare_machine(elf_m, spec, c_sym, is_raw=False)
                    elf_m.cpu.mem_write(elf_vma, stub_bytes)
                    elf_m.cpu.ctl_remove_cache(0, 0x00100000)
                    elf_until = (elf_m.symbols["sub_6CFF4"] & ~1) if spec["stop_at_halt"] else STOP_PC
                    try:
                        mut_out = elf_m.run_call(
                            elf_vma,
                            spec["args"],
                            primask=spec["primask"],
                            basepri=spec["basepri"],
                            ipsr=spec["ipsr"],
                            sp=spec["sp"],
                            until_pc=elf_until,
                            limit=25000,
                        )
                    except (RuntimeError, UcError):
                        killed = True
                        break

                    if (
                        (spec["check_r0"] and mut_out["r0"] != raw_out["r0"])
                        or (spec["check_r1"] and mut_out["r1"] != raw_out["r1"])
                        or mut_out["primask"] != raw_out["primask"]
                        or mut_out["basepri"] != raw_out["basepri"]
                        or mut_out["transactions"] != raw_out["transactions"]
                        or (spec["stub_pending"] and mut_out["stub_calls"] != raw_out["stub_calls"])
                    ):
                        killed = True
                        break

                    for (w_addr, w_len), rb in zip(spec["watch"], raw_watch_blobs):
                        if elf_m.read_bytes(w_addr, w_len) != rb:
                            killed = True
                            break
                    if killed:
                        break

                    if raw_auto_blobs is not None:
                        mut_auto = elf_m.snapshot_auto_regions()
                        if mut_auto != raw_auto_blobs:
                            killed = True
                            break
            finally:
                elf_m.cpu.mem_write(elf_vma, orig_elf_code)
                elf_m.cpu.ctl_remove_cache(0, 0x00100000)

            if not killed:
                batch_survivors.append({
                    "c_symbol": c_sym,
                    "raw_rva": hex(raw_rva),
                    "stub": stub_name,
                })

    return {
        "results": batch_results,
        "survivors": batch_survivors,
        "identities": batch_identities,
        "trials": batch_trials,
        "divergences": batch_divergences,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--ledger", type=Path, required=True)
    parser.add_argument("--p2-vectors", type=Path, default=None)
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 8, 12))
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args()

    raw_bytes = args.raw.read_bytes()
    raw_sha = hashlib.sha256(raw_bytes).hexdigest()
    if raw_sha != EXPECTED_RAW_SHA256:
        print(f"ERROR: raw acib SHA-256 mismatch: {raw_sha}", file=sys.stderr)
        return 1

    ledger = json.loads(args.ledger.read_text(encoding="utf-8"))
    sym_to_rva: dict[str, int] = {}
    for fn in ledger["functions"]:
        if fn["status"] == "reconstructed_c" and "c_symbol" in fn:
            sym_to_rva[fn["c_symbol"]] = int(fn["rva"], 16)

    p2_path = args.p2_vectors
    if p2_path is None:
        default_p2 = args.ledger.parent / "p2_vectors.json"
        if default_p2.is_file():
            p2_path = default_p2

    extra_stubs, p2_cases = load_p2_specs(p2_path)
    all_cases = build_cases() + p2_cases

    stub_rvas = {rva & ~1 for rva, _, _ in PENDING_STUB_SPECS} | {
        (int(es["rva"], 16) if isinstance(es["rva"], str) else int(es["rva"])) & ~1
        for es in extra_stubs
    }
    overlap_stubs = sorted(
        f"{csym}@0x{rva:06x}" for csym, rva in sym_to_rva.items() if (rva & ~1) in stub_rvas
    )
    if overlap_stubs:
        print(
            f"ERROR: {len(overlap_stubs)} reconstructed_c functions overlap PENDING_STUB_SPECS: {overlap_stubs}",
            file=sys.stderr,
        )
        return 1

    exercised_syms: set[str] = set()
    ordered_syms: list[str] = []
    for sp in all_cases:
        csym = sp["c_symbol"]
        if csym not in sym_to_rva:
            raise KeyError(f"{csym} not marked reconstructed_c in ledger.json")
        if csym not in exercised_syms:
            exercised_syms.add(csym)
            ordered_syms.append(csym)

    ordered_syms.sort(key=lambda s: sym_to_rva[s])

    untested_syms = sorted(set(sym_to_rva) - exercised_syms)
    if untested_syms:
        print(
            f"ERROR: {len(untested_syms)} reconstructed_c functions lack Unicorn coverage: {untested_syms[:20]}",
            file=sys.stderr,
        )

    n_workers = max(1, min(args.jobs, len(ordered_syms)))
    # Partition ordered_syms into ~3x n_workers chunks for balanced load
    n_chunks = max(1, min(len(ordered_syms), n_workers * 3))
    chunks: list[list[str]] = [[] for _ in range(n_chunks)]
    for idx, s in enumerate(ordered_syms):
        chunks[idx % n_chunks].append(s)
    chunks = [c for c in chunks if c]

    results: list[dict[str, Any]] = []
    stub_survivors: list[dict[str, str]] = []
    literal_raw_stub_identities: list[dict[str, str]] = []
    stub_trials = 0
    divergences = 0

    elf_str = str(args.elf.resolve())
    raw_str = str(args.raw.resolve())
    led_str = str(args.ledger.resolve())
    p2_str = str(p2_path.resolve()) if p2_path is not None and p2_path.is_file() else ""

    if n_workers == 1 or len(ordered_syms) <= 20:
        _init_worker(elf_str, raw_str, led_str, p2_str)
        batch_outs = [_run_symbol_batch(ch) for ch in chunks]
    else:
        with ProcessPoolExecutor(
            max_workers=n_workers,
            initializer=_init_worker,
            initargs=(elf_str, raw_str, led_str, p2_str),
        ) as pool:
            batch_outs = list(pool.map(_run_symbol_batch, chunks))

    for bo in batch_outs:
        results.extend(bo["results"])
        stub_survivors.extend(bo["survivors"])
        literal_raw_stub_identities.extend(bo["identities"])
        stub_trials += bo["trials"]
        divergences += bo["divergences"]

    results.sort(key=lambda r: (int(r["raw_rva"], 16), r["case_id"]))
    stub_survivors.sort(key=lambda r: (int(r["raw_rva"], 16), r["stub"]))
    literal_raw_stub_identities.sort(key=lambda r: (int(r["raw_rva"], 16), r["stub"]))

    suite_stats: dict[str, dict[str, int]] = {}
    for res in results:
        st = suite_stats.setdefault(res["suite"], {"cases": 0, "divergences": 0})
        st["cases"] += 1
        if not res["passed"]:
            st["divergences"] += 1
            print(
                f"DIVERGENCE [{res['suite']}/{res['case_id']}] ({res['c_symbol']} @ {res['raw_rva']}): {res['mismatches']}",
                file=sys.stderr,
            )

    for surv in stub_survivors[:30]:
        print(
            f"STUB SURVIVOR: {surv['c_symbol']} @ {surv['raw_rva']} survived {surv['stub']}",
            file=sys.stderr,
        )

    passed_all = (
        divergences == 0
        and len(results) > 0
        and len(untested_syms) == 0
        and len(stub_survivors) == 0
    )

    report = {
        "target": "acib",
        "engine": "unicorn-armv7em-cortex-m4",
        "workers": n_workers,
        "case_count": len(results),
        "functions_exercised": len(exercised_syms),
        "ledger_reconstructed_c": len(sym_to_rva),
        "untested_reconstructed_c": untested_syms,
        "divergences": divergences,
        "stub_mutation_audit": {
            "stubs": ["bx_lr (7047)", "ret_0 (00207047)", "ret_1 (01207047)"],
            "trials": stub_trials,
            "literal_raw_stub_identities": literal_raw_stub_identities,
            "survivors_count": len(stub_survivors),
            "survivors": stub_survivors,
        },
        "suites": suite_stats,
        "passed": passed_all,
        "cases": results,
    }

    out_path = args.output or (args.elf.parent / "arm-semantics.json")
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if not passed_all:
        print(
            f"arm_semantics: FAIL (divergences={divergences}, untested={len(untested_syms)}, "
            f"stub_survivors={len(stub_survivors)}; see {out_path})",
            file=sys.stderr,
        )
        return 1

    print(
        f"arm_semantics: PASS ({len(results)}/{len(results)} Unicorn ARMv7E-M differential cases, "
        f"{len(exercised_syms)}/{len(sym_to_rva)} reconstructed C functions, 0 divergences, "
        f"0/{stub_trials} stub-mutation survivors, {n_workers} workers)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
