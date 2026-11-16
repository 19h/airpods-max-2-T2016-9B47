#!/usr/bin/env python3
"""
Unicorn Cortex-M4F (ARMv7E-M + VFPv4-SP-D16) differential execution harness for rkof.

Executes 100% (108/108) of reconstructed C11 functions compiled into `rkof.elf`
side-by-side against the original Apple Thumb-2 machine code in `../../../rkof`,
verifying exact architectural equivalence across:
  - Return registers (r0, r1, VFPv4 s0 / d0)
  - Callee-saved register preservation (r4..r11)
  - ARMv7-M special registers (BASEPRI, PRIMASK)
  - Firmware SRAM globals, scratch buffers, SoC MMIO apertures, ARM SCS, and
    W3 PHY shared mailbox / trace / crash / NV calibration / warm-restore SRAM
  - External boundary call traces (exact stub invocation order, integer args,
    and decoded C-string literals)
"""

from __future__ import annotations

import argparse
from concurrent.futures import ProcessPoolExecutor
import json
import os
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

import unicorn as uc
from elftools.elf.elffile import ELFFile
from unicorn import arm_const as ac

ROM_BASE = 0x00000000
ROM_SIZE = 0x00200000
SCRATCH_RTSI_PAGE = 0x00800000
SCRATCH_RTSI_SIZE = 0x00001000
SRAM_BASE = 0x20000000
SRAM_SIZE = 0x00100000
TEST_RAM_BASE = 0x20100000
TEST_RAM_SIZE = 0x00100000
REMAP_BASE = 0x4000C000
REMAP_SIZE = 0x00001000
UART_BASE = 0x42019000
UART_SIZE = 0x00001000
PMGR_BASE = 0x42440000
PMGR_SIZE = 0x00040000
W3_ALL_BASE = 0x4D000000
W3_ALL_SIZE = 0x00300000
SCS_BASE = 0xE000E000
SCS_SIZE = 0x00002000

INITIAL_SP = 0x2010F000
STOP_ADDR = 0x2010FE00
STACK_LOCAL_LO = INITIAL_SP - 0x4000
STACK_LOCAL_HI = INITIAL_SP + 0x0100
MMIO_DYN_LO = 0x40000000
MMIO_DYN_HI = 0x60000000
ZERO_PAGE = b"\x00" * 0x1000


def f32_bits(val: float) -> int:
    return struct.unpack("<I", struct.pack("<f", val))[0]


def f64_bits(val: float) -> int:
    return struct.unpack("<Q", struct.pack("<d", val))[0]


def pack_u32(*words: int) -> bytes:
    return b"".join(struct.pack("<I", w & 0xFFFFFFFF) for w in words)


def read_guest_cstr(mu: uc.Uc, addr: int, max_len: int = 128) -> str:
    if addr == 0:
        return "<NULL>"
    raw = bytes(mu.mem_read(addr, max_len))
    nul = raw.find(0)
    if nul >= 0:
        raw = raw[:nul]
    return raw.decode("latin-1")


@dataclass
class StubHook:
    name: str
    ref_addr: int
    elf_sym: str = ""
    ret_r0: int = 0
    record_args: int = 2
    cstr_args: tuple[int, ...] = ()
    stop_on_call: bool = False
    on_call: Callable[[uc.Uc, list[tuple[Any, ...]]], int | None] | None = None


@dataclass
class DiffCase:
    suite: str
    name: str
    symbol: str
    ref_rva: int
    r0: int = 0
    r1: int = 0
    r2: int = 0
    r3: int = 0
    s0_bits: int | None = None
    d0_bits: int | None = None
    s_regs: dict[int, int] = field(default_factory=dict)
    d_regs: dict[int, int] = field(default_factory=dict)
    primask: int = 0
    basepri: int = 0
    stop_pc: int | None = None
    stop_on_wfi: bool = False
    max_insns: int = 200000
    check_r0: bool = True
    check_r1: bool = False
    check_s0: bool = False
    check_d0: bool = False
    check_s_regs: tuple[int, ...] = ()
    check_d_regs: tuple[int, ...] = ()
    check_special_regs: bool = False
    check_all_writes: bool = False
    check_mmio_trace: bool = False
    mem_writes: list[tuple[int, bytes]] = field(default_factory=list)
    compare_ranges: list[tuple[int, int]] = field(default_factory=list)
    stubs: list[StubHook] = field(default_factory=list)
    stack_args: list[int] = field(default_factory=list)
    mmio_read_seq: dict[int, list[int]] = field(default_factory=dict)


def load_elf_segments_and_symbols(
    elf_path: Path,
) -> tuple[list[tuple[int, bytes]], dict[str, int]]:
    segments: list[tuple[int, bytes]] = []
    symbols: dict[str, int] = {}
    with elf_path.open("rb") as fh:
        elf = ELFFile(fh)
        for seg in elf.iter_segments():
            if seg["p_type"] == "PT_LOAD" and int(seg["p_filesz"]) > 0:
                segments.append((int(seg["p_vaddr"]), seg.data()))
        symtab = elf.get_section_by_name(".symtab")
        if symtab is None:
            raise RuntimeError(f"Missing .symtab in {elf_path}")
        for sym in symtab.iter_symbols():
            if sym.name:
                symbols[sym.name] = int(sym["st_value"]) & ~1
    return segments, symbols


class FastVM:
    """
    Reusable Cortex-M4F Unicorn VM with exact chunk-diff rollback, dynamic SoC
    MMIO aperture mapping (0x40000000..0x5FFFFFFF), full non-stack SRAM/TEST_RAM
    and MMIO state comparison, and optional MMIO R/W trace recording.

    Note: avoids attaching UC_HOOK_MEM_WRITE to SRAM/ROM so Unicorn 2.x never
    splits Thumb-2 IT blocks mid-execution on predicated stores (`strbne`, etc.).

    Integrity invariant: when `is_elf=True`, ONLY `rkof.elf` PT_LOAD segments
    are loaded into ROM/SRAM — raw `rkof` code is NEVER mapped into the ELF VM.
    """

    def __init__(
        self,
        is_elf: bool,
        raw: bytes,
        elf_segs: list[tuple[int, bytes]],
        elf_syms: dict[str, int],
    ) -> None:
        self.is_elf = is_elf
        self.elf_syms = elf_syms
        self.mu = uc.Uc(uc.UC_ARCH_ARM, uc.UC_MODE_THUMB | uc.UC_MODE_MCLASS)

        self.mapped_4k: set[int] = set()
        self.dyn_mmio_64k: list[int] = []
        self.ctx: dict[str, Any] = {
            "active": False,
            "check_mmio_trace": False,
            "mmio_trace": [],
            "mmio_seq": {},
            "mmio_idx": {},
        }

        self._map_region(ROM_BASE, ROM_SIZE, uc.UC_PROT_READ | uc.UC_PROT_EXEC)
        self.writable_spans: list[tuple[int, int]] = [
            (SCRATCH_RTSI_PAGE, SCRATCH_RTSI_SIZE),
            (SRAM_BASE, 0x000E0000),
            (TEST_RAM_BASE, 0x00020000),
            (REMAP_BASE, REMAP_SIZE),
            (UART_BASE, UART_SIZE),
            (PMGR_BASE, PMGR_SIZE),
            (W3_ALL_BASE, W3_ALL_SIZE),
            (SCS_BASE, SCS_SIZE),
        ]
        self._map_region(SCRATCH_RTSI_PAGE, SCRATCH_RTSI_SIZE, uc.UC_PROT_ALL)
        self._map_region(SRAM_BASE, SRAM_SIZE, uc.UC_PROT_ALL)
        self._map_region(TEST_RAM_BASE, TEST_RAM_SIZE, uc.UC_PROT_ALL)
        self._map_region(REMAP_BASE, REMAP_SIZE, uc.UC_PROT_ALL)
        self._map_region(UART_BASE, UART_SIZE, uc.UC_PROT_ALL)
        self._map_region(PMGR_BASE, PMGR_SIZE, uc.UC_PROT_ALL)
        self._map_region(W3_ALL_BASE, W3_ALL_SIZE, uc.UC_PROT_ALL)
        self._map_region(SCS_BASE, SCS_SIZE, uc.UC_PROT_ALL)

        if is_elf:
            for vaddr, data in elf_segs:
                self.mu.mem_write(vaddr, data)
        else:
            self.mu.mem_write(ROM_BASE, raw[:0x1F389A])
            self.mu.mem_write(SRAM_BASE, raw[0x200000 : 0x200000 + 0xF138])

        self.mu.mem_write(0xE000ED88, pack_u32(0x00F00000))
        self.mu.mem_write(STOP_ADDR, bytes.fromhex("00bf00bf"))

        if hasattr(ac, "UC_ARM_REG_FPEXC"):
            try:
                self.mu.reg_write(ac.UC_ARM_REG_FPEXC, 0x40000000)
            except uc.UcError:
                pass

        # Snapshot clean baseline chunks (up to 64KB each) for fast memcmp rollback
        self.base_chunks: list[tuple[int, int, bytes]] = []
        self.base_chunk_map: dict[int, bytes] = {}
        for base, size in self.writable_spans:
            off = 0
            while off < size:
                step = min(0x10000, size - off)
                addr = base + off
                cb = bytes(self.mu.mem_read(addr, step))
                self.base_chunks.append((addr, step, cb))
                self.base_chunk_map[addr] = cb
                off += step

        self.rom_patches: list[tuple[int, bytes]] = []

        self.mu.hook_add(
            uc.UC_HOOK_MEM_READ_UNMAPPED | uc.UC_HOOK_MEM_WRITE_UNMAPPED,
            self._on_unmapped,
        )

    def _map_region(self, base: int, size: int, perms: int) -> None:
        self.mu.mem_map(base, size, perms)
        for p in range(base, base + size, 0x1000):
            self.mapped_4k.add(p)

    def _ensure_mmio_mapped(self, addr: int) -> bool:
        if not (MMIO_DYN_LO <= addr < MMIO_DYN_HI):
            return False
        base64 = addr & ~0xFFFF
        any_new = False
        p = base64
        end = base64 + 0x10000
        while p < end:
            if p not in self.mapped_4k:
                any_new = True
                run_start = p
                while p < end and p not in self.mapped_4k:
                    self.mapped_4k.add(p)
                    p += 0x1000
                self.mu.mem_map(run_start, p - run_start, uc.UC_PROT_ALL)
                zb = b"\x00" * (p - run_start)
                self.base_chunks.append((run_start, p - run_start, zb))
                self.base_chunk_map[run_start] = zb
            else:
                p += 0x1000
        if any_new:
            self.dyn_mmio_64k.append(base64)
        return True

    def _on_unmapped(
        self, _uc_obj: uc.Uc, _access: int, address: int, _size: int, _value: int, _user: Any
    ) -> bool:
        return self._ensure_mmio_mapped(address)

    def _on_mmio_write(
        self, _uc_obj: uc.Uc, _access: int, address: int, size: int, value: int, _user: Any
    ) -> None:
        ctx = self.ctx
        if ctx["active"] and ctx["check_mmio_trace"]:
            mask = (1 << (size * 8)) - 1
            ctx["mmio_trace"].append(("W", address, size, value & mask))

    def _on_mmio_read(
        self, uc_obj: uc.Uc, _access: int, address: int, size: int, _value: int, _user: Any
    ) -> None:
        ctx = self.ctx
        if not ctx["active"]:
            return
        if ctx["check_mmio_trace"]:
            raw_b = bytes(uc_obj.mem_read(address, size))
            val = int.from_bytes(raw_b, "little")
            ctx["mmio_trace"].append(("R", address, size, val))
        seq = ctx["mmio_seq"].get(address)
        if seq is not None:
            nxt_idx = ctx["mmio_idx"].get(address, 0) + 1
            ctx["mmio_idx"][address] = nxt_idx
            nxt_val = seq[nxt_idx] if nxt_idx < len(seq) else seq[-1]
            uc_obj.mem_write(address, pack_u32(nxt_val))

    def _write_host(self, addr: int, payload: bytes) -> None:
        if not payload:
            return
        if MMIO_DYN_LO <= addr < MMIO_DYN_HI:
            self._ensure_mmio_mapped(addr)
        if addr < ROM_SIZE:
            orig = bytes(self.mu.mem_read(addr, len(payload)))
            self.rom_patches.append((addr, orig))
        self.mu.mem_write(addr, payload)
        if addr < ROM_SIZE:
            self.mu.ctl_flush_tb()

    def rollback(self) -> None:
        mu = self.mu
        had_rom_patch = bool(self.rom_patches)
        for addr, orig in reversed(self.rom_patches):
            mu.mem_write(addr, orig)
        self.rom_patches.clear()
        if had_rom_patch:
            mu.ctl_flush_tb()
        for c_addr, c_len, c_base in self.base_chunks:
            if bytes(mu.mem_read(c_addr, c_len)) != c_base:
                mu.mem_write(c_addr, c_base)

    def run_case(
        self,
        case: DiffCase,
        patch_bytes: tuple[int, bytes] | None = None,
    ) -> dict[str, Any]:
        mu = self.mu
        code_hook_handle = None
        mmio_r_handle = None
        mmio_w_handle = None
        try:
            for addr, payload in case.mem_writes:
                self._write_host(addr, payload)

            for mm_addr, seq in case.mmio_read_seq.items():
                if seq:
                    self._write_host(mm_addr, pack_u32(seq[0]))

            if patch_bytes is not None:
                self._write_host(patch_bytes[0], patch_bytes[1])

            sp = INITIAL_SP
            if case.stack_args:
                stack_blob = pack_u32(*case.stack_args)
                sp -= (len(stack_blob) + 7) & ~7
                self._write_host(sp, stack_blob)

            callees = {
                ac.UC_ARM_REG_R4: 0x44440004,
                ac.UC_ARM_REG_R5: 0x55550005,
                ac.UC_ARM_REG_R6: 0x66660006,
                ac.UC_ARM_REG_R7: 0x77770007,
                ac.UC_ARM_REG_R8: 0x88880008,
                ac.UC_ARM_REG_R9: 0x99990009,
                ac.UC_ARM_REG_R10: 0xAAAA000A,
                ac.UC_ARM_REG_R11: 0xBBBB000B,
            }
            for reg_id, sentinel in callees.items():
                mu.reg_write(reg_id, sentinel)

            mu.reg_write(ac.UC_ARM_REG_R12, 0)
            mu.reg_write(ac.UC_ARM_REG_SP, sp)
            mu.reg_write(ac.UC_ARM_REG_MSP, sp)
            mu.reg_write(ac.UC_ARM_REG_PSP, 0)
            mu.reg_write(ac.UC_ARM_REG_LR, STOP_ADDR | 1)
            mu.reg_write(ac.UC_ARM_REG_XPSR, 0x01000000)
            mu.reg_write(ac.UC_ARM_REG_APSR, 0)
            mu.reg_write(ac.UC_ARM_REG_FPSCR, 0)
            mu.reg_write(ac.UC_ARM_REG_FAULTMASK, 0)
            mu.reg_write(ac.UC_ARM_REG_CONTROL, 0)
            mu.reg_write(ac.UC_ARM_REG_R0, case.r0 & 0xFFFFFFFF)
            mu.reg_write(ac.UC_ARM_REG_R1, case.r1 & 0xFFFFFFFF)
            mu.reg_write(ac.UC_ARM_REG_R2, case.r2 & 0xFFFFFFFF)
            mu.reg_write(ac.UC_ARM_REG_R3, case.r3 & 0xFFFFFFFF)
            mu.reg_write(ac.UC_ARM_REG_PRIMASK, case.primask)
            mu.reg_write(ac.UC_ARM_REG_BASEPRI, case.basepri)

            for s_idx in range(16):
                mu.reg_write(ac.UC_ARM_REG_S0 + s_idx, 0)
            for s_idx in range(16, 32):
                mu.reg_write(ac.UC_ARM_REG_S0 + s_idx, 0x3F800000 + s_idx)

            if case.s0_bits is not None:
                mu.reg_write(ac.UC_ARM_REG_S0, case.s0_bits & 0xFFFFFFFF)
            for s_idx, bits in case.s_regs.items():
                mu.reg_write(ac.UC_ARM_REG_S0 + int(s_idx), bits & 0xFFFFFFFF)
            if case.d0_bits is not None:
                mu.reg_write(ac.UC_ARM_REG_D0, case.d0_bits & 0xFFFFFFFFFFFFFFFF)
            for d_idx, bits in case.d_regs.items():
                mu.reg_write(ac.UC_ARM_REG_D0 + int(d_idx), bits & 0xFFFFFFFFFFFFFFFF)

            trace: list[tuple[Any, ...]] = []
            hook_map: dict[int, StubHook] = {}
            for stub in case.stubs:
                if self.is_elf and stub.elf_sym:
                    target_pc = self.elf_syms[stub.elf_sym] & ~1
                else:
                    target_pc = stub.ref_addr & ~1
                hook_map[target_pc] = stub

            stopped_by_hook = {"flag": False}

            if hook_map or case.stop_on_wfi:

                def _code_hook(
                    uc_obj: uc.Uc, address: int, _size: int, _user_data: Any
                ) -> None:
                    pc_even = address & ~1
                    if (
                        case.stop_on_wfi
                        and bytes(uc_obj.mem_read(pc_even, 2)) == b"\x30\xbf"
                    ):
                        trace.append(("wfi",))
                        stopped_by_hook["flag"] = True
                        uc_obj.emu_stop()
                        return

                    stub = hook_map.get(pc_even)
                    if stub is None:
                        return
                    arg_regs = [
                        uc_obj.reg_read(ac.UC_ARM_REG_R0),
                        uc_obj.reg_read(ac.UC_ARM_REG_R1),
                        uc_obj.reg_read(ac.UC_ARM_REG_R2),
                        uc_obj.reg_read(ac.UC_ARM_REG_R3),
                    ]
                    rec_items: list[Any] = [stub.name]
                    for idx in range(stub.record_args):
                        if idx in stub.cstr_args:
                            rec_items.append(read_guest_cstr(uc_obj, arg_regs[idx]))
                        else:
                            rec_items.append(arg_regs[idx])
                    trace.append(tuple(rec_items))

                    ret_val = stub.ret_r0
                    if stub.on_call is not None:
                        override = stub.on_call(uc_obj, trace)
                        if override is not None:
                            ret_val = override

                    if stub.stop_on_call:
                        stopped_by_hook["flag"] = True
                        uc_obj.emu_stop()
                        return

                    uc_obj.reg_write(ac.UC_ARM_REG_R0, ret_val & 0xFFFFFFFF)
                    lr_val = uc_obj.reg_read(ac.UC_ARM_REG_LR)
                    uc_obj.reg_write(ac.UC_ARM_REG_PC, lr_val | 1)

                code_hook_handle = mu.hook_add(uc.UC_HOOK_CODE, _code_hook)

            mmio_trace: list[tuple[Any, ...]] = []
            self.ctx = {
                "active": True,
                "check_mmio_trace": case.check_mmio_trace,
                "mmio_trace": mmio_trace,
                "mmio_seq": case.mmio_read_seq,
                "mmio_idx": {},
            }
            if case.check_mmio_trace or case.mmio_read_seq:
                mmio_r_handle = mu.hook_add(
                    uc.UC_HOOK_MEM_READ,
                    self._on_mmio_read,
                    begin=MMIO_DYN_LO,
                    end=MMIO_DYN_HI - 1,
                )
            if case.check_mmio_trace:
                mmio_w_handle = mu.hook_add(
                    uc.UC_HOOK_MEM_WRITE,
                    self._on_mmio_write,
                    begin=MMIO_DYN_LO,
                    end=MMIO_DYN_HI - 1,
                )

            start_pc = self.elf_syms[case.symbol] if self.is_elf else case.ref_rva
            until_pc = case.stop_pc if case.stop_pc is not None else STOP_ADDR

            mu.emu_start(start_pc | 1, until_pc, count=case.max_insns)
            self.ctx["active"] = False

            did_early_stop = case.stop_pc is not None or stopped_by_hook["flag"]
            callee_ok = all(
                mu.reg_read(reg_id) == sentinel for reg_id, sentinel in callees.items()
            ) and all(
                (mu.reg_read(ac.UC_ARM_REG_S0 + s_idx) & 0xFFFFFFFF)
                == (0x3F800000 + s_idx)
                for s_idx in range(16, 32)
            )

            s_out = {
                idx: (mu.reg_read(ac.UC_ARM_REG_S0 + idx) & 0xFFFFFFFF)
                for idx in case.check_s_regs
            }
            d_out = {
                idx: (mu.reg_read(ac.UC_ARM_REG_D0 + idx) & 0xFFFFFFFFFFFFFFFF)
                for idx in case.check_d_regs
            }

            # When check_all_writes is True, capture every modified chunk outside local stack
            wl_snap: dict[int, bytes] = {}
            if case.check_all_writes:
                # 1) TEST_RAM non-stack arena 0x20100000..0x2010B000 (44 KB)
                arena_b = bytes(mu.mem_read(0x20100000, STACK_LOCAL_LO - 0x20100000))
                wl_snap[0x20100000] = arena_b
                # 2) Any modified SRAM (.data/.bss) or MMIO/SCS chunk
                for c_addr, c_len, c_base in self.base_chunks:
                    if c_addr == 0x20100000 or c_addr == 0x20110000:
                        continue
                    cur_b = bytes(mu.mem_read(c_addr, c_len))
                    if cur_b != c_base:
                        wl_snap[c_addr] = cur_b

            out: dict[str, Any] = {
                "r0": mu.reg_read(ac.UC_ARM_REG_R0),
                "r1": mu.reg_read(ac.UC_ARM_REG_R1) if case.check_r1 else None,
                "s0": (
                    (mu.reg_read(ac.UC_ARM_REG_S0) & 0xFFFFFFFF)
                    if case.check_s0
                    else None
                ),
                "d0": (
                    mu.reg_read(ac.UC_ARM_REG_D0) & 0xFFFFFFFFFFFFFFFF
                    if case.check_d0
                    else None
                ),
                "s_regs": s_out,
                "d_regs": d_out,
                "primask": (
                    mu.reg_read(ac.UC_ARM_REG_PRIMASK)
                    if case.check_special_regs
                    else None
                ),
                "basepri": (
                    mu.reg_read(ac.UC_ARM_REG_BASEPRI)
                    if case.check_special_regs
                    else None
                ),
                "callee_preserved": callee_ok if not did_early_stop else True,
                "early_stop": did_early_stop,
                "reached_stop_pc": (
                    ((mu.reg_read(ac.UC_ARM_REG_PC) & ~1) == (case.stop_pc & ~1))
                    if case.stop_pc is not None
                    else True
                ),
                "trace": trace,
                "mem": [
                    bytes(mu.mem_read(addr, length))
                    for addr, length in case.compare_ranges
                ],
                "written_lines": wl_snap,
                "mmio_trace": mmio_trace if case.check_mmio_trace else [],
            }
            return out
        finally:
            self.ctx["active"] = False
            for h in (code_hook_handle, mmio_r_handle, mmio_w_handle):
                if h is not None:
                    try:
                        mu.hook_del(h)
                    except uc.UcError:
                        pass
            self.rollback()


def create_vm(is_elf: bool, raw: bytes, elf_segs: list[tuple[int, bytes]]) -> uc.Uc:
    vm = FastVM(is_elf, raw, elf_segs, {})
    return vm.mu


def run_one_vm(
    is_elf: bool,
    raw: bytes,
    elf_segs: list[tuple[int, bytes]],
    elf_syms: dict[str, int],
    case: DiffCase,
) -> dict[str, Any]:
    vm = FastVM(is_elf, raw, elf_segs, elf_syms)
    return vm.run_case(case)


def install_mock_pwr_vtable() -> list[tuple[int, bytes]]:
    """
    Assemble tiny Thumb mock callbacks in guest RAM (0x2010E000) for
    sub_FDD1C (get power domain), sub_FDD48 (set domain 0x6E), and
    sub_FDE94 (set domain 0x5C):
      - State word at 0x2010E100 (initial = 1)
      - Call counter at 0x2010E104 (initial = 0)
      - Last domain ID at 0x2010E108 (initial = 0)
      - Vtable at 0x2010E080 (+0x0C -> set_fn 0x2010E021, +0x14 -> get_fn 0x2010E001)
      - Pointer at RKOF_DATA_PWR_VTABLE_PTR_ADDR (0x20001BD0) -> 0x2010E080
    """
    # get_fn(r0=obj, r1=dom, r2=state_out):
    #   ldr r3, [pc, #8] ; str r1, [r3, #8] ; ldr r3, [r3] ; str r3, [r2] ; movs r0, #0 ; bx lr ; .word 0x2010E100
    get_code = bytes.fromhex("024b99601b6813600020704700e11020")
    # set_fn(r0=obj, r1=dom, r2=state):
    #   ldr r3, [pc, #12] ; str r1, [r3, #8] ; str r2, [r3] ; ldr r1, [r3, #4] ; adds r1, #1 ; str r1, [r3, #4] ; movs r0, #0 ; bx lr ; .word 0x2010E100
    set_code = bytes.fromhex("034b99601a605968491c59600020704700e11020")
    vtable = pack_u32(0, 0, 0, 0x2010E021, 0, 0x2010E001)
    return [
        (0x2010E000, get_code),
        (0x2010E020, set_code),
        (0x2010E080, vtable),
        (0x2010E100, pack_u32(1, 0, 0)),
        (0x20001BD0, pack_u32(0x2010E080)),
    ]


def panic_chain_stubs(halt_stop: bool = True) -> list[StubHook]:
    """Standard stub set for rtkit_panic / assert / stack_chk_fail paths."""
    return [
        StubHook("sub_187240", 0x187240, "sub_187240", record_args=1, cstr_args=(0,)),
        StubHook("sub_187222", 0x187222, "sub_187222", record_args=1, cstr_args=(0,)),
        StubHook("sub_188300", 0x188300, "sub_188300", record_args=3, cstr_args=(2,)),
        StubHook("sub_17C98", 0x017C98, "sub_17C98", record_args=0),
        StubHook("sub_17CEC", 0x017CEC, "sub_17CEC", record_args=0),
        StubHook(
            "sub_1A234C",
            0x1A234C,
            "sub_1A234C",
            record_args=0,
            stop_on_call=halt_stop,
        ),
    ]


def build_cases() -> list[DiffCase]:
    cases: list[DiffCase] = []

    # ---- Suite 1: boot_remap_and_exceptions ---------------------------------
    def _pbl_info_stub(uc_obj: uc.Uc, _trace: list[tuple[Any, ...]]) -> int:
        out_ptr = uc_obj.reg_read(ac.UC_ARM_REG_R0)
        uc_obj.mem_write(out_ptr, pack_u32(0xCAFE1234))
        return 0

    def _pbl_log_stub(uc_obj: uc.Uc, _trace: list[tuple[Any, ...]]) -> int:
        out_a = uc_obj.reg_read(ac.UC_ARM_REG_R0)
        out_b = uc_obj.reg_read(ac.UC_ARM_REG_R1)
        uc_obj.mem_write(out_a, pack_u32(0x20080000))
        uc_obj.mem_write(out_b, pack_u32(0x00004000))
        return 0

    cases.extend([
        DiffCase(
            suite="boot_remap",
            name="rtkit_entry_programs_rmc_rmd_rmfi_rtsi",
            symbol="rtkit_entry",
            ref_rva=0x000108,
            stop_pc=0x0001F8,
            mem_writes=[
                (0x4000C02C, pack_u32(0x00010005)),
                (0x4000C080, pack_u32(0x00000000)),
                (0x4000C12C, pack_u32(0x00000001)),
                (0x008000F0, pack_u32(0x00000000)),
            ],
            compare_ranges=[
                (0x4000C02C, 4),
                (0x4000C0A0, 0x20),
                (0x4000C120, 4),
                (0x008000F0, 4),
            ],
        ),
        DiffCase(
            suite="boot_remap",
            name="rtkit_entry_skips_when_status_done_bit_set",
            symbol="rtkit_entry",
            ref_rva=0x000108,
            stop_pc=0x0001F8,
            mem_writes=[
                (0x4000C080, pack_u32(0x00000002)),
                (0x4000C120, pack_u32(0xCAFEBABE)),
                (0x008000F0, pack_u32(0x11223344)),
            ],
            compare_ranges=[(0x4000C120, 4), (0x008000F0, 4)],
        ),
        DiffCase(
            suite="boot_remap",
            name="rtkit_start_clears_3_bss_spans_preserves_stacks_and_inits_mpu_fpu",
            symbol="rtkit_start",
            ref_rva=0x016D60,
            max_insns=4500000,
            check_r0=False,
            check_special_regs=True,
            mem_writes=[
                (0x20010000, pack_u32(0xDEADBEEF)),
                (0x200409DC, pack_u32(0x534B5452, 0x4B434154)),
                (0x20050000, pack_u32(0xBAADF00D)),
                (0x200D36B8, pack_u32(0x534B5452, 0x4B434154)),
                (0x200D4680, pack_u32(0xFEEDFACE)),
                (0xE000ED14, pack_u32(0x00000008)),
                (0xE000ED90, pack_u32(0x00000800)),
                (0xE000ED9C, pack_u32(0xFFFFFFE0)),
                (0x001F37B8, pack_u32(0x2010EC01)),
                (0x2010EC00, bytes.fromhex("704700bf")),
            ],
            compare_ranges=[
                (0x200001D8, 4),
                (0x20010000, 4),
                (0x200409DC, 8),
                (0x20050000, 4),
                (0x200D32D0, 0x40),
                (0x200D36B8, 8),
                (0x200D4680, 4),
                (0x200D46B0, 8),
                (0xE000ED14, 16),
                (0xE000ED88, 4),
            ],
            stubs=[
                StubHook("ctor0_stop", 0x2010EC00, "", record_args=0, stop_on_call=True)
            ],
        ),
        DiffCase(
            suite="boot_remap",
            name="stack_chk_init_copies_stkg_tlv",
            symbol="__stack_chk_init",
            ref_rva=0x00CA40,
            check_r0=False,
            mem_writes=[(0x200001D8, pack_u32(0))],
            compare_ranges=[(0x200001D8, 4)],
        ),
        DiffCase(
            suite="boot_remap",
            name="stack_chk_fail_invokes_rtkit_panic_canary_msg",
            symbol="__stack_chk_fail",
            ref_rva=0x00CA58,
            check_r0=False,
            mem_writes=[(0x2000C81C + 0x24, pack_u32(0))],
            stubs=panic_chain_stubs(),
        ),
        DiffCase(
            suite="boot_remap",
            name="sub_175C8_reads_boot_mode",
            symbol="sub_175C8",
            ref_rva=0x0175C8,
            mem_writes=[(0x20000180, pack_u32(0x00000003))],
        ),
        DiffCase(
            suite="boot_remap",
            name="sub_175D4_poisons_rtkstack_words",
            symbol="sub_175D4",
            ref_rva=0x0175D4,
            r0=0x20100100,
            r1=28,
            check_r0=False,
            mem_writes=[(0x20100100, b"\xAA" * 32)],
            compare_ranges=[(0x20100100, 32)],
        ),
        DiffCase(
            suite="boot_remap",
            name="sub_1748C_captures_hardfault_cfsr_hfsr_mmfar",
            symbol="sub_1748C",
            ref_rva=0x01748C,
            r0=0x20103000,
            r1=3,
            basepri=0x40,
            primask=1,
            check_r0=False,
            mem_writes=[
                (0x200CEDF0 + 0x4C, pack_u32(0)),
                (0x200D021C, b"\x00" * 0x20),
                (0x20103000, b"\x00" * 0xA0),
                (0x20103000 + 0x18, pack_u32(0xFFFFFFFD)),
                (0x20103000 + 0x54, pack_u32(0xFFFFFFFD)),
                (0x20103000 + 0x5C, pack_u32(0x01000200)),
                (0xE000ED28, pack_u32(0x00000082, 0x40000000, 0x00000000, 0xDEAD1234, 0xBEEF5678)),
                (0x2000C81C + 0x24, pack_u32(0)),
            ],
            compare_ranges=[
                (0x200CEDF0 + 0x4C, 4),
                (0x200D021C, 0x18),
                (0x20103000, 0x20),
            ],
            stubs=[
                StubHook("sub_1887D8", 0x1887D8, "sub_1887D8", record_args=0),
                *panic_chain_stubs(),
            ],
        ),
        DiffCase(
            suite="boot_remap",
            name="sub_17970_decodes_blx_and_bl32_return_address",
            symbol="sub_17970",
            ref_rva=0x017970,
            r0=0x20103105,
            mem_writes=[(0x20103100, struct.pack("<HH", 0xF000, 0xF802))],
            stubs=[StubHook("sub_1A375C", 0x1A375C, "sub_1A375C", ret_r0=1, record_args=1)],
        ),
        DiffCase(
            suite="boot_remap",
            name="sub_179C8_dispatches_panic_vprintf_and_coredump",
            symbol="sub_179C8",
            ref_rva=0x0179C8,
            r0=0x20103140,
            r1=0x20103160,
            check_r0=False,
            mem_writes=[
                (0x200CEE6C + 0x14, pack_u32(0)),
                (0x20103140, b"panic-test-%d\x00"),
                (0x20103160, pack_u32(42)),
            ],
            stubs=panic_chain_stubs(halt_stop=False),
        ),
        DiffCase(
            suite="boot_remap",
            name="rtkit_panic_masks_irq_and_halts",
            symbol="rtkit_panic",
            ref_rva=0x0179A4,
            r0=0x20103140,
            r1=99,
            check_r0=False,
            mem_writes=[
                (0x200CEE6C + 0x14, pack_u32(0)),
                (0x2000C81C + 0x24, pack_u32(0)),
                (0x20103140, b"Fatal error 0x%x\x00"),
            ],
            stubs=panic_chain_stubs(),
        ),
        DiffCase(
            suite="boot_remap",
            name="rtkit_halt_forever_populates_pblr_and_pblv_records",
            symbol="rtkit_halt_forever",
            ref_rva=0x017A10,
            r0=3,
            check_r0=False,
            mem_writes=[
                (0x2000C81C + 0x24, pack_u32(1)),
                (0x200CEDF0 + 0x78, pack_u32(0x20103200)),
                (0x20103200, b"\x00" * 24),
            ],
            compare_ranges=[(0x20103200, 20)],
            stubs=[
                StubHook("sub_17C98", 0x017C98, "sub_17C98", record_args=0),
                StubHook("sub_19CC9C", 0x19CC9C, "sub_19CC9C", record_args=0, on_call=_pbl_info_stub),
                StubHook("sub_19CBB4", 0x19CBB4, "sub_19CBB4", record_args=0, on_call=_pbl_log_stub),
                StubHook("sub_17CEC", 0x017CEC, "sub_17CEC", record_args=0),
                StubHook("sub_1A234C", 0x1A234C, "sub_1A234C", record_args=0, stop_on_call=True),
            ],
        ),
        DiffCase(
            suite="boot_remap",
            name="sub_17BD4_stores_big_endian_crash_fourcc",
            symbol="sub_17BD4",
            ref_rva=0x017BD4,
            r0=0x42504859,
            check_r0=False,
            mem_writes=[(0x200CFA10 + 0x44, b"\xFF" * 8)],
            compare_ranges=[(0x200CFA10 + 0x44, 5)],
        ),
        DiffCase(
            suite="boot_remap",
            name="sub_17BF8_bswaps_32bit_word",
            symbol="sub_17BF8",
            ref_rva=0x017BF8,
            r0=0x20103240,
            mem_writes=[(0x20103240, pack_u32(0x12345678))],
        ),
        DiffCase(
            suite="boot_remap",
            name="rtkit_assert_fail_formats_file_line_func",
            symbol="rtkit_assert_fail",
            ref_rva=0x187128,
            r0=0x20103260,
            r1=120,
            r2=0x20103280,
            check_r0=False,
            mem_writes=[
                (0x200CEE6C + 0x14, pack_u32(0)),
                (0x2000C81C + 0x24, pack_u32(0)),
                (0x20103260, b"bt_phy.c\x00"),
                (0x20103280, b"bt_phy_otp_init\x00"),
            ],
            stubs=panic_chain_stubs(),
        ),
        DiffCase(
            suite="boot_remap",
            name="rtkit_assert_fail_hex64_formats_64bit_args",
            symbol="rtkit_assert_fail_hex64",
            ref_rva=0x18713C,
            r0=0x20103260,
            r1=66,
            r2=0x20103280,
            stack_args=[0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00],
            check_r0=False,
            mem_writes=[
                (0x200CEE6C + 0x14, pack_u32(0)),
                (0x2000C81C + 0x24, pack_u32(0)),
                (0x20103260, b"bt_phy.c\x00"),
                (0x20103280, b"bt_phy_otp_init\x00"),
            ],
            stubs=panic_chain_stubs(),
        ),
        DiffCase(
            suite="boot_remap",
            name="rtkit_unimplemented_method_panics",
            symbol="rtkit_unimplemented_method",
            ref_rva=0x18A0DC,
            r0=0x201032A0,
            check_r0=False,
            mem_writes=[
                (0x200CEE6C + 0x14, pack_u32(0)),
                (0x2000C81C + 0x24, pack_u32(0)),
                (0x201032A0, b"OSObject::start\x00"),
            ],
            stubs=panic_chain_stubs(),
        ),
    ])

    # ---- Suite 2: libc ------------------------------------------------------
    def _vsnprintf_sink_stub(uc_obj: uc.Uc, trace: list[tuple[Any, ...]]) -> int:
        sink_addr = uc_obj.reg_read(ac.UC_ARM_REG_R0)
        putc_fn, cap, dst, written, flags = struct.unpack(
            "<5I", bytes(uc_obj.mem_read(sink_addr, 20))
        )
        # Normalize local scratch pointer when cap == 2 (initial cap == 0)
        dst_tag = dst if cap != 2 else 0xDEAD0002
        trace.append(("sink_fields", putc_fn, cap, dst_tag, written, flags))
        if cap > 4:
            uc_obj.mem_write(dst, b"OK!\x00")
        return 3

    cases.extend([
        DiffCase(
            suite="libc",
            name="sub_CA68_memchr_hit",
            symbol="sub_CA68",
            ref_rva=0x00CA68,
            r0=0x20100000,
            r1=ord("K"),
            r2=10,
            mem_writes=[(0x20100000, b"RTKit-W3!\x00")],
        ),
        DiffCase(
            suite="libc",
            name="sub_CA68_memchr_high_bit_0xfe_hit",
            symbol="sub_CA68",
            ref_rva=0x00CA68,
            r0=0x20100003,
            r1=0xFE,
            r2=12,
            mem_writes=[(0x20100003, b"\x80\x91\xA2\xFE\xFF\x00\x11\x22\x33\x44\x55\x66")],
        ),
        DiffCase(
            suite="libc",
            name="sub_CA68_memchr_miss_and_zero_len",
            symbol="sub_CA68",
            ref_rva=0x00CA68,
            r0=0x20100000,
            r1=ord("Z"),
            r2=9,
            mem_writes=[(0x20100000, b"RTKit-W3!")],
        ),
        DiffCase(
            suite="libc",
            name="memcmp_aligned_word_mismatch_returns_minus1",
            symbol="memcmp",
            ref_rva=0x00CA8C,
            r0=0x20100000,
            r1=0x20100020,
            r2=8,
            mem_writes=[
                (0x20100000, bytes([1, 2, 3, 10, 5, 6, 7, 8])),
                (0x20100020, bytes([1, 2, 3, 25, 5, 6, 7, 8])),
            ],
        ),
        DiffCase(
            suite="libc",
            name="memcmp_aligned_word_mismatch_returns_plus1",
            symbol="memcmp",
            ref_rva=0x00CA8C,
            r0=0x20100000,
            r1=0x20100020,
            r2=8,
            mem_writes=[
                (0x20100000, bytes([1, 2, 200, 10, 5, 6, 7, 8])),
                (0x20100020, bytes([1, 2, 100, 25, 5, 6, 7, 8])),
            ],
        ),
        DiffCase(
            suite="libc",
            name="memcmp_equal_64_bytes_returns_zero",
            symbol="memcmp",
            ref_rva=0x00CA8C,
            r0=0x20100000,
            r1=0x20100080,
            r2=64,
            mem_writes=[
                (0x20100000, bytes(((i * 7 + 0x80) & 0xFF) for i in range(64))),
                (0x20100080, bytes(((i * 7 + 0x80) & 0xFF) for i in range(64))),
            ],
        ),
        DiffCase(
            suite="libc",
            name="memcmp_unaligned_head_returns_raw_byte_diff",
            symbol="memcmp",
            ref_rva=0x00CA8C,
            r0=0x20100001,
            r1=0x20100021,
            r2=7,
            mem_writes=[
                (0x20100000, bytes([0, 10, 2, 3, 4, 5, 6, 7])),
                (0x20100020, bytes([0, 35, 2, 3, 4, 5, 6, 7])),
            ],
        ),
        DiffCase(
            suite="libc",
            name="memcmp_unaligned_rhs_word_loop",
            symbol="memcmp",
            ref_rva=0x00CA8C,
            r0=0x20100000,
            r1=0x20100021,
            r2=9,
            mem_writes=[
                (0x20100000, b"ABCDEFGH!"),
                (0x20100021, b"ABCDEFGH?"),
            ],
        ),
        DiffCase(
            suite="libc",
            name="memcpy_len_0_preserves_dst",
            symbol="memcpy",
            ref_rva=0x00CBC0,
            r0=0x20100100,
            r1=0x20100000,
            r2=0,
            mem_writes=[
                (0x20100000, b"\xAA\xBB\xCC\xDD"),
                (0x20100100, b"\x11\x22\x33\x44"),
            ],
            compare_ranges=[(0x20100100, 4)],
        ),
        DiffCase(
            suite="libc",
            name="memcpy_len_1_and_4_high_bit_bytes",
            symbol="memcpy",
            ref_rva=0x00CBC0,
            r0=0x20100100,
            r1=0x20100000,
            r2=4,
            mem_writes=[
                (0x20100000, b"\x80\xFE\xA5\xFF\x99"),
                (0x20100100, b"\x00" * 8),
            ],
            compare_ranges=[(0x20100100, 8)],
        ),
        DiffCase(
            suite="libc",
            name="memcpy_unaligned_31_bytes",
            symbol="memcpy",
            ref_rva=0x00CBC0,
            r0=0x20100103,
            r1=0x20100001,
            r2=31,
            mem_writes=[
                (0x20100000, bytes(((i + 0x80) & 0xFF) for i in range(64))),
                (0x20100100, b"\xFF" * 48),
            ],
            compare_ranges=[(0x20100100, 48)],
        ),
        DiffCase(
            suite="libc",
            name="memcpy_aligned_64_bytes",
            symbol="memcpy",
            ref_rva=0x00CBC0,
            r0=0x20100180,
            r1=0x20100000,
            r2=64,
            mem_writes=[
                (0x20100000, bytes(((i * 3 + 0x90) & 0xFF) for i in range(64))),
                (0x20100180, b"\x00" * 72),
            ],
            compare_ranges=[(0x20100180, 72)],
        ),
        DiffCase(
            suite="libc",
            name="memcpy_backward_overlap_12_bytes",
            symbol="memcpy",
            ref_rva=0x00CBC0,
            r0=0x20100003,
            r1=0x20100000,
            r2=12,
            mem_writes=[(0x20100000, b"0123456789abcdef")],
            compare_ranges=[(0x20100000, 16)],
        ),
        DiffCase(
            suite="libc",
            name="memcpy_backward_overlap_48_bytes",
            symbol="memcpy",
            ref_rva=0x00CBC0,
            r0=0x20100005,
            r1=0x20100000,
            r2=48,
            mem_writes=[(0x20100000, bytes(((i * 5 + 0x81) & 0xFF) for i in range(64)))],
            compare_ranges=[(0x20100000, 64)],
        ),
        DiffCase(
            suite="libc",
            name="memcpy_forward_overlap_12_bytes",
            symbol="memcpy",
            ref_rva=0x00CBC0,
            r0=0x20100000,
            r1=0x20100003,
            r2=12,
            mem_writes=[(0x20100000, b"0123456789abcdef")],
            compare_ranges=[(0x20100000, 16)],
        ),
        DiffCase(
            suite="libc",
            name="memcpy_same_ptr_noop",
            symbol="memcpy",
            ref_rva=0x00CBC0,
            r0=0x20100000,
            r1=0x20100000,
            r2=16,
            mem_writes=[(0x20100000, b"0123456789abcdef")],
            compare_ranges=[(0x20100000, 16)],
        ),
        DiffCase(
            suite="libc",
            name="memcpy_thunk_copy",
            symbol="memcpy_thunk",
            ref_rva=0x00CBB8,
            r0=0x20100200,
            r1=0x20100000,
            r2=19,
            mem_writes=[(0x20100000, b"AppleT2016RTKitOS!!\x00")],
            compare_ranges=[(0x20100200, 20)],
        ),
        DiffCase(
            suite="libc",
            name="memcpy_thunk_backward_overlap",
            symbol="memcpy_thunk",
            ref_rva=0x00CBB8,
            r0=0x20100003,
            r1=0x20100000,
            r2=12,
            mem_writes=[(0x20100000, b"0123456789abcdef")],
            compare_ranges=[(0x20100000, 16)],
        ),
        DiffCase(
            suite="libc",
            name="j_memcpy_copy",
            symbol="j_memcpy",
            ref_rva=0x0301B0,
            r0=0x20100240,
            r1=0x20100000,
            r2=12,
            mem_writes=[(0x20100000, b"W3-PHY-SHM!\x00")],
            compare_ranges=[(0x20100240, 12)],
        ),
        DiffCase(
            suite="libc",
            name="j_memcpy_backward_overlap",
            symbol="j_memcpy",
            ref_rva=0x0301B0,
            r0=0x20100003,
            r1=0x20100000,
            r2=12,
            mem_writes=[(0x20100000, b"0123456789abcdef")],
            compare_ranges=[(0x20100000, 16)],
        ),
        DiffCase(
            suite="libc",
            name="memset_len_0_and_1_boundary",
            symbol="memset",
            ref_rva=0x00CCD0,
            r0=0x20100300,
            r1=0xDE,
            r2=1,
            mem_writes=[(0x20100300, b"\x11\x22\x33\x44")],
            compare_ranges=[(0x20100300, 4)],
        ),
        DiffCase(
            suite="libc",
            name="memset_unaligned_31_bytes_high_bit",
            symbol="memset",
            ref_rva=0x00CCD0,
            r0=0x20100301,
            r1=0xA5,
            r2=31,
            mem_writes=[(0x20100300, b"\x00" * 36)],
            compare_ranges=[(0x20100300, 36)],
        ),
        DiffCase(
            suite="libc",
            name="memset_aligned_64_bytes",
            symbol="memset",
            ref_rva=0x00CCD0,
            r0=0x20100400,
            r1=0xFE,
            r2=64,
            mem_writes=[(0x20100400, b"\x00" * 68)],
            compare_ranges=[(0x20100400, 68)],
        ),
        DiffCase(
            suite="libc",
            name="sub_CD48_strchr_found_and_nul",
            symbol="sub_CD48",
            ref_rva=0x00CD48,
            r0=0x20100000,
            r1=0,
            mem_writes=[(0x20100000, b"bt_phy.c\x00")],
        ),
        DiffCase(
            suite="libc",
            name="sub_CD48_strchr_miss_returns_null",
            symbol="sub_CD48",
            ref_rva=0x00CD48,
            r0=0x20100000,
            r1=ord("z"),
            mem_writes=[(0x20100000, b"bt_phy.c\x00")],
        ),
        DiffCase(
            suite="libc",
            name="strcmp_ordering_negative",
            symbol="strcmp",
            ref_rva=0x00CD78,
            r0=0x20100000,
            r1=0x20100020,
            mem_writes=[
                (0x20100000, b"modem_inits.c\x00"),
                (0x20100020, b"modem_initz.c\x00"),
            ],
        ),
        DiffCase(
            suite="libc",
            name="strcmp_equal_and_unsigned_high_bit",
            symbol="strcmp",
            ref_rva=0x00CD78,
            r0=0x20100000,
            r1=0x20100020,
            mem_writes=[
                (0x20100000, b"rk\x80\x00"),
                (0x20100020, b"rk\x7f\x00"),
            ],
        ),
        DiffCase(
            suite="libc",
            name="strcmp_equal_returns_zero",
            symbol="strcmp",
            ref_rva=0x00CD78,
            r0=0x20100000,
            r1=0x20100020,
            mem_writes=[
                (0x20100000, b"identical\x00"),
                (0x20100020, b"identical\x00"),
            ],
        ),
        DiffCase(
            suite="libc",
            name="sub_CE20_strlen_empty_zero",
            symbol="sub_CE20",
            ref_rva=0x00CE20,
            r0=0x20100000,
            mem_writes=[(0x20100000, b"\x00abc")],
        ),
        DiffCase(
            suite="libc",
            name="sub_CE20_strlen_unaligned_21",
            symbol="sub_CE20",
            ref_rva=0x00CE20,
            r0=0x20100003,
            mem_writes=[(0x20100003, b"AirPods3,6-b515d-rkof\x00")],
        ),
        DiffCase(
            suite="libc",
            name="sub_CE20_strlen_aligned_64_high_bit",
            symbol="sub_CE20",
            ref_rva=0x00CE20,
            r0=0x20100040,
            mem_writes=[(0x20100040, (b"\x81\xFE\x42\x7F" * 16) + b"\x00")],
        ),
        DiffCase(
            suite="libc",
            name="strncmp_prefix_bounded_equal",
            symbol="strncmp",
            ref_rva=0x00CEA8,
            r0=0x20100000,
            r1=0x20100020,
            r2=4,
            mem_writes=[
                (0x20100000, b"uart_tx\x00"),
                (0x20100020, b"uart_rx\x00"),
            ],
        ),
        DiffCase(
            suite="libc",
            name="strncmp_mismatch_and_unsigned_high_bit",
            symbol="strncmp",
            ref_rva=0x00CEA8,
            r0=0x20100000,
            r1=0x20100020,
            r2=8,
            mem_writes=[
                (0x20100000, b"uart_\x90\x00"),
                (0x20100020, b"uart_\x20\x00"),
            ],
        ),
        DiffCase(
            suite="libc",
            name="rtkit_mem_is_filled_true",
            symbol="rtkit_mem_is_filled",
            ref_rva=0x187D7E,
            r0=0x20100001,
            r1=0xA5,
            r2=23,
            mem_writes=[(0x20100001, b"\xA5" * 23)],
        ),
        DiffCase(
            suite="libc",
            name="rtkit_mem_is_filled_false_on_tail_byte",
            symbol="rtkit_mem_is_filled",
            ref_rva=0x187D7E,
            r0=0x20100001,
            r1=0xA5,
            r2=23,
            mem_writes=[(0x20100001, (b"\xA5" * 22) + b"\xA4")],
        ),
        DiffCase(
            suite="libc",
            name="vsnprintf_builds_sink_struct_and_nul_terminates",
            symbol="vsnprintf",
            ref_rva=0x187818,
            r0=0x20100340,
            r1=16,
            r2=0x20100360,
            r3=0x20100380,
            mem_writes=[
                (0x20100340, b"XXXX"),
                (0x20100360, b"%s\x00"),
            ],
            compare_ranges=[(0x20100340, 4)],
            stubs=[
                StubHook(
                    "sub_18726C",
                    0x18726C,
                    "sub_18726C",
                    record_args=0,
                    on_call=_vsnprintf_sink_stub,
                )
            ],
        ),
        DiffCase(
            suite="libc",
            name="vsnprintf_zero_cap_uses_scratch_cap2",
            symbol="vsnprintf",
            ref_rva=0x187818,
            r0=0,
            r1=0,
            r2=0x20100360,
            r3=0x20100380,
            mem_writes=[(0x20100360, b"%d\x00")],
            stubs=[
                StubHook(
                    "sub_18726C",
                    0x18726C,
                    "sub_18726C",
                    record_args=0,
                    on_call=_vsnprintf_sink_stub,
                )
            ],
        ),
        DiffCase(
            suite="libc",
            name="snprintf_forwards_variadic_args_to_vsnprintf",
            symbol="snprintf",
            ref_rva=0x1877FE,
            r0=0x20100390,
            r1=32,
            r2=0x20100360,
            r3=0x12345678,
            mem_writes=[
                (0x20100390, b"ZZZZ"),
                (0x20100360, b"%x\x00"),
            ],
            compare_ranges=[(0x20100390, 4)],
            stubs=[
                StubHook(
                    "sub_18726C",
                    0x18726C,
                    "sub_18726C",
                    record_args=0,
                    on_call=_vsnprintf_sink_stub,
                )
            ],
        ),
    ])

    # ---- Suite 3: aeabi (64-bit integer + hard-float VFPv4) -----------------
    for sh_amt in (0, 1, 13, 31, 32, 33, 37, 63, 64, 95, 256):
        cases.append(
            DiffCase(
                suite="aeabi",
                name=f"sub_D23C_ashldi3_shift_{sh_amt}",
                symbol="sub_D23C",
                ref_rva=0x00D23C,
                r0=0x89ABCDEF,
                r1=0x01234567,
                r2=sh_amt,
                check_r1=True,
            )
        )
        cases.append(
            DiffCase(
                suite="aeabi",
                name=f"sub_D9F8_lshrdi3_shift_{sh_amt}",
                symbol="sub_D9F8",
                ref_rva=0x00D9F8,
                r0=0x89ABCDEF,
                r1=0xFEDCBA98,
                r2=sh_amt,
                check_r1=True,
            )
        )
    cases.extend([
        DiffCase(
            suite="aeabi",
            name="sub_D210_normalize_f64_subnormal",
            symbol="sub_D210",
            ref_rva=0x00D210,
            r0=0x20100000,
            mem_writes=[(0x20100000, struct.pack("<Q", 0x0000000000004000))],
            compare_ranges=[(0x20100000, 8)],
        ),
        DiffCase(
            suite="aeabi",
            name="sub_D5A4_normalize_f32_subnormal",
            symbol="sub_D5A4",
            ref_rva=0x00D5A4,
            r0=0x20100000,
            mem_writes=[(0x20100000, struct.pack("<Q", 0x0000000000000200))],
            compare_ranges=[(0x20100000, 8)],
        ),
        DiffCase(
            suite="aeabi",
            name="sub_DD76_udivmoddi4_64bit",
            symbol="sub_DD76",
            ref_rva=0x00DD76,
            r0=0xBD3E1F05,
            r1=0x00012345,
            r2=0x00001003,
            r3=0x00000000,
            stack_args=[0x20100040],
            check_r1=True,
            compare_ranges=[(0x20100040, 8)],
        ),
        DiffCase(
            suite="aeabi",
            name="sub_DD68_udivdi3_large",
            symbol="sub_DD68",
            ref_rva=0x00DD68,
            r0=0x00000000,
            r1=0x40000000,
            r2=0x00000003,
            r3=0x00000001,
            check_r1=True,
        ),
        DiffCase(
            suite="aeabi",
            name="sub_D5D0_divdi3_signed_negative",
            symbol="sub_D5D0",
            ref_rva=0x00D5D0,
            r0=(-1000000000000) & 0xFFFFFFFF,
            r1=((-1000000000000) >> 32) & 0xFFFFFFFF,
            r2=777,
            r3=0,
            check_r1=True,
        ),
        DiffCase(
            suite="aeabi",
            name="sub_D610_extendsfdf2_normal",
            symbol="sub_D610",
            ref_rva=0x00D610,
            s0_bits=f32_bits(-42.625),
            check_r0=False,
            check_d0=True,
        ),
        DiffCase(
            suite="aeabi",
            name="sub_D610_extendsfdf2_zero",
            symbol="sub_D610",
            ref_rva=0x00D610,
            s0_bits=0x80000000,
            check_r0=False,
            check_d0=True,
        ),
        DiffCase(
            suite="aeabi",
            name="sub_D610_extendsfdf2_subnormal",
            symbol="sub_D610",
            ref_rva=0x00D610,
            s0_bits=0x00000200,
            check_r0=False,
            check_d0=True,
        ),
        DiffCase(
            suite="aeabi",
            name="sub_D680_fixdfsi_negative",
            symbol="sub_D680",
            ref_rva=0x00D680,
            d0_bits=f64_bits(-123456.875),
        ),
        DiffCase(
            suite="aeabi",
            name="sub_D6C2_fixsfsi64_negative",
            symbol="sub_D6C2",
            ref_rva=0x00D6C2,
            s0_bits=f32_bits(-65536.0),
            check_r1=True,
        ),
        DiffCase(
            suite="aeabi",
            name="sub_D704_fixunsdfsi_positive",
            symbol="sub_D704",
            ref_rva=0x00D704,
            d0_bits=f64_bits(3000000000.0),
        ),
        DiffCase(
            suite="aeabi",
            name="sub_D736_fixunssfdi_large",
            symbol="sub_D736",
            ref_rva=0x00D736,
            s0_bits=f32_bits(16777216.0),
            check_r1=True,
        ),
        DiffCase(
            suite="aeabi",
            name="sub_D7BC_floatdisf_round_to_even",
            symbol="sub_D7BC",
            ref_rva=0x00D7BC,
            r0=(-98765432101) & 0xFFFFFFFF,
            r1=((-98765432101) >> 32) & 0xFFFFFFFF,
            check_r0=False,
            check_s0=True,
        ),
    ])

    # ---- Suite 4: irq_systick_sched_and_services ----------------------------
    timer_ticks = {"val": 100}

    def _mock_timer_read(_uc_obj: uc.Uc, _trace: list[tuple[Any, ...]]) -> int:
        timer_ticks["val"] += 30
        return timer_ticks["val"]

    systick_dev_init = [
        (0x200D46B0, pack_u32(0x80, 0x40)),
        (0x20100500, b"\x00" * 0x40),
        (0x20100500 + 0x18, pack_u32(24000000)),
    ]

    # Mock Spicy logger vtable (+0x24 -> emit_code @ 0x2010E461, +0x28 -> emit_ex_code @ 0x2010E481)
    # emit_code(r0=self, r1=log_id, r2=ap): ldr r2, [r2] ; ldr r3, [pc, #4] ; stm r3!, {r0, r1, r2} ; bx lr ; .word 0x2010E4A0
    spicy_emit_code = bytes.fromhex("1268014b07c37047" + "a0e41020")
    # emit_ex_code(r0=self, r1=lvl, r2=log_id, r3=ap): ldr r3, [r3] ; push {r4} ; ldr r4, [pc, #4] ; stm r4!, {r0, r1, r2, r3} ; pop {r4} ; bx lr ; .word 0x2010E4C0
    spicy_emit_ex_code = bytes.fromhex("1b6810b4014c0fc410bc7047" + "c0e41020")
    spicy_vtable = pack_u32(*([0] * 9), 0x2010E461, 0x2010E481)
    spicy_setup = [
        (0x200CF630, pack_u32(0x2010E400)),
        (0x2010E400, pack_u32(0x2010E420)),
        (0x2010E420, spicy_vtable),
        (0x2010E460, spicy_emit_code),
        (0x2010E480, spicy_emit_ex_code),
        (0x2010E4A0, b"\x00" * 64),
    ]

    cases.extend([
        DiffCase(
            suite="irq_systick_sched",
            name="rtkit_enter_critical_sets_basepri",
            symbol="rtkit_enter_critical",
            ref_rva=0x016F08,
            basepri=0x80,
            check_special_regs=True,
            mem_writes=[(0x200D46B0, pack_u32(0x80, 0x40))],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_16F20_restores_basepri",
            symbol="sub_16F20",
            ref_rva=0x016F20,
            r0=0x80,
            basepri=0x40,
            check_r0=False,
            check_special_regs=True,
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_16F26_reads_primask_and_disables_irq",
            symbol="sub_16F26",
            ref_rva=0x016F26,
            primask=0,
            check_special_regs=True,
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_16F38_restores_primask_zero",
            symbol="sub_16F38",
            ref_rva=0x016F38,
            r0=1,
            primask=1,
            check_r0=False,
            check_special_regs=True,
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_16F44_irq_masked_by_basepri_returns_0",
            symbol="sub_16F44",
            ref_rva=0x016F44,
            r0=0x55,
            primask=0,
            basepri=0x40,
            mem_writes=[(0x200D46B0, pack_u32(0x80, 0x40))],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_16F44_irq_unmasked_returns_1",
            symbol="sub_16F44",
            ref_rva=0x016F44,
            r0=0x55,
            primask=0,
            basepri=0x00,
            mem_writes=[(0x200D46B0, pack_u32(0x80, 0x40))],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_17024_executes_dsb_isb_wfi_idle_loop",
            symbol="sub_17024",
            ref_rva=0x017024,
            stop_on_wfi=True,
            check_r0=False,
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_17064_enables_nvic_iser_and_clears_icpr",
            symbol="sub_17064",
            ref_rva=0x017064,
            r0=1,
            r1=23,
            r2=1,
            compare_ranges=[(0xE000E100, 4), (0xE000E280, 4)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_170C8_registers_nvic_handler_and_arg",
            symbol="sub_170C8",
            ref_rva=0x0170C8,
            r0=0,
            r1=23,
            r2=0x0001760D,
            r3=0x12345678,
            mem_writes=[(0x200011E0, pack_u32(64))],
            compare_ranges=[
                (0x200D3460 + 36, 4),
                (0x200D3588 + 36, 4),
            ],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_1710C_sets_systick_and_external_irq_prio",
            symbol="sub_1710C",
            ref_rva=0x01710C,
            r0=0,
            r1=21,
            r2=2,
            mem_writes=[
                (0x200011E0, pack_u32(64)),
                (0x200D46B0, pack_u32(0x80, 0x04)),
            ],
            compare_ranges=[(0xE000E404, 4)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_175F0_acquires_basepri_spinlock",
            symbol="sub_175F0",
            ref_rva=0x0175F0,
            r0=0x20100380,
            mem_writes=[
                (0x200D46B0, pack_u32(0x80, 0x40)),
                (0x20100380, pack_u32(0)),
            ],
            compare_ranges=[(0x20100380, 4)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_1760C_releases_basepri_spinlock",
            symbol="sub_1760C",
            ref_rva=0x01760C,
            r0=0x20100380,
            basepri=0x40,
            check_r0=False,
            check_special_regs=True,
            mem_writes=[(0x20100380, pack_u32(0x81))],
            compare_ranges=[(0x20100380, 4)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="rtkit_sched_switch_swaps_thread_sp",
            symbol="rtkit_sched_switch",
            ref_rva=0x016F8C,
            r0=0x20040D00,
            mem_writes=[
                (0x200D46B0, pack_u32(0x80, 0x40)),
                (0x200CEBF0, b"\x01"),
                (0x200CE7E0, pack_u32(1 << 5)),
                (0x200CE7E8 + (5 * 8), pack_u32(0x20100440)),
                (0x2000C710, pack_u32(0x20100400)),
                (0x20100400, b"\x00" * 0x30),
                (0x20100400 + 0x28, pack_u32(0x20100480)),
                (0x20100440, b"\x00" * 0x30),
                (0x20100440 + 0x28, pack_u32(0x20100484)),
                (0x20100480, pack_u32(0, 0x20040E80)),
            ],
            compare_ranges=[
                (0x200CEBF0, 1),
                (0x2000C710, 4),
                (0x20100480, 8),
            ],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_1A2994_higher_prio_thread_preempts_current",
            symbol="sub_1A2994",
            ref_rva=0x1A2994,
            r0=0x20100400,
            mem_writes=[
                (0x2000C710, pack_u32(0x20100440)),
                (0x20100400, b"\x00" * 0x20),
                (0x20100400 + 0x14, bytes([12, 0, 1])),
                (0x20100440, b"\x00" * 0x20),
                (0x20100440 + 0x14, bytes([4, 0, 0])),
            ],
            compare_ranges=[(0x20100400 + 0x16, 1)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_1A2994_lower_prio_thread_returns_zero",
            symbol="sub_1A2994",
            ref_rva=0x1A2994,
            r0=0x20100400,
            mem_writes=[
                (0x2000C710, pack_u32(0x20100440)),
                (0x20100400, b"\x00" * 0x20),
                (0x20100400 + 0x14, bytes([2, 0, 1])),
                (0x20100440, b"\x00" * 0x20),
                (0x20100440 + 0x14, bytes([9, 0, 0])),
            ],
            compare_ranges=[(0x20100400 + 0x16, 1)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_18A494_systick_periodic_config",
            symbol="sub_18A494",
            ref_rva=0x18A494,
            r0=0x20100500,
            r1=0,
            r2=24000,
            r3=0,
            mem_writes=systick_dev_init,
            compare_ranges=[(0x20100500 + 0x30, 12), (0xE000E010, 12)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_18A4D0_systick_oneshot_valid_period",
            symbol="sub_18A4D0",
            ref_rva=0x18A4D0,
            r0=0x20100500,
            r1=0,
            r2=48000,
            r3=0,
            mem_writes=systick_dev_init,
            compare_ranges=[(0x20100500 + 0x30, 12), (0xE000E010, 12)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_18A510_systick_reads_remaining_ticks",
            symbol="sub_18A510",
            ref_rva=0x18A510,
            r0=0x20100500,
            r1=0x20100560,
            mem_writes=systick_dev_init
            + [
                (0x20100500 + 0x30, pack_u32(10000, 0)),
                (0xE000E010, pack_u32(7, 24000, 1234)),
                (0x20100560, pack_u32(0, 0)),
            ],
            compare_ranges=[(0x20100560, 8)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_18A568_ticks_to_us_ceil_24mhz",
            symbol="sub_18A568",
            ref_rva=0x18A568,
            r0=0x20100500,
            r1=0,
            r2=2401,
            r3=0,
            check_r1=True,
            mem_writes=systick_dev_init,
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_18A59C_us_to_ticks_ceil_24mhz",
            symbol="sub_18A59C",
            ref_rva=0x18A59C,
            r0=0x20100500,
            r1=0,
            r2=125,
            r3=0,
            check_r1=True,
            mem_writes=systick_dev_init,
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_18A5E0_splits_large_64bit_reload_across_rvr_and_rem",
            symbol="sub_18A5E0",
            ref_rva=0x18A5E0,
            r0=0x20100500,
            r1=0,
            r2=0x02000100,
            r3=0,
            check_r0=False,
            mem_writes=systick_dev_init,
            compare_ranges=[(0x20100500 + 0x30, 8), (0xE000E010, 12)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_18A134_udelay_polls_timer_24mhz",
            symbol="sub_18A134",
            ref_rva=0x18A134,
            r0=3,
            r1=0,
            check_r0=False,
            mem_writes=[(0x20001D80, pack_u32(0x20005000))],
            stubs=[
                StubHook(
                    name="sub_1A3204",
                    ref_addr=0x1A3204,
                    elf_sym="sub_1A3204",
                    record_args=1,
                    on_call=_mock_timer_read,
                )
            ],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_1887C4_sets_coredump_overflow_bit",
            symbol="sub_1887C4",
            ref_rva=0x1887C4,
            check_r0=False,
            mem_writes=[
                (0x20008128, b"\x00" * 0x64),
                (0x20008128 + 0x54, pack_u32(1)),
            ],
            compare_ranges=[(0x20008128, 0x64)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_16D20_forwards_heap_free_mode_1",
            symbol="sub_16D20",
            ref_rva=0x016D20,
            r0=0x20100600,
            r1=0x20100680,
            check_r0=False,
            stubs=[
                StubHook("rtkit_heap_free", 0x016718, "rtkit_heap_free", record_args=3)
            ],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_16D26_walks_heap_tail_block_size",
            symbol="sub_16D26",
            ref_rva=0x016D26,
            r0=0x20100600,
            mem_writes=[
                (0x20100600 + 0x48, pack_u32(0x20100680)),
                (0x20100680, pack_u32(0, 0x83, 0, 0x201006A0)),
                (0x201006A0, pack_u32(0, 0x143, 0, 0)),
            ],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="sub_16D46_toggles_heap_flag_and_notifies_sub_167E8",
            symbol="sub_16D46",
            ref_rva=0x016D46,
            r0=0x20100600,
            r1=1,
            check_r0=False,
            mem_writes=[(0x20100600 + 0x4C, b"\x00")],
            compare_ranges=[(0x20100600 + 0x4C, 1)],
            stubs=[StubHook("sub_167E8", 0x0167E8, "sub_167E8", record_args=2)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="rtkit_shell_register_cmd_inserts_entry_and_increments_count",
            symbol="rtkit_shell_register_cmd",
            ref_rva=0x01CA44,
            r0=0x20103800,
            r1=0x20103AC0,
            r2=0x20103AD0,
            r3=0x000FE121,
            mem_writes=[
                (0x20103800, b"\x00" * 0x240),
                (0x20103800 + 0x04, pack_u32(0x20103B00)),
                (0x20103800 + 0x220, pack_u32(1, 8)),
                (0x20103800 + 0x234, b"\x01"),
                (0x20103B00, b"\x00" * 32),
            ],
            compare_ranges=[(0x20103800 + 0x220, 8), (0x20103B00, 24)],
            stubs=[
                StubHook("sub_1A2A3C", 0x1A2A3C, "sub_1A2A3C", ret_r0=0x77, record_args=1),
                StubHook("sub_1A295C", 0x1A295C, "sub_1A295C", record_args=2),
            ],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="spicy_log_emit_dispatches_vtable_0x24_with_vararg",
            symbol="spicy_log_emit",
            ref_rva=0x0C7AB4,
            r0=0x11223344,
            r1=0xAABBCCDD,
            check_r0=False,
            mem_writes=spicy_setup,
            compare_ranges=[(0x2010E4A0, 12)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="spicy_log_emit_ex_dispatches_vtable_0x28_with_vararg",
            symbol="spicy_log_emit_ex",
            ref_rva=0x0C7AF4,
            r0=3,
            r1=0x55667788,
            r2=0xCAFE9999,
            check_r0=False,
            mem_writes=spicy_setup,
            compare_ranges=[(0x2010E4C0, 16)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="rtkit_heap_lock_or_init_initializes_two_sram_arenas",
            symbol="rtkit_heap_lock_or_init",
            ref_rva=0x186AC0,
            r0=0x20103C00,
            mem_writes=[
                (0x20024398 + 8, pack_u32(0)),
                (0x20103C00, pack_u32(0)),
            ],
            compare_ranges=[(0x20024398 + 8, 4), (0x20103C00, 4)],
            stubs=[
                StubHook("sub_1A2A3C", 0x1A2A3C, "sub_1A2A3C", ret_r0=0x33, record_args=1),
                StubHook("sub_1AA14", 0x01AA14, "sub_1AA14", ret_r0=0x200CD700, record_args=1),
                StubHook("sub_186A8C", 0x186A8C, "sub_186A8C", record_args=2),
            ],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="rtkit_heap_unlock_releases_heap_spinlock",
            symbol="rtkit_heap_unlock",
            ref_rva=0x186B3C,
            r0=0x33,
            check_r0=False,
            stubs=[StubHook("sub_1A295C", 0x1A295C, "sub_1A295C", record_args=2)],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="rt_malloc_locks_allocates_and_unlocks",
            symbol="rt_malloc",
            ref_rva=0x186BBC,
            r0=64,
            mem_writes=[(0x20024398 + 8, pack_u32(0x200CD700))],
            stubs=[
                StubHook("sub_1A2A3C", 0x1A2A3C, "sub_1A2A3C", ret_r0=0x44, record_args=1),
                StubHook(
                    "rtkit_heap_alloc",
                    0x016C48,
                    "rtkit_heap_alloc",
                    ret_r0=0x20104000,
                    record_args=2,
                ),
                StubHook("sub_1A295C", 0x1A295C, "sub_1A295C", record_args=2),
            ],
        ),
        DiffCase(
            suite="irq_systick_sched",
            name="rt_free_unwraps_0xab5c155a_guard_header_before_free",
            symbol="rt_free",
            ref_rva=0x186E14,
            r0=0x20104020,
            check_r0=False,
            mem_writes=[
                (0x20024398 + 8, pack_u32(0x200CD700)),
                (0x20104018, pack_u32(0x20104000, 0xAB5C155A)),
            ],
            stubs=[
                StubHook("sub_1A2A3C", 0x1A2A3C, "sub_1A2A3C", ret_r0=0x44, record_args=1),
                StubHook("rtkit_heap_free", 0x016718, "rtkit_heap_free", record_args=3),
                StubHook("sub_1A295C", 0x1A295C, "sub_1A295C", record_args=2),
            ],
        ),
    ])

    # ---- Suite 5: coproc_supervisor -----------------------------------------
    for mask_val, label in [
        (0x02, "mask_0x02"),
        (0x00010002, "uxth_quirk_0x10002"),
        (0x00010004, "non_uxth_0x10004_null"),
        (0x04, "mask_0x04"),
        (0x08, "mask_0x08"),
        (0x10, "mask_0x10"),
        (0x20, "mask_0x20_dyn0"),
        (0x40, "mask_0x40_dyn40"),
        (0x80, "mask_0x80"),
    ]:
        cases.append(
            DiffCase(
                suite="coproc_supervisor",
                name=f"sub_17F5C_{label}",
                symbol="sub_17F5C",
                ref_rva=0x017F5C,
                r0=mask_val,
            )
        )

    # Mock BTM enable callback @ 0x2010E200:
    #   ldr r3, [pc, #4] ; stm r3!, {r0, r1, r2} ; movs r0, #0x2A ; bx lr ; .word 0x2010E240
    btm_cb_code = bytes.fromhex("014b07c32a207047" + "40e21020")
    # Mock GTB vtable callbacks @ 0x2010E320 (init_fn) and 0x2010E340 (reg_fn):
    gtb_init_code = bytes.fromhex("00207047")
    gtb_reg_code = bytes.fromhex("01480ec000207047" + "80e31020")

    cases.extend([
        DiffCase(
            suite="coproc_supervisor",
            name="sub_15B20_calls_sub_15830_and_btm_vtable_enable",
            symbol="sub_15B20",
            ref_rva=0x015B20,
            mem_writes=[
                (0x20000B88, pack_u32(0x2010E220)),
                (0x2010E220, pack_u32(0x2010E201)),
                (0x2010E200, btm_cb_code),
                (0x2010E240, b"\x00" * 16),
            ],
            compare_ranges=[(0x2010E240, 12)],
            stubs=[StubHook("sub_15830", 0x015830, "sub_15830", ret_r0=0, record_args=0)],
        ),
        DiffCase(
            suite="coproc_supervisor",
            name="sub_15B78_populates_coproc_grant_from_carveout",
            symbol="sub_15B78",
            ref_rva=0x015B78,
            r0=0xDEADBEEF,
            mem_writes=[
                (
                    0x20024220,
                    pack_u32(0, 0, 0, 0, 0x4D280000, 0x10000, 0x4D289000, 0xABCD0001),
                ),
            ],
            compare_ranges=[(0x200009B0, 0x10)],
        ),
        DiffCase(
            suite="coproc_supervisor",
            name="sub_15B98_notifies_coproc2_and_calls_sub_13328_slot",
            symbol="sub_15B98",
            ref_rva=0x015B98,
            mem_writes=[(0x20000A90 + 8, b"\x07")],
            stubs=[
                StubHook("sub_1A5A8", 0x01A5A8, "sub_1A5A8", record_args=1),
                StubHook("sub_13328", 0x013328, "sub_13328", record_args=1),
            ],
        ),
        DiffCase(
            suite="coproc_supervisor",
            name="sub_15BAC_populates_btp_desc_and_bt_phy_otp_init_cb",
            symbol="sub_15BAC",
            ref_rva=0x015BAC,
            mem_writes=[
                (0x20024220, pack_u32(0x4D280000, 0x10000, 0, 0, 0x4D289000, 0x11223344)),
                (0x2000D2FC + 0x10, pack_u32(0x20100700)),
                (0x20100700 + 0x78, pack_u32(0, 0xFFFFFFFF)),
            ],
            compare_ranges=[(0x2000C988 + 0x48, 16), (0x20100700 + 0x78, 8)],
            stubs=[
                StubHook("sub_141B0", 0x0141B0, "sub_141B0", record_args=0),
                StubHook("sub_1A646", 0x01A646, "sub_1A646", record_args=1),
            ],
        ),
        DiffCase(
            suite="coproc_supervisor",
            name="sub_15C1C_calls_sub_1BA50",
            symbol="sub_15C1C",
            ref_rva=0x015C1C,
            mem_writes=[(0x20040648, b"\x00")],
            compare_ranges=[(0x20040648, 1)],
            stubs=[StubHook("sub_1BA50", 0x01BA50, "sub_1BA50", ret_r0=0, record_args=0)],
        ),
        DiffCase(
            suite="coproc_supervisor",
            name="sub_15C4C_calls_sub_1BA72",
            symbol="sub_15C4C",
            ref_rva=0x015C4C,
            mem_writes=[(0x20040648, b"\x01")],
            compare_ranges=[(0x20040648, 1)],
            stubs=[StubHook("sub_1BA72", 0x01BA72, "sub_1BA72", ret_r0=0, record_args=0)],
        ),
        DiffCase(
            suite="coproc_supervisor",
            name="sub_F1B90_gtb_capture_platform_registers_0x267_0x1305_and_36_regs",
            symbol="sub_F1B90",
            ref_rva=0x0F1B90,
            r0=0,
            r1=0x778899AA,
            mem_writes=[
                (0x20067F98, pack_u32(0)),
                (0x20002DD0, pack_u32(0x2010E300)),
                (0x2010E300, pack_u32(0x2010E321, 0, 0, 0x2010E341)),
                (0x2010E320, gtb_init_code),
                (0x2010E340, gtb_reg_code),
                (0x2010E380, b"\x00" * 16),
            ],
            compare_ranges=[(0x20067F98, 4), (0x2010E380, 12)],
            stubs=[StubHook("sub_22F14", 0x022F14, "sub_22F14", ret_r0=0, record_args=3)],
        ),
    ])

    # ---- Suite 6: phy_modem_wnvv_and_crash ----------------------------------
    crash_setup = install_mock_pwr_vtable() + [
        (0x4D289000, bytes((i & 0xFF) for i in range(0x200))),
        (0x4D289300, b"W3-PHYB-FAULT-DETAIL!___"),
        (
            0x4D289200,
            bytes(range(0x40))
            + pack_u32(0xBEEF1234, 0x4D289300, 24)
            + bytes(range(0x4C)),
        ),
    ]

    def _otp_word_stub(uc_obj: uc.Uc, _trace: list[tuple[Any, ...]]) -> int:
        out_ptr = uc_obj.reg_read(ac.UC_ARM_REG_R0)
        aligned_off = uc_obj.reg_read(ac.UC_ARM_REG_R1) & 0xFFFF
        val = 0x44332211 if aligned_off == 4 else 0x88776655
        uc_obj.mem_write(out_ptr, pack_u32(val))
        return 0

    def _otp_empty_stub(uc_obj: uc.Uc, _trace: list[tuple[Any, ...]]) -> int:
        out_ptr = uc_obj.reg_read(ac.UC_ARM_REG_R0)
        uc_obj.mem_write(out_ptr, pack_u32(0xFFFFFFFF))
        return 0

    bump_state = {"elf": 0x20110000, "ref": 0x20110000}

    def _make_bump_malloc(slot: str) -> Callable[[uc.Uc, list[tuple[Any, ...]]], int]:
        def _alloc(uc_obj: uc.Uc, _trace: list[tuple[Any, ...]]) -> int:
            sz = uc_obj.reg_read(ac.UC_ARM_REG_R0)
            addr = bump_state[slot]
            bump_state[slot] = (addr + sz + 7) & ~7
            return addr

        return _alloc

    def _bump_alloc_stateless(uc_obj: uc.Uc, _trace: list[tuple[Any, ...]]) -> int:
        # Read/advance bump pointer stored inside guest RAM at 0x2010FFE0 so ref & elf are identical!
        cur = struct.unpack("<I", bytes(uc_obj.mem_read(0x2010FFE0, 4)))[0]
        sz = uc_obj.reg_read(ac.UC_ARM_REG_R0)
        nxt = (cur + sz + 7) & ~7
        uc_obj.mem_write(0x2010FFE0, pack_u32(nxt))
        return cur

    # Precompute valid 'NV' container for standalone sub_FDFD0 test:
    # Header: payload_len=4, magic=0x4E56 ('NV'), flags=0, crc16=computed
    # Body (4 bytes): 2x 1-byte tags (0x1234, 0x5678)
    nv_head6 = struct.pack("<HHH", 4, 0x4E56, 0)
    nv_body4 = bytes([0x34, 0x12, 0x78, 0x56])

    def _host_ror16_crc(seed: int, data: bytes) -> int:
        acc = seed & 0xFFFF
        for b in data:
            rot = ((acc >> 1) | ((acc & 1) << 15)) & 0xFFFF
            acc = (rot + b) & 0xFFFF
        return 0xFFFF if acc == 0 else acc

    nv_crc = _host_ror16_crc(_host_ror16_crc(0, nv_head6), nv_body4)
    valid_nv_blob = nv_head6 + struct.pack("<H", nv_crc) + nv_body4 + (b"\xFF" * 32)

    cases.extend([
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDD1C_queries_power_domain_0x6e",
            symbol="sub_FDD1C",
            ref_rva=0x0FDD1C,
            r0=0x20101800,
            mem_writes=install_mock_pwr_vtable() + [(0x20101800, pack_u32(0))],
            compare_ranges=[(0x20101800, 4), (0x2010E100, 12)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDD48_sets_power_domain_0x6e",
            symbol="sub_FDD48",
            ref_rva=0x0FDD48,
            r0=2,
            mem_writes=install_mock_pwr_vtable(),
            compare_ranges=[(0x2010E100, 12)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDE94_sets_otp_power_domain_0x5c",
            symbol="sub_FDE94",
            ref_rva=0x0FDE94,
            r0=2,
            mem_writes=install_mock_pwr_vtable(),
            compare_ranges=[(0x2010E100, 12)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDEC0_reads_unaligned_32bit_word_across_otp_granules",
            symbol="sub_FDEC0",
            ref_rva=0x0FDEC0,
            r0=5,
            stubs=[
                StubHook(
                    "sub_18E930",
                    0x18E930,
                    "sub_18E930",
                    record_args=0,
                    on_call=_otp_word_stub,
                )
            ],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDD74_ror16_crc_checksum",
            symbol="sub_FDD74",
            ref_rva=0x0FDD74,
            r0=0xFFFF,
            r1=0x20100000,
            r2=16,
            mem_writes=[(0x20100000, b"WNVV-CONTAINER01")],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDF78_media_crc16_direct_and_zero_clamp",
            symbol="sub_FDF78",
            ref_rva=0x0FDF78,
            r0=0,
            r1=0x20100000,
            r2=4,
            r3=0,
            mem_writes=[(0x20100000, b"\x00\x00\x00\x00")],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDD90_assert_w3_phy_and_modem_reset",
            symbol="sub_FDD90",
            ref_rva=0x0FDD90,
            r0=1,
            check_r0=False,
            mem_writes=[
                (0x4D154048, pack_u32(0x00000025)),
                (0x4D140000, pack_u32(0x00000003)),
            ],
            compare_ranges=[(0x4D154048, 4), (0x4D140000, 4)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDD90_release_w3_phy_and_modem_reset",
            symbol="sub_FDD90",
            ref_rva=0x0FDD90,
            r0=0,
            check_r0=False,
            mem_writes=[
                (0x4D154048, pack_u32(0x0000007F)),
                (0x4D140000, pack_u32(0x000FFFFF)),
            ],
            compare_ranges=[(0x4D154048, 4), (0x4D140000, 4)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDE48_records_in_window_and_skips_out_of_window",
            symbol="sub_FDE48",
            ref_rva=0x0FDE48,
            r0=0x4D154ABC,
            r1=0x13579BDF,
            check_r0=False,
            mem_writes=[(0x4D291000, b"\x00" * 32)],
            compare_ranges=[(0x4D291000, 24)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDE48_ignores_outside_4d154000_4d155000",
            symbol="sub_FDE48",
            ref_rva=0x0FDE48,
            r0=0x4D155004,
            r1=0x13579BDF,
            check_r0=False,
            mem_writes=[(0x4D291000, b"\x00" * 32)],
            compare_ranges=[(0x4D291000, 24)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDE74_triggers_modem_inits_line_157_assert",
            symbol="sub_FDE74",
            ref_rva=0x0FDE74,
            check_r0=False,
            mem_writes=[
                (0x200CEE6C + 0x14, pack_u32(0)),
                (0x2000C81C + 0x24, pack_u32(0)),
            ],
            stubs=panic_chain_stubs(),
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDDDC_replays_mmio_pairs_and_logs_warm_restore",
            symbol="sub_FDDDC",
            ref_rva=0x0FDDDC,
            r0=0x20100000,
            r1=24,
            check_r0=False,
            mem_writes=[
                (0x4D291000, b"\x00" * 40),
                (
                    0x20100000,
                    pack_u32(
                        0x4D154100, 0x11112222,
                        0x4D140020, 0x33334444,
                        0x4D154200, 0x55556666,
                    ),
                ),
            ],
            compare_ranges=[
                (0x4D154100, 4),
                (0x4D140020, 4),
                (0x4D154200, 4),
                (0x4D291000, 32),
            ],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDDF8_coalesces_consecutive_same_reg_rmw_triples",
            symbol="sub_FDDF8",
            ref_rva=0x0FDDF8,
            r0=0x20100000,
            r1=36,
            check_r0=False,
            mem_writes=[
                (0x4D291000, b"\x00" * 40),
                (0x4D154300, pack_u32(0x000000FF)),
                (0x4D154304, pack_u32(0xF0000000)),
                (
                    0x20100000,
                    pack_u32(
                        0x4D154300, 0x0000000F, 0x000000A0,
                        0x4D154300, 0x0000FF00, 0x00005B00,
                        0x4D154304, 0x000000FF, 0x00000042,
                    ),
                ),
            ],
            compare_ranges=[
                (0x4D154300, 8),
                (0x4D291000, 32),
            ],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="bt_phy_mmio_pre_otp_rf_pmgr_sequence",
            symbol="bt_phy_mmio_pre_otp",
            ref_rva=0x10694C,
            check_r0=False,
            mem_writes=[
                (0x4246E004, pack_u32(0x00000100)),
                (0x4246E03C, pack_u32(0x00800000)),
                (0x4247A004, pack_u32(0x0000003F)),
            ],
            compare_ranges=[(0x4246E004, 0x78), (0x4247A004, 4)],
            stubs=[StubHook("sub_18A134", 0x18A134, "sub_18A134", record_args=2)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="bt_phy_mmio_init_seq_rf_pmgr_sequence",
            symbol="bt_phy_mmio_init_seq",
            ref_rva=0x1068A0,
            check_r0=False,
            mem_writes=[
                (0x4246E034, pack_u32(0x00000300)),
                (0x4247A004, pack_u32(0x00000025)),
            ],
            compare_ranges=[(0x4246E004, 0x78), (0x4247A004, 4)],
            stubs=[StubHook("sub_18A134", 0x18A134, "sub_18A134", record_args=2)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDC04_formats_spicy_log_descriptor",
            symbol="sub_FDC04",
            ref_rva=0x0FDC04,
            r0=0x20100800,
            check_r0=False,
            mem_writes=[(0x20100800, b"\xFF" * 32)],
            compare_ranges=[(0x20100800, 24)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDC38_harvests_bphy_trace_and_crash_sram",
            symbol="sub_FDC38",
            ref_rva=0x0FDC38,
            r0=0,
            r1=0x20101000,
            r2=0x400,
            mem_writes=crash_setup + [(0x20101000, b"\x00" * 0x300)],
            compare_ranges=[(0x20101000, 0x2B4), (0x2010E100, 8)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDCB4_harvests_mgrc_crash_detail",
            symbol="sub_FDCB4",
            ref_rva=0x0FDCB4,
            r0=0,
            r1=0x20101400,
            r2=64,
            mem_writes=crash_setup + [(0x20101400, b"\x00" * 64)],
            compare_ranges=[(0x20101400, 40), (0x2010E100, 8)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDCB4_overflow_marks_coredump_without_power_restore",
            symbol="sub_FDCB4",
            ref_rva=0x0FDCB4,
            r0=0,
            r1=0x20101400,
            r2=16,
            mem_writes=crash_setup
            + [
                (0x20008128, b"\x00" * 0x64),
                (0x20008128 + 0x54, pack_u32(1)),
            ],
            compare_ranges=[(0x20008128, 0x64), (0x2010E100, 8)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDF08_validates_wnvv_header_direct_sram",
            symbol="sub_FDF08",
            ref_rva=0x0FDF08,
            r0=0x20102000,
            r1=0x20102100,
            r2=0,
            r3=0x20102080,
            mem_writes=[
                (0x20102000, struct.pack("<HHHH", 0x0020, 0x4E56, 0x0001, 0xABCD)),
                (0x20102080, pack_u32(0xFFFFFFFF)),
            ],
            compare_ranges=[(0x20102080, 4)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FDFD0_scans_valid_nv_container_and_updates_ctx",
            symbol="sub_FDFD0",
            ref_rva=0x0FDFD0,
            r0=0x20102800,
            check_r0=False,
            mem_writes=[
                (0x20102900, valid_nv_blob),
                (0x20102800, pack_u32(0, 0x20102900, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)),
            ],
            compare_ranges=[(0x20102800, 0x34)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FE4D4_finds_sorted_wnvv_node_and_prev_link",
            symbol="sub_FE4D4",
            ref_rva=0x0FE4D4,
            r0=0x080C,
            r1=0x20102210,
            mem_writes=[
                (0x200D030C + 0x30, pack_u32(0x20102300)),
                (0x20102300, struct.pack("<IHHI", 0x20102320, 0x0105, 4, 0x20102400)),
                (0x20102320, struct.pack("<IHHI", 0x00000000, 0x080C, 12, 0x20102420)),
                (0x20102210, pack_u32(0)),
            ],
            compare_ranges=[(0x20102210, 4)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FE504_wnvv_sorted_list_lookup",
            symbol="sub_FE504",
            ref_rva=0x0FE504,
            r0=0x080C,
            r1=0x20102200,
            mem_writes=[
                (0x200D030C + 0x30, pack_u32(0x20102300)),
                (0x20102300, struct.pack("<IHHI", 0x20102320, 0x0105, 4, 0x20102400)),
                (0x20102320, struct.pack("<IHHI", 0x00000000, 0x080C, 12, 0x20102420)),
                (0x20102200, b"\x00\x00\x00\x00"),
            ],
            compare_ranges=[(0x20102200, 2)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FE2B0_parses_wnvv_1b_3b_16b_tags_and_applies_0x9f_cal_trim",
            symbol="sub_FE2B0",
            ref_rva=0x0FE2B0,
            r0=0x20102A00,
            r1=0,
            r2=0x20102A00,
            r3=22,
            stack_args=[0],
            check_r0=False,
            mem_writes=[
                (0x200D030C + 0x30, pack_u32(0)),
                (0x2010FFE0, pack_u32(0x20110000)),
                (0x4246E008, pack_u32(0x80000003)),
                (
                    0x20102A00,
                    b"NVHDR888"
                    + bytes([0x5A, 0x12])
                    + struct.pack("<H", 0x8000 | (3 << 11) | 0x009F)
                    + pack_u32(0x00000078)
                    + struct.pack("<HH", 0xC000 | 0x080C, 4)
                    + pack_u32(0xCAFEBABE),
                ),
            ],
            compare_ranges=[
                (0x200D030C + 0x30, 4),
                (0x20110000, 48),
                (0x4246E008, 4),
            ],
            stubs=[
                StubHook(
                    "rt_malloc",
                    0x186BBC,
                    "rt_malloc",
                    record_args=1,
                    on_call=_bump_alloc_stateless,
                )
            ],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="phy_shell_cmd_sleep_disables_clocks_and_clears_mux",
            symbol="phy_shell_cmd",
            ref_rva=0x0FE120,
            r0=0,
            r1=0x20102C00,
            mem_writes=[
                (0x20102C00, pack_u32(0x20102C20, 0x20102C30)),
                (0x20102C20, b"phy\x00"),
                (0x20102C30, b"sleep\x00"),
                (0x42445028, pack_u32(0xABCD1234)),
                (0x42440400, pack_u32(0x0F, 0, 0x0F, 0, 0x0F, 0, 0x0F, 0, 0x0F)),
            ],
            compare_ranges=[(0x42445028, 4), (0x42440400, 0x24)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="phy_shell_cmd_wake_enables_all_five_phy_clocks",
            symbol="phy_shell_cmd",
            ref_rva=0x0FE120,
            r0=0,
            r1=0x20102C00,
            mem_writes=[
                (0x20102C00, pack_u32(0x20102C20, 0x20102C30)),
                (0x20102C20, b"phy\x00"),
                (0x20102C30, b"wake\x00"),
                (0x42440400, pack_u32(0xF0, 0, 0xF0, 0, 0xF0, 0, 0xF0, 0, 0xF0)),
            ],
            compare_ranges=[(0x42440400, 0x24)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="phy_shell_cmd_uart_programs_aux_pinmux",
            symbol="phy_shell_cmd",
            ref_rva=0x0FE120,
            r0=0,
            r1=0x20102C00,
            mem_writes=[
                (0x20102C00, pack_u32(0x20102C20, 0x20102C30)),
                (0x20102C20, b"phy\x00"),
                (0x20102C30, b"uart\x00"),
                (0x420190BC, pack_u32(0x0000FF77, 0x0000FF77)),
            ],
            compare_ranges=[(0x420190BC, 8)],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="phy_shell_cmd_crash_sends_0x1001d_ipc",
            symbol="phy_shell_cmd",
            ref_rva=0x0FE120,
            r0=0,
            r1=0x20102C00,
            mem_writes=[
                (0x20102C00, pack_u32(0x20102C20, 0x20102C30, 0x20102C40)),
                (0x20102C20, b"phy\x00"),
                (0x20102C30, b"crash\x00"),
                (0x20102C40, b"2\x00"),
            ],
            stubs=[
                StubHook("sub_187A64", 0x187A64, "sub_187A64", ret_r0=2, record_args=3),
                StubHook("sub_23B78", 0x023B78, "sub_23B78", record_args=4),
            ],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="phy_shell_cmd_unknown_subcmd_logs_error_and_returns_minus1",
            symbol="phy_shell_cmd",
            ref_rva=0x0FE120,
            r0=0,
            r1=0x20102C00,
            mem_writes=[
                (0x20102C00, pack_u32(0x20102C20, 0x20102C30)),
                (0x20102C20, b"phy\x00"),
                (0x20102C30, b"badcmd\x00"),
            ],
            stubs=[
                StubHook("sub_189C4", 0x0189C4, "sub_189C4", record_args=0),
                StubHook("sub_1A5F4", 0x01A5F4, "sub_1A5F4", record_args=1),
            ],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="bt_phy_otp_init_end_to_end_real_embedded_nv_replay_and_synthesis",
            symbol="bt_phy_otp_init",
            ref_rva=0x0FD7C0,
            max_insns=1500000,
            mem_writes=install_mock_pwr_vtable()
            + [
                (0x2010FFE0, pack_u32(0x20110000)),
                (0x20024398 + 4, pack_u32(0x20103800)),
                (0x20103800, b"\x00" * 0x240),
                (0x20103800 + 0x04, pack_u32(0x20103B00)),
                (0x20103800 + 0x220, pack_u32(0, 8)),
                (0x20103800 + 0x234, b"\x01"),
                (0x4D28D000, b"\x00" * 0x400),
                (0x4D291000, b"\xFF" * 0x400),
            ],
            compare_ranges=[
                (0x42443000, 4),
                (0x4246E004, 0x78),
                (0x4247A004, 4),
                (0x42019204, 8),
                (0x4D140000, 4),
                (0x4D154048, 4),
                (0x4D28D000, 0x200),
                (0x4D291000, 0x200),
                (0x20103800 + 0x220, 4),
            ],
            stubs=[
                StubHook("sub_1A646", 0x01A646, "sub_1A646", record_args=1),
                StubHook("sub_18A134", 0x18A134, "sub_18A134", record_args=2),
                StubHook(
                    "rt_malloc",
                    0x186BBC,
                    "rt_malloc",
                    record_args=1,
                    on_call=_bump_alloc_stateless,
                ),
                StubHook(
                    "sub_18E930",
                    0x18E930,
                    "sub_18E930",
                    record_args=0,
                    on_call=_otp_empty_stub,
                ),
                StubHook("sub_1880F8", 0x1880F8, "sub_1880F8", ret_r0=0, record_args=2),
                StubHook("sub_141B0", 0x0141B0, "sub_141B0", record_args=0),
                StubHook("sub_1A2A3C", 0x1A2A3C, "sub_1A2A3C", ret_r0=0x55, record_args=1),
                StubHook("sub_1A295C", 0x1A295C, "sub_1A295C", record_args=2),
            ],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FE564_crc8_table_hash",
            symbol="sub_FE564",
            ref_rva=0x0FE564,
            r0=0xFF,
            r1=0x20100000,
            r2=14,
            mem_writes=[(0x20100000, b"T2016-B0-PHYB!")],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FE580_dirty_queue_xor_detector_returns_1",
            symbol="sub_FE580",
            ref_rva=0x0FE580,
            r0=0x20102500,
            mem_writes=[
                (0x20102500 + 0x58, pack_u32(10, 10)),
                (0x20102500 + 0x6C, pack_u32(20, 21)),
                (0x20102500 + 0x80, pack_u32(30, 30)),
            ],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FE580_clean_queue_xor_detector_returns_0",
            symbol="sub_FE580",
            ref_rva=0x0FE580,
            r0=0x20102500,
            mem_writes=[
                (0x20102500 + 0x58, pack_u32(10, 10)),
                (0x20102500 + 0x6C, pack_u32(20, 20)),
                (0x20102500 + 0x80, pack_u32(30, 30)),
            ],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FE59E_ring_peek_0xbc_big_endian",
            symbol="sub_FE59E",
            ref_rva=0x0FE59E,
            r0=0x20102600,
            mem_writes=[
                (0x20102620, bytes([0xAA, 0x34, 0x00, 0xBC, 0x12])),
                (0x20102600, pack_u32(0, 5, 1, 2, 0x20102620)),
            ],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FE59E_ring_peek_non_bc_little_endian",
            symbol="sub_FE59E",
            ref_rva=0x0FE59E,
            r0=0x20102600,
            mem_writes=[
                (0x20102620, bytes([0xAA, 0x34, 0x00, 0x04, 0x12])),
                (0x20102600, pack_u32(0, 5, 1, 2, 0x20102620)),
            ],
        ),
        DiffCase(
            suite="phy_modem_and_crash",
            name="sub_FE7CE_ring_free_space_wrapped_and_unwrapped",
            symbol="sub_FE7CE",
            ref_rva=0x0FE7CE,
            r0=0x20102600,
            mem_writes=[(0x20102600, pack_u32(0, 16, 11, 4, 0x20102620))],
        ),
    ])

    cases.extend(load_phase2_cases())
    return cases


def load_phase2_cases(spec_path: Path | None = None) -> list[DiffCase]:
    if spec_path is None:
        spec_path = (
            Path(__file__).resolve().parents[1] / "verification" / "phase2_cases.json"
        )
    if not spec_path.is_file():
        return []
    doc = json.loads(spec_path.read_text(encoding="utf-8"))
    presets: dict[str, list[tuple[int, bytes]]] = {}
    for pname, writes in doc.get("presets", {}).items():
        presets[pname] = [
            (int(str(addr), 0), bytes.fromhex(hex_b)) for addr, hex_b in writes
        ]

    out: list[DiffCase] = []
    for sym_spec in doc.get("symbols", []):
        suite = str(sym_spec["suite"])
        symbol = str(sym_spec["symbol"])
        ref_rva = int(str(sym_spec["rva"]), 0)
        preset_name = sym_spec.get("preset")
        base_writes = list(presets.get(preset_name, [])) if preset_name else []
        for addr, hex_b in sym_spec.get("mem", []):
            base_writes.append((int(str(addr), 0), bytes.fromhex(hex_b)))

        stubs = [
            StubHook(
                name=str(st["name"]),
                ref_addr=int(str(st["ref_addr"]), 0),
                elf_sym=str(st.get("elf_sym", st["name"])),
                ret_r0=int(str(st.get("ret_r0", 0)), 0),
                record_args=int(st.get("record_args", 2)),
                cstr_args=tuple(int(x) for x in st.get("cstr_args", [])),
                stop_on_call=bool(st.get("stop_on_call", False)),
            )
            for st in sym_spec.get("stubs", [])
        ]
        cmp_ranges = [
            (int(str(a), 0), int(ln)) for a, ln in sym_spec.get("compare_ranges", [])
        ]
        check_r0 = bool(sym_spec.get("check_r0", True))
        check_r1 = bool(sym_spec.get("check_r1", False))
        check_s0 = bool(sym_spec.get("check_s0", False))
        check_d0 = bool(sym_spec.get("check_d0", False))
        check_s_regs = tuple(int(x) for x in sym_spec.get("check_s_regs", []))
        check_d_regs = tuple(int(x) for x in sym_spec.get("check_d_regs", []))
        check_special = bool(sym_spec.get("check_special_regs", False))
        check_all_writes = bool(sym_spec.get("check_all_writes", True))
        check_mmio_trace = bool(sym_spec.get("check_mmio_trace", True))
        max_insns = int(sym_spec.get("max_insns", 100000))

        for v_idx, vec in enumerate(sym_spec.get("vectors", [])):
            v_writes = list(base_writes)
            for addr, hex_b in vec.get("mem", []):
                v_writes.append((int(str(addr), 0), bytes.fromhex(hex_b)))
            s_regs = {
                int(k): int(str(v), 0) for k, v in vec.get("s_regs", {}).items()
            }
            d_regs = {
                int(k): int(str(v), 0) for k, v in vec.get("d_regs", {}).items()
            }
            mmio_seq = {
                int(str(k), 0): [int(str(x), 0) for x in vs]
                for k, vs in vec.get("mmio_read_seq", {}).items()
            }
            v_name = str(vec.get("name") or f"{symbol}_v{v_idx}")
            out.append(
                DiffCase(
                    suite=suite,
                    name=v_name,
                    symbol=symbol,
                    ref_rva=ref_rva,
                    r0=int(str(vec.get("r0", 0)), 0),
                    r1=int(str(vec.get("r1", 0)), 0),
                    r2=int(str(vec.get("r2", 0)), 0),
                    r3=int(str(vec.get("r3", 0)), 0),
                    s0_bits=(
                        int(str(vec["s0_bits"]), 0)
                        if "s0_bits" in vec and vec["s0_bits"] is not None
                        else None
                    ),
                    d0_bits=(
                        int(str(vec["d0_bits"]), 0)
                        if "d0_bits" in vec and vec["d0_bits"] is not None
                        else None
                    ),
                    s_regs=s_regs,
                    d_regs=d_regs,
                    primask=int(vec.get("primask", 0)),
                    basepri=int(vec.get("basepri", 0)),
                    max_insns=int(vec.get("max_insns", max_insns)),
                    check_r0=check_r0,
                    check_r1=check_r1,
                    check_s0=check_s0,
                    check_d0=check_d0,
                    check_s_regs=check_s_regs,
                    check_d_regs=check_d_regs,
                    check_special_regs=check_special,
                    check_all_writes=check_all_writes,
                    check_mmio_trace=check_mmio_trace,
                    mem_writes=v_writes,
                    compare_ranges=cmp_ranges,
                    stubs=stubs,
                    stack_args=[int(str(x), 0) for x in vec.get("stack_args", [])],
                    mmio_read_seq=mmio_seq,
                )
            )
    return out


def compare_outcomes(
    case: DiffCase,
    ref_out: dict[str, Any],
    elf_out: dict[str, Any],
    ref_vm: FastVM | None = None,
    elf_vm: FastVM | None = None,
) -> list[str]:
    diffs: list[str] = []

    if not ref_out["early_stop"] and not elf_out["early_stop"]:
        if case.check_r0 and ref_out["r0"] != elf_out["r0"]:
            diffs.append(f"r0 ref=0x{ref_out['r0']:08x} != elf=0x{elf_out['r0']:08x}")
        if case.check_r1 and ref_out["r1"] != elf_out["r1"]:
            diffs.append(f"r1 ref=0x{ref_out['r1']:08x} != elf=0x{elf_out['r1']:08x}")
        if case.check_s0 and ref_out["s0"] != elf_out["s0"]:
            diffs.append(f"s0 ref=0x{ref_out['s0']:08x} != elf=0x{elf_out['s0']:08x}")
        if case.check_d0 and ref_out["d0"] != elf_out["d0"]:
            diffs.append(f"d0 ref=0x{ref_out['d0']:016x} != elf=0x{elf_out['d0']:016x}")
        for s_idx in case.check_s_regs:
            rs = ref_out.get("s_regs", {}).get(s_idx)
            es = elf_out.get("s_regs", {}).get(s_idx)
            if rs != es:
                diffs.append(f"s{s_idx} ref={rs} != elf={es}")
        for d_idx in case.check_d_regs:
            rd = ref_out.get("d_regs", {}).get(d_idx)
            ed = elf_out.get("d_regs", {}).get(d_idx)
            if rd != ed:
                diffs.append(f"d{d_idx} ref={rd} != elf={ed}")
        if not elf_out["callee_preserved"]:
            diffs.append("ELF clobbered callee-saved register(s) r4..r11 / s16..s31")

    if case.check_special_regs:
        if ref_out["primask"] != elf_out["primask"]:
            diffs.append(f"PRIMASK ref={ref_out['primask']} != elf={elf_out['primask']}")
        if ref_out["basepri"] != elf_out["basepri"]:
            diffs.append(
                f"BASEPRI ref=0x{ref_out['basepri']:x} != elf=0x{elf_out['basepri']:x}"
            )

    if ref_out.get("reached_stop_pc", True) != elf_out.get("reached_stop_pc", True):
        diffs.append(
            f"reached_stop_pc ref={ref_out.get('reached_stop_pc')} != elf={elf_out.get('reached_stop_pc')}"
        )

    if ref_out["trace"] != elf_out["trace"]:
        diffs.append(f"stub trace ref={ref_out['trace']} != elf={elf_out['trace']}")

    if case.check_mmio_trace:
        rm = ref_out.get("mmio_trace", [])
        em = elf_out.get("mmio_trace", [])
        if rm != em:
            diffs.append(f"mmio_trace ref={rm[:8]} != elf={em[:8]}")

    for idx, (addr, length) in enumerate(case.compare_ranges):
        rb = ref_out["mem"][idx]
        eb = elf_out["mem"][idx]
        if rb != eb:
            for off in range(length):
                if rb[off] != eb[off]:
                    diffs.append(
                        f"mem[0x{addr + off:08x}] ref=0x{rb[off]:02x} != elf=0x{eb[off]:02x}"
                    )
                    break

    if case.check_all_writes:
        rwl: dict[int, bytes] = ref_out.get("written_lines", {})
        ewl: dict[int, bytes] = elf_out.get("written_lines", {})
        all_chunks = sorted(set(rwl.keys()) | set(ewl.keys()))
        for ca in all_chunks:
            rb = rwl.get(ca)
            eb = ewl.get(ca)
            if rb is None and ref_vm is not None:
                rb = ref_vm.base_chunk_map.get(ca)
            if eb is None and elf_vm is not None:
                eb = elf_vm.base_chunk_map.get(ca)
            if rb != eb:
                if rb is not None and eb is not None and len(rb) == len(eb):
                    for off in range(len(rb)):
                        if rb[off] != eb[off]:
                            diffs.append(
                                f"write_mem[0x{ca + off:08x}] ref=0x{rb[off]:02x} != elf=0x{eb[off]:02x}"
                            )
                            break
                else:
                    diffs.append(f"write_chunk[0x{ca:08x}] mismatch")
                break

    return diffs


_WORKER_STATE: dict[str, Any] = {}


def _init_worker(elf_path_str: str, ref_path_str: str) -> None:
    elf_path = Path(elf_path_str)
    ref_path = Path(ref_path_str)
    raw = ref_path.read_bytes()
    elf_segs, elf_syms = load_elf_segments_and_symbols(elf_path)
    cases = build_cases()
    ref_vm = FastVM(False, raw, elf_segs, elf_syms)
    elf_vm = FastVM(True, raw, elf_segs, elf_syms)
    _WORKER_STATE["cases"] = cases
    _WORKER_STATE["elf_syms"] = elf_syms
    _WORKER_STATE["ref_vm"] = ref_vm
    _WORKER_STATE["elf_vm"] = elf_vm


def _run_diff_batch(bounds: tuple[int, int]) -> list[tuple[int, dict[str, Any]]]:
    lo, hi = bounds
    cases: list[DiffCase] = _WORKER_STATE["cases"]
    elf_syms: dict[str, int] = _WORKER_STATE["elf_syms"]
    ref_vm: FastVM = _WORKER_STATE["ref_vm"]
    elf_vm: FastVM = _WORKER_STATE["elf_vm"]
    results: list[tuple[int, dict[str, Any]]] = []

    for idx in range(lo, hi):
        case = cases[idx]
        if case.symbol not in elf_syms:
            err = f"Missing symbol '{case.symbol}' in ELF"
            results.append(
                (
                    idx,
                    {
                        "suite": case.suite,
                        "name": case.name,
                        "symbol": case.symbol,
                        "ref_rva": f"0x{case.ref_rva:06x}",
                        "passed": False,
                        "diffs": [err],
                    },
                )
            )
            continue

        try:
            ref_out = ref_vm.run_case(case)
            elf_out = elf_vm.run_case(case)
            diffs = compare_outcomes(case, ref_out, elf_out, ref_vm, elf_vm)
        except Exception as exc:  # noqa: BLE001
            diffs = [f"Unicorn execution error: {exc}"]
            ref_out = {"r0": None}

        results.append(
            (
                idx,
                {
                    "suite": case.suite,
                    "name": case.name,
                    "symbol": case.symbol,
                    "ref_rva": f"0x{case.ref_rva:06x}",
                    "elf_addr": f"0x{elf_syms[case.symbol]:06x}",
                    "r0": (
                        f"0x{ref_out['r0']:08x}"
                        if isinstance(ref_out.get("r0"), int)
                        else None
                    ),
                    "passed": len(diffs) == 0,
                    "diffs": diffs,
                },
            )
        )
    return results


def _run_mutation_symbol_batch(
    item: tuple[list[tuple[int, list[int]]], dict[str, bytes]],
) -> dict[int, list[dict[str, bool]]]:
    sym_groups, stubs_dict = item
    cases: list[DiffCase] = _WORKER_STATE["cases"]
    elf_syms: dict[str, int] = _WORKER_STATE["elf_syms"]
    ref_vm: FastVM = _WORKER_STATE["ref_vm"]
    elf_vm: FastVM = _WORKER_STATE["elf_vm"]
    out: dict[int, list[dict[str, bool]]] = {}

    for rva, case_indices in sym_groups:
        killed_so_far = {k: False for k in stubs_dict}
        dets_for_rva: list[dict[str, bool]] = []
        for c_idx in case_indices:
            if all(killed_so_far.values()):
                break
            case = cases[c_idx]
            if case.symbol not in elf_syms:
                continue
            target_addr = elf_syms[case.symbol] & ~1
            ref_out = ref_vm.run_case(case)
            det: dict[str, bool] = {}
            for stub_name, stub_bytes in stubs_dict.items():
                if killed_so_far[stub_name]:
                    det[stub_name] = True
                    continue
                try:
                    mut_out = elf_vm.run_case(
                        case, patch_bytes=(target_addr, stub_bytes)
                    )
                    killed = (
                        len(compare_outcomes(case, ref_out, mut_out, ref_vm, elf_vm))
                        > 0
                    )
                except Exception:  # noqa: BLE001
                    killed = True
                det[stub_name] = killed
                if killed:
                    killed_so_far[stub_name] = True
            dets_for_rva.append(det)
        out[rva] = dets_for_rva
    return out


def audit_parallel_stub_mutations(
    elf_path: Path,
    ref_path: Path,
    stubs_dict: dict[str, bytes],
    workers: int | None = None,
) -> dict[int, list[dict[str, bool]]]:
    cases = build_cases()
    by_rva: dict[int, list[int]] = {}
    for idx, case in enumerate(cases):
        by_rva.setdefault(case.ref_rva, []).append(idx)

    groups = sorted(by_rva.items())
    if not groups:
        return {}

    n_workers = workers or min(8, max(1, os.cpu_count() or 4), len(groups))
    if len(groups) < 32 or n_workers <= 1:
        _init_worker(str(elf_path), str(ref_path))
        return _run_mutation_symbol_batch((groups, stubs_dict))

    chunk_sz = max(16, (len(groups) + n_workers * 2 - 1) // (n_workers * 2))
    batches = [
        (groups[i : i + chunk_sz], stubs_dict) for i in range(0, len(groups), chunk_sz)
    ]
    merged: dict[int, list[dict[str, bool]]] = {}
    with ProcessPoolExecutor(
        max_workers=n_workers,
        initializer=_init_worker,
        initargs=(str(elf_path), str(ref_path)),
    ) as pool:
        for part in pool.map(_run_mutation_symbol_batch, batches):
            merged.update(part)
    return merged


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--elf", required=True, type=Path, help="Path to linked rkof.elf")
    ap.add_argument("--reference", required=True, type=Path, help="Path to raw rkof binary")
    ap.add_argument("--output", required=True, type=Path, help="Path to output JSON report")
    ap.add_argument("--workers", type=int, default=0, help="Worker process count (0=auto)")
    args = ap.parse_args()

    cases = build_cases()
    unique_symbols = sorted({c.symbol for c in cases})

    n_workers = (
        args.workers
        if args.workers > 0
        else min(8, max(1, os.cpu_count() or 4), max(1, len(cases) // 32))
    )

    ordered_results: list[dict[str, Any]] = [{} for _ in range(len(cases))]
    if len(cases) < 64 or n_workers <= 1:
        _init_worker(str(args.elf), str(args.reference))
        for idx, rec in _run_diff_batch((0, len(cases))):
            ordered_results[idx] = rec
    else:
        chunk_sz = max(32, (len(cases) + n_workers * 2 - 1) // (n_workers * 2))
        batches = [
            (i, min(i + chunk_sz, len(cases))) for i in range(0, len(cases), chunk_sz)
        ]
        with ProcessPoolExecutor(
            max_workers=n_workers,
            initializer=_init_worker,
            initargs=(str(args.elf), str(args.reference)),
        ) as pool:
            for batch_res in pool.map(_run_diff_batch, batches):
                for idx, rec in batch_res:
                    ordered_results[idx] = rec

    suite_map: dict[str, list[dict[str, Any]]] = {}
    failures: list[dict[str, Any]] = []
    for rec in ordered_results:
        sname = rec["suite"]
        rec_clean = {k: v for k, v in rec.items() if k != "suite"}
        suite_map.setdefault(sname, []).append(rec_clean)
        if not rec["passed"]:
            failures.append(
                {"suite": sname, "case": rec["name"], "diffs": rec["diffs"]}
            )

    suites_out = [
        {
            "suite": s_name,
            "case_count": len(s_cases),
            "passed_count": sum(1 for c in s_cases if c["passed"]),
            "passed": all(c["passed"] for c in s_cases),
            "cases": s_cases,
        }
        for s_name, s_cases in suite_map.items()
    ]

    report = {
        "target": "rkof",
        "architecture": "ARMv7E-M (Cortex-M4F + VFPv4-SP-D16)",
        "elf": str(args.elf),
        "reference": str(args.reference),
        "case_count": len(cases),
        "unique_symbol_count": len(unique_symbols),
        "divergence_count": len(failures),
        "passed": len(failures) == 0 and len(cases) > 0,
        "suites": suites_out,
        "failures": failures,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if failures:
        for fail in failures[:50]:
            print(
                f"[FAIL] {fail['suite']}::{fail['case']}: {'; '.join(fail['diffs'])}",
                file=sys.stderr,
            )
        if len(failures) > 50:
            print(
                f"[FAIL] ... and {len(failures) - 50} additional failures",
                file=sys.stderr,
            )
        return 1

    print(
        f"[OK] arm_semantics: {len(cases)}/{len(cases)} Unicorn Cortex-M4F differential "
        f"cases ({len(unique_symbols)} unique symbols) passed across {len(suites_out)} suites (0 divergences)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
