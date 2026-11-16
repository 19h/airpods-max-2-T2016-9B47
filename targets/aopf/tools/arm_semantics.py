#!/usr/bin/env python3
"""Unicorn ARMv7E-M Thumb-2 Cortex-M4F parallel differential & stub-mutation harness for targets/aopf/.

Executes reconstructed C11 functions compiled into aopf.elf side-by-side against
the original firmware machine code in ../../../aopf at each function's original RVA,
comparing return registers (r0 / r0:r1), FPU registers (s0..s3), output memory
buffers, global SRAM mutations (RAM_DATA + RAM_BSS), CPU interrupt-mask registers
(PRIMASK/BASEPRI), virtual/callback dispatch traces (vt_calls), pending_c callee
traces (stub_calls), and full ordered MMIO read+write traces.

When executing aopf.elf (is_elf=True), ROM_TEXT (0x0..0x64000) is initialized to
Thumb `udf #255` (0xDEFF) traps rather than raw firmware bytes, proving that no
`reconstructed_c` function executes unported raw ROM_TEXT helpers. Also verifies
that every claimed `reconstructed_c` function kills all three constant-return stub
mutations (bx lr, movs r0,#0;bx lr, movs r0,#1;bx lr), except verified literal
2/4-byte raw firmware stubs documented in `literal_raw_stub_identities`.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ProcessPoolExecutor
import json
import multiprocessing as mp
import os
from pathlib import Path
import re
import struct
import sys
from typing import Any, Callable

from elftools.elf.elffile import ELFFile
from unicorn import (
    Uc,
    UC_ARCH_ARM,
    UC_HOOK_CODE,
    UC_HOOK_MEM_READ,
    UC_HOOK_MEM_WRITE,
    UC_MEM_READ,
    UC_MEM_WRITE,
    UC_MODE_MCLASS,
    UC_MODE_THUMB,
    UC_PROT_ALL,
)
from unicorn.arm_const import (
    UC_ARM_REG_BASEPRI,
    UC_ARM_REG_C1_C0_2,
    UC_ARM_REG_FPEXC,
    UC_ARM_REG_FPSCR,
    UC_ARM_REG_IPSR,
    UC_ARM_REG_LR,
    UC_ARM_REG_PC,
    UC_ARM_REG_PRIMASK,
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_R3,
    UC_ARM_REG_S0,
    UC_ARM_REG_S1,
    UC_ARM_REG_S2,
    UC_ARM_REG_S3,
    UC_ARM_REG_S4,
    UC_ARM_REG_S5,
    UC_ARM_REG_S6,
    UC_ARM_REG_S7,
    UC_ARM_REG_S8,
    UC_ARM_REG_S9,
    UC_ARM_REG_S10,
    UC_ARM_REG_S11,
    UC_ARM_REG_S12,
    UC_ARM_REG_S13,
    UC_ARM_REG_S14,
    UC_ARM_REG_S15,
    UC_ARM_REG_SP,
    UC_ARM_REG_XPSR,
)

ROM_BASE = 0x00000000
ROM_MAP_SIZE = 0x00078000
VT_STUB_BASE = 0x00077C00
VT_STUB_COUNT = 128
VT_STUB_END = VT_STUB_BASE + VT_STUB_COUNT * 4  # 0x00077E00
CB_STUB_ADDR = 0x00077FE0
STOP_ADDR = 0x00077FF0

SRAM_BASE = 0x20800000
SRAM_MAP_SIZE = 0x000A0000
SCRATCH_BASE = 0x20880000
SCRATCH_SIZE = 0x00008000  # 32 KiB structured scratch arena (0x20880000..0x20888000)
DEFAULT_VT_ARRAY = 0x20887000
INIT_SP = 0x2089A800

FPU_S_REGS = (
    UC_ARM_REG_S0, UC_ARM_REG_S1, UC_ARM_REG_S2, UC_ARM_REG_S3,
    UC_ARM_REG_S4, UC_ARM_REG_S5, UC_ARM_REG_S6, UC_ARM_REG_S7,
    UC_ARM_REG_S8, UC_ARM_REG_S9, UC_ARM_REG_S10, UC_ARM_REG_S11,
    UC_ARM_REG_S12, UC_ARM_REG_S13, UC_ARM_REG_S14, UC_ARM_REG_S15,
)

GLOBAL_DIFF_SPANS: tuple[tuple[int, int], ...] = (
    (0x20800000, 0x00080000),  # RAM_DATA + RAM_BSS below SCRATCH_BASE
    (0x20888000, 0x00011490),  # RAM_BSS between SCRATCH_END and PSP stack
    (0x2089A890, 0x00000D80),  # RAM_BSS between PSP top and MSP bottom
    (0x2089BA10, 0x000004C8),  # RAM_BSS above MSP top .. 0x2089BED8
)

MMIO_RANGES = [
    (0x4002C000, 0x00004000),
    (0x42000000, 0x00040000),
    (0x42200000, 0x00030000),
    (0x42440000, 0x00010000),
    (0x440C4000, 0x00008000),
    (0xE0001000, 0x00001000),
    (0xE000E000, 0x00001000),
]

STUB_MUTATIONS: dict[str, bytes] = {
    "bx_lr": b"\x70\x47",
    "ret_0": b"\x00\x20\x70\x47",
    "ret_1": b"\x01\x20\x70\x47",
}

DEFAULT_PENDING_STUBS: list[dict[str, Any]] = [
    {"rva": "0x6cd4",  "symbol": "rtk_stack_chk_fail",            "arity": 0},
    {"rva": "0x1daa8", "symbol": "rtk_assert_fail_hex",           "arity": 3, "kind": "assert_file_line"},
    {"rva": "0x2031c", "symbol": "pmgr_invalid_powerstate_panic", "arity": 2},
    {"rva": "0x20330", "symbol": "rtk_panic",                     "arity": 1, "kind": "fmt0"},
    {"rva": "0x35460", "symbol": "laguna_assert",                 "arity": 3, "kind": "assert_file_line"},
    {"rva": "0x35464", "symbol": "laguna_assert_fail",            "arity": 2, "kind": "assert_file_line"},
    {"rva": "0x4f18c", "symbol": "rtk_heap_lock_release",         "arity": 1},
    {"rva": "0x4f198", "symbol": "rtk_malloc",                    "arity": 1, "ret": 0x20886800},
    {"rva": "0x4f1c0", "symbol": "rtk_realloc",                   "arity": 2, "ret": 0x20886800},
    {"rva": "0x4f2f0", "symbol": "rtk_calloc",                    "arity": 2, "ret": 0x20886800},
    {"rva": "0x4f330", "symbol": "rtk_memalign",                  "arity": 2, "ret": 0x20886800},
    {"rva": "0x4f3a0", "symbol": "rtk_free",                      "arity": 1},
    {"rva": "0x4f3f8", "symbol": "rtk_assert_fail",               "arity": 3, "kind": "assert_file_line"},
    {"rva": "0x4f40c", "symbol": "rtk_assert_laguna_ok",          "arity": 1},
    {"rva": "0x4f4b0", "symbol": "rtk_printf",                    "arity": 1, "kind": "fmt0"},
    {"rva": "0x4f4d0", "symbol": "rtk_vprintf",                   "arity": 1, "kind": "fmt0"},
    {"rva": "0x4fa96", "symbol": "rtk_snprintf",                  "arity": 3},
    {"rva": "0x4fab0", "symbol": "rtk_vsnprintf",                 "arity": 3},
    {"rva": "0x4fb10", "symbol": "rtk_vsnprintf_clamped",         "arity": 3},
    {"rva": "0x4fb28", "symbol": "rtk_call_once",                 "arity": 2},
    {"rva": "0x50324", "symbol": "rtk_fastlock_unlock_if_zero",   "arity": 2},
    {"rva": "0x5032c", "symbol": "rtk_fastlock_unlock",           "arity": 1},
    {"rva": "0x50404", "symbol": "rtk_fastlock_lock",             "arity": 1},
    {"rva": "0x514f8", "symbol": "rtk_unimplemented_method",      "arity": 0},
    {"rva": "0x5152c", "symbol": "rtk_delay_us",                  "arity": 2},
    {"rva": "0x5ac7c", "symbol": "rtk_vm_entry_alloc",            "arity": 2, "ret": 0x20886800},
    {"rva": "0x5de48", "symbol": "rtk_cxa_guard_acquire",         "arity": 1, "ret": 1},
    {"rva": "0x5de88", "symbol": "rtk_cxa_guard_unlock",          "arity": 0},
    {"rva": "0x5dea4", "symbol": "rtk_cxa_guard_release",         "arity": 1},
    {"rva": "0x5deac", "symbol": "rtk_cxa_pure_virtual",          "arity": 0},
    {"rva": "0x5debc", "symbol": "rtk_cxx_operator_new",          "arity": 1, "ret": 0x20886800},
    {"rva": "0x5ded4", "symbol": "rtk_cxx_malloc_thunk",          "arity": 1, "ret": 0x20886800},
    {"rva": "0x5dedc", "symbol": "rtk_cxx_free_thunk",            "arity": 1},
    {"rva": "0x5dee0", "symbol": "rtk_cxx_operator_delete",       "arity": 1},
    {"rva": "0x5dee8", "symbol": "rtk_cxx_operator_new_array",    "arity": 1, "ret": 0x20886800},
    {"rva": "0x5df04", "symbol": "rtk_cxx_operator_delete_array", "arity": 1},
    {"rva": "0x5ef16", "symbol": "rtk_heap_free_block",           "arity": 2},
    {"rva": "0x60380", "symbol": "rtk_assert_line_fail",          "arity": 2, "kind": "assert_file_line"},
]

PROV_RE = re.compile(
    r"/\*\s*@provenance\s+RVA\s+(0x[0-9a-fA-F]+)\s+([^\*\n]+?)\s*\*/"
)
IDENT_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
SKIP_IDENTS = {"__attribute__", "section", "aligned"}


def _f32_to_u32(val: int | float) -> int:
    if isinstance(val, float):
        return struct.unpack("<I", struct.pack("<f", val))[0]
    return int(val) & 0xFFFFFFFF


def _is_f32_nan(bits: int) -> bool:
    return (bits & 0x7F800000) == 0x7F800000 and (bits & 0x007FFFFF) != 0


def discover_provenance_funcs(target_dir: Path, elf_path: Path) -> tuple[dict[str, tuple[int, int]], dict[str, int]]:
    with open(elf_path, "rb") as f:
        elf = ELFFile(f)
        symtab = elf.get_section_by_name(".symtab")
        elf_syms = {
            s.name: (s["st_value"] & ~1)
            for s in symtab.iter_symbols()
            if s["st_info"]["type"] == "STT_FUNC" and s["st_value"] != 0
        }

    func_map: dict[str, tuple[int, int]] = {}
    for c_path in sorted((target_dir / "src").rglob("*.c")):
        if c_path.name == "vt_trampolines.c":
            continue
        txt = c_path.read_text(encoding="utf-8")
        for m in PROV_RE.finditer(txt):
            rva_int = int(m.group(1), 16)
            tail = txt[m.end() : m.end() + 350]
            for im in IDENT_RE.finditer(tail):
                cand = im.group(1)
                if cand not in SKIP_IDENTS:
                    if cand in elf_syms:
                        func_map[cand] = (rva_int, elf_syms[cand])
                    break
    return func_map, elf_syms


class CaseSpec:
    def __init__(
        self,
        name: str,
        c_symbol: str,
        args: tuple[int, ...] = (),
        stack_words: tuple[int, ...] = (),
        ret64: bool = False,
        ignore_r0: bool = False,
        setup_fn: Callable[[Uc], None] | None = None,
        extra_mem_spans: list[tuple[int, int]] | None = None,
        check_sysregs: bool = False,
        *,
        fpu_s: tuple[int | float, ...] = (),
        check_fpu: int = 0,
        fpu_ulp_tol: int = 0,
        mem_writes: list[tuple[int, bytes]] | None = None,
        mmio_reads: dict[int, list[int]] | None = None,
        vt_rets: dict[int, int] | None = None,
        vt_arities: dict[int, int] | None = None,
        vt_fpu_rets: dict[int, int] | None = None,
        vt_out_u32: dict[str, int] | None = None,
        seed_vt_array: bool = False,
        stub_pending: bool = False,
        check_globals: bool = False,
        primask: int = 0,
        basepri: int = 0,
        ipsr: int = 0,
        max_insns: int = 100000,
    ) -> None:
        self.name = name
        self.c_symbol = c_symbol
        self.args = args
        self.stack_words = stack_words
        self.ret64 = ret64
        self.ignore_r0 = ignore_r0
        self.setup_fn = setup_fn
        self.extra_mem_spans = extra_mem_spans or []
        self.check_sysregs = check_sysregs
        self.fpu_s = tuple(_f32_to_u32(v) for v in fpu_s)
        self.check_fpu = check_fpu
        self.fpu_ulp_tol = fpu_ulp_tol
        self.mem_writes = mem_writes or []
        self.mmio_reads = mmio_reads or {}
        self.vt_rets = vt_rets or {}
        self.vt_arities = vt_arities or {}
        self.vt_fpu_rets = vt_fpu_rets or {}
        self.vt_out_u32 = vt_out_u32 or {}
        self.seed_vt_array = seed_vt_array
        self.stub_pending = stub_pending
        self.check_globals = check_globals
        self.primask = primask
        self.basepri = basepri
        self.ipsr = ipsr
        self.max_insns = max_insns


def _parse_int(v: int | str) -> int:
    return int(v, 0) if isinstance(v, str) else int(v)


def load_json_specs(spec_paths: list[Path]) -> tuple[list[dict[str, Any]], list[CaseSpec]]:
    pending_stubs: list[dict[str, Any]] = list(DEFAULT_PENDING_STUBS)
    seen_stubs: set[int] = {_parse_int(p["rva"]) for p in pending_stubs}
    specs: list[CaseSpec] = []

    for spath in spec_paths:
        if not spath.is_file():
            continue
        doc = json.loads(spath.read_text(encoding="utf-8"))
        for pst in doc.get("pending_stubs", []):
            rva = _parse_int(pst["rva"])
            if rva not in seen_stubs:
                seen_stubs.add(rva)
                pending_stubs.append(pst)
        for item in doc.get("cases", []):
            writes: list[tuple[int, bytes]] = []
            for w in item.get("writes", []):
                w_addr = _parse_int(w[0])
                w_val = w[1]
                if isinstance(w_val, str):
                    writes.append((w_addr, bytes.fromhex(w_val)))
                elif isinstance(w_val, int):
                    sz = int(w[2]) if len(w) > 2 else 4
                    writes.append((w_addr, (w_val & ((1 << (8 * sz)) - 1)).to_bytes(sz, "little")))
                elif isinstance(w_val, list):
                    writes.append((w_addr, struct.pack(f"<{len(w_val)}I", *(int(x) & 0xFFFFFFFF for x in w_val))))
            mmio_reads = {
                _parse_int(k): [_parse_int(x) for x in vals]
                for k, vals in item.get("mmio_reads", {}).items()
            }
            vt_rets = {int(k): _parse_int(v) for k, v in item.get("vt_rets", {}).items()}
            vt_arities = {int(k): int(v) for k, v in item.get("vt_arities", {}).items()}
            vt_fpu_rets = {int(k): _f32_to_u32(v) for k, v in item.get("vt_fpu_rets", {}).items()}
            vt_out_u32 = {str(k): _parse_int(v) for k, v in item.get("vt_out_u32", {}).items()}
            extra_spans = [(_parse_int(a), int(sz)) for a, sz in item.get("extra_mem_spans", [])]

            specs.append(
                CaseSpec(
                    name=item["case"],
                    c_symbol=item["c_symbol"],
                    args=tuple(_parse_int(a) for a in item.get("args", [])),
                    stack_words=tuple(_parse_int(w) for w in item.get("stack_words", [])),
                    ret64=bool(item.get("ret64", False)),
                    ignore_r0=bool(item.get("ignore_r0", False)),
                    extra_mem_spans=extra_spans,
                    check_sysregs=bool(item.get("check_sysregs", False)),
                    fpu_s=tuple(item.get("fpu_s", [])),
                    check_fpu=int(item.get("check_fpu", 0)),
                    fpu_ulp_tol=int(item.get("fpu_ulp_tol", 0)),
                    mem_writes=writes,
                    mmio_reads=mmio_reads,
                    vt_rets=vt_rets,
                    vt_arities=vt_arities,
                    vt_fpu_rets=vt_fpu_rets,
                    vt_out_u32=vt_out_u32,
                    seed_vt_array=bool(item.get("seed_vt_array", False)),
                    stub_pending=bool(item.get("stub_pending", False)),
                    check_globals=bool(item.get("check_globals", True)),
                    primask=int(item.get("primask", 0)),
                    basepri=int(item.get("basepri", 0)),
                    ipsr=int(item.get("ipsr", 0)),
                    max_insns=int(item.get("max_insns", 100000)),
                )
            )
    return pending_stubs, specs


class ReusableArmMachine:
    """Single reusable Unicorn Cortex-M4F VM (either raw firmware or UDF-poisoned ELF)."""

    def __init__(
        self,
        is_elf: bool,
        raw_bytes: bytes,
        elf_alloc_sections: list[tuple[int, bytes]],
        elf_syms: dict[str, int],
        pending_stubs: list[dict[str, Any]],
    ) -> None:
        self.is_elf = is_elf
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.uc.mem_map(ROM_BASE, ROM_MAP_SIZE, UC_PROT_ALL)
        self.uc.mem_map(SRAM_BASE, SRAM_MAP_SIZE, UC_PROT_ALL)
        for mbase, msize in MMIO_RANGES:
            self.uc.mem_map(mbase, msize, UC_PROT_ALL)

        self.base_ram_data = raw_bytes[0x77B20 : 0x77B20 + 0x1A0B0]
        self.zero_ram_bss = b"\x00" * (SRAM_MAP_SIZE - 0x1A0B0)
        self.default_vt_blob = struct.pack(
            f"<{VT_STUB_COUNT}I",
            *((VT_STUB_BASE + 4 * k) | 1 for k in range(VT_STUB_COUNT)),
        )

        if is_elf:
            # Poison ROM_TEXT [0x0..0x64000) with Thumb `udf #255` (0xDEFF) so any accidental
            # branch into unported raw ROM_TEXT immediately faults instead of executing raw code.
            self.uc.mem_write(ROM_BASE, b"\xff\xde" * (0x64000 // 2))
            for vaddr, data in elf_alloc_sections:
                self.uc.mem_write(vaddr, data)
        else:
            self.uc.mem_write(ROM_BASE, raw_bytes[:0x77B14])

        # Populate 128 virtual/callback dispatch stub slots (`bx lr; nop`) in unmapped ROM padding
        self.uc.mem_write(VT_STUB_BASE, b"\x70\x47\x00\xbf" * VT_STUB_COUNT)
        self.uc.mem_write(STOP_ADDR, b"\x00\xbe\x00\xbe")
        cb_code = bytes.fromhex("02490a68521c0a6000207047c01f8820")
        self.uc.mem_write(CB_STUB_ADDR, cb_code)
        self.uc.mem_write(SRAM_BASE, self.base_ram_data)

        self.dirty_sram_pages: set[int] = set()
        self.touched_mmio_words: set[int] = set()
        self.patched_rom: list[tuple[int, bytes]] = []

        self.mmio_trace: list[tuple[str, int, int, int]] = []
        self.mmio_read_plan: dict[int, list[int]] = {}
        self.mmio_read_idx: dict[int, int] = {}

        self.vt_calls: list[tuple[Any, ...]] = []
        self.vt_rets: dict[int, int] = {}
        self.vt_arities: dict[int, int] = {}
        self.vt_fpu_rets: dict[int, int] = {}
        self.vt_out_u32: dict[str, int] = {}

        self.track_stub_calls: bool = False
        self.stub_calls: list[tuple[Any, ...]] = []
        self._stub_by_addr: dict[int, dict[str, Any]] = {}

        for pst in pending_stubs:
            rva = _parse_int(pst["rva"]) & ~1
            sym = str(pst["symbol"])
            hook_addr = (elf_syms.get(sym, 0) & ~1) if is_elf else rva
            if hook_addr == 0 or hook_addr in self._stub_by_addr:
                continue
            self._stub_by_addr[hook_addr] = pst
            self.uc.hook_add(UC_HOOK_CODE, self._on_pending_stub, begin=hook_addr, end=hook_addr)

        self.uc.hook_add(UC_HOOK_CODE, self._on_vt_stub, begin=VT_STUB_BASE, end=VT_STUB_END - 1)
        # Never attach UC_HOOK_MEM_WRITE to SRAM (Unicorn 2.x Thumb-2 IT-block store bug);
        # hook ONLY peripheral MMIO ranges.
        for mbase, msize in MMIO_RANGES:
            self.uc.hook_add(
                UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE,
                self._on_mmio_access,
                begin=mbase,
                end=mbase + msize - 1,
            )

    def _on_sram_write(self, _uc: Uc, _access: int, address: int, size: int, _val: int, _ud: Any) -> None:
        p0 = address & ~0xFF
        p1 = (address + size - 1) & ~0xFF
        self.dirty_sram_pages.add(p0)
        if p1 != p0:
            self.dirty_sram_pages.add(p1)

    def _on_mmio_access(self, uc_inst: Uc, access: int, address: int, size: int, value: int, _ud: Any) -> None:
        self.touched_mmio_words.add(address & ~3)
        mask = (1 << (size * 8)) - 1
        if access == UC_MEM_WRITE:
            self.mmio_trace.append(("W", address, size, value & mask))
        elif access == UC_MEM_READ:
            plan = self.mmio_read_plan.get(address)
            if plan is not None:
                idx = self.mmio_read_idx.get(address, 0)
                chosen = plan[idx] if idx < len(plan) else plan[-1]
                self.mmio_read_idx[address] = idx + 1
                uc_inst.mem_write(address, (chosen & mask).to_bytes(size, "little"))
            cur_val = int.from_bytes(uc_inst.mem_read(address, size), "little") & mask
            self.mmio_trace.append(("R", address, size, cur_val))

    @staticmethod
    def _norm_arg(val: int) -> int | str:
        val &= 0xFFFFFFFF
        if (INIT_SP - 0x4000) <= val <= (INIT_SP + 0x100):
            return "SP_LOCAL"
        return val

    def _on_vt_stub(self, uc_inst: Uc, address: int, _size: int, _ud: Any) -> None:
        offset = address - VT_STUB_BASE
        if (offset & 3) != 0:
            return
        slot = offset >> 2
        regs = [
            uc_inst.reg_read(UC_ARM_REG_R0) & 0xFFFFFFFF,
            uc_inst.reg_read(UC_ARM_REG_R1) & 0xFFFFFFFF,
            uc_inst.reg_read(UC_ARM_REG_R2) & 0xFFFFFFFF,
            uc_inst.reg_read(UC_ARM_REG_R3) & 0xFFFFFFFF,
        ]
        for a_idx, rval in enumerate(regs):
            key = f"{slot}:{a_idx}"
            if key in self.vt_out_u32 and SRAM_BASE <= rval < SRAM_BASE + SRAM_MAP_SIZE - 3:
                uc_inst.mem_write(rval, struct.pack("<I", self.vt_out_u32[key] & 0xFFFFFFFF))
        arity = max(0, min(4, self.vt_arities.get(slot, 4)))
        self.vt_calls.append((slot, *(self._norm_arg(regs[i]) for i in range(arity))))
        uc_inst.reg_write(UC_ARM_REG_R0, self.vt_rets.get(slot, 0) & 0xFFFFFFFF)
        if slot in self.vt_fpu_rets:
            uc_inst.reg_write(UC_ARM_REG_S0, self.vt_fpu_rets[slot] & 0xFFFFFFFF)

    def _read_cstr(self, ptr: int) -> str | None:
        if ptr == 0:
            return None
        try:
            raw_s = bytes(self.uc.mem_read(ptr, 128))
        except Exception:
            return f"PTR_{ptr:#x}"
        nul = raw_s.find(b"\x00")
        if nul >= 0:
            raw_s = raw_s[:nul]
        return raw_s.decode("latin1")

    def _on_pending_stub(self, uc_inst: Uc, address: int, _size: int, _ud: Any) -> None:
        if not self.track_stub_calls:
            return
        pst = self._stub_by_addr.get(address & ~1)
        if pst is None:
            return
        sym = str(pst["symbol"])
        arity = int(pst.get("arity", 0))
        kind = str(pst.get("kind", "args"))
        r_vals = [
            uc_inst.reg_read(UC_ARM_REG_R0) & 0xFFFFFFFF,
            uc_inst.reg_read(UC_ARM_REG_R1) & 0xFFFFFFFF,
            uc_inst.reg_read(UC_ARM_REG_R2) & 0xFFFFFFFF,
            uc_inst.reg_read(UC_ARM_REG_R3) & 0xFFFFFFFF,
        ]
        if kind == "fmt0":
            self.stub_calls.append((sym, self._read_cstr(r_vals[0])))
        elif kind == "assert_file_line":
            self.stub_calls.append((sym, self._read_cstr(r_vals[0]), r_vals[1]))
        else:
            logged = [self._norm_arg(r_vals[i]) for i in range(min(arity, 4))]
            self.stub_calls.append((sym, *logged))
        ret_val = _parse_int(pst.get("ret", 0)) & 0xFFFFFFFF
        uc_inst.reg_write(UC_ARM_REG_R0, ret_val)

    def patch_rom(self, addr: int, patch_bytes: bytes) -> None:
        orig = bytes(self.uc.mem_read(addr, len(patch_bytes)))
        self.patched_rom.append((addr, orig))
        self.uc.mem_write(addr, patch_bytes)
        self.uc.ctl_remove_cache(addr & ~3, (addr & ~3) + len(patch_bytes) + 32)

    def reset(self) -> None:
        for addr, orig in reversed(self.patched_rom):
            self.uc.mem_write(addr, orig)
            self.uc.ctl_remove_cache(addr & ~3, (addr & ~3) + len(orig) + 32)
        self.patched_rom.clear()

        self.uc.mem_write(SRAM_BASE, self.base_ram_data)
        self.uc.mem_write(SRAM_BASE + 0x1A0B0, self.zero_ram_bss)
        self.dirty_sram_pages.clear()

        for mword in self.touched_mmio_words:
            self.uc.mem_write(mword, b"\x00\x00\x00\x00")
        self.touched_mmio_words.clear()

        self.mmio_trace.clear()
        self.mmio_read_plan.clear()
        self.mmio_read_idx.clear()
        self.vt_calls.clear()
        self.vt_rets.clear()
        self.vt_arities.clear()
        self.vt_fpu_rets.clear()
        self.vt_out_u32.clear()
        self.stub_calls.clear()
        self.track_stub_calls = False

        self.uc.reg_write(UC_ARM_REG_C1_C0_2, 0x00F00000)
        self.uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
        self.uc.reg_write(UC_ARM_REG_FPSCR, 0)
        for sreg in FPU_S_REGS:
            self.uc.reg_write(sreg, 0)
        self.uc.reg_write(UC_ARM_REG_SP, INIT_SP)
        self.uc.reg_write(UC_ARM_REG_LR, STOP_ADDR | 1)
        self.uc.reg_write(UC_ARM_REG_XPSR, 0x01000000)
        self.uc.reg_write(UC_ARM_REG_PRIMASK, 0)
        self.uc.reg_write(UC_ARM_REG_BASEPRI, 0)
        self.uc.reg_write(UC_ARM_REG_IPSR, 0)
        for reg in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3):
            self.uc.reg_write(reg, 0)

        self.uc.mem_write(0x2089BED0, struct.pack("<I", 0x80))  # g_crit_basepri
        self.dirty_sram_pages.add(0x2089BE00)

    def exec_spec(
        self,
        spec: CaseSpec,
        entry_pc: int,
        pending_stubs: list[dict[str, Any]],
        patch: tuple[int, bytes] | None = None,
    ) -> dict[str, Any]:
        self.reset()

        if spec.stub_pending:
            for hook_addr in self._stub_by_addr:
                self.patch_rom(hook_addr, b"\x70\x47")
        self.track_stub_calls = spec.stub_pending

        if patch is not None:
            self.patch_rom(patch[0], patch[1])

        if spec.seed_vt_array:
            self.uc.mem_write(DEFAULT_VT_ARRAY, self.default_vt_blob)
            for off in range(0, len(self.default_vt_blob), 0x100):
                self.dirty_sram_pages.add((DEFAULT_VT_ARRAY + off) & ~0xFF)

        if spec.stack_words:
            sp = INIT_SP - 4 * ((len(spec.stack_words) + 1) & ~1)
            self.uc.reg_write(UC_ARM_REG_SP, sp)
            for idx, w in enumerate(spec.stack_words):
                addr = sp + idx * 4
                self.uc.mem_write(addr, struct.pack("<I", w & 0xFFFFFFFF))
                self.dirty_sram_pages.add(addr & ~0xFF)

        reg_ids = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)
        for idx, val in enumerate(spec.args[:4]):
            self.uc.reg_write(reg_ids[idx], val & 0xFFFFFFFF)
        if len(spec.args) > 4 and not spec.stack_words:
            extra_stk = spec.args[4:]
            sp = INIT_SP - 4 * ((len(extra_stk) + 1) & ~1)
            self.uc.reg_write(UC_ARM_REG_SP, sp)
            for idx, w in enumerate(extra_stk):
                addr = sp + idx * 4
                self.uc.mem_write(addr, struct.pack("<I", w & 0xFFFFFFFF))
                self.dirty_sram_pages.add(addr & ~0xFF)

        for idx, s_bits in enumerate(spec.fpu_s[:16]):
            self.uc.reg_write(FPU_S_REGS[idx], s_bits & 0xFFFFFFFF)

        if spec.primask:
            self.uc.reg_write(UC_ARM_REG_PRIMASK, spec.primask)
        if spec.basepri:
            self.uc.reg_write(UC_ARM_REG_BASEPRI, spec.basepri)
        if spec.ipsr:
            self.uc.reg_write(UC_ARM_REG_IPSR, spec.ipsr)

        for w_addr, w_bytes in spec.mem_writes:
            self.uc.mem_write(w_addr, w_bytes)
            if SRAM_BASE <= w_addr < SRAM_BASE + SRAM_MAP_SIZE:
                for p in range(w_addr & ~0xFF, ((w_addr + max(len(w_bytes), 1) - 1) & ~0xFF) + 1, 0x100):
                    self.dirty_sram_pages.add(p)
            else:
                for mw in range(w_addr & ~3, ((w_addr + max(len(w_bytes), 1) - 1) & ~3) + 1, 4):
                    self.touched_mmio_words.add(mw)

        self.mmio_read_plan = {k: list(v) for k, v in spec.mmio_reads.items()}
        self.vt_rets = dict(spec.vt_rets)
        self.vt_arities = dict(spec.vt_arities)
        self.vt_fpu_rets = dict(spec.vt_fpu_rets)
        self.vt_out_u32 = dict(spec.vt_out_u32)

        if spec.setup_fn is not None:
            # Track any writes performed directly by Python setup_fn
            spec.setup_fn(self.uc)
            # Mark common setup pages dirty so reset restores them cleanly
            for p in range(SCRATCH_BASE, SCRATCH_BASE + 0x2000, 0x100):
                self.dirty_sram_pages.add(p)
            for fixed_p in (0x2080C200, 0x2080C500, 0x2080C600, 0x2080C700, 0x2080C800,
                            0x2080CB00, 0x2080CC00, 0x2080CD00, 0x2080D800, 0x2080DA00,
                            0x2080DB00, 0x20819800, 0x2089BA00, 0x2089BB00, 0x2089BC00):
                self.dirty_sram_pages.add(fixed_p)
            for fixed_m in (0xE000E018, 0xE000E100, 0xE000E200, 0xE000E300, 0xE000ED04,
                            0xE000ED24, 0x42016008, 0x4201600C, 0x42200000, 0x42220000,
                            0x42220020, 0x42220080, 0x42220808, 0x42440100):
                self.touched_mmio_words.add(fixed_m & ~3)

        self.mmio_trace.clear()
        self.mmio_read_idx.clear()
        self.vt_calls.clear()
        self.stub_calls.clear()

        halted_err: str | None = None
        try:
            self.uc.emu_start(entry_pc | 1, STOP_ADDR, count=spec.max_insns)
            end_pc = self.uc.reg_read(UC_ARM_REG_PC) & ~1
            if end_pc != STOP_ADDR:
                halted_err = f"stopped_at_0x{end_pc:x}"
        except Exception as exc:
            halted_err = f"uc_err:{exc}"

        fpu_out = tuple(
            self.uc.reg_read(FPU_S_REGS[i]) & 0xFFFFFFFF for i in range(spec.check_fpu)
        )
        globals_snap: list[bytes] = []
        if spec.check_globals:
            globals_snap = [bytes(self.uc.mem_read(g_base, g_len)) for g_base, g_len in GLOBAL_DIFF_SPANS]

        return {
            "halted_err": halted_err,
            "r0": self.uc.reg_read(UC_ARM_REG_R0) & 0xFFFFFFFF,
            "r1": self.uc.reg_read(UC_ARM_REG_R1) & 0xFFFFFFFF,
            "fpu": fpu_out,
            "scratch": bytes(self.uc.mem_read(SCRATCH_BASE, SCRATCH_SIZE)),
            "extra": [bytes(self.uc.mem_read(a, sz)) for (a, sz) in spec.extra_mem_spans],
            "globals": globals_snap,
            "mmio": list(self.mmio_trace),
            "vt_calls": list(self.vt_calls),
            "stub_calls": list(self.stub_calls),
            "sysregs": (
                self.uc.reg_read(UC_ARM_REG_PRIMASK) & 0xFFFFFFFF,
                self.uc.reg_read(UC_ARM_REG_BASEPRI) & 0xFFFFFFFF,
            ),
        }


class DiffHarness:
    def __init__(
        self,
        raw_bytes: bytes,
        elf_path: Path,
        func_map: dict[str, tuple[int, int]],
        elf_syms: dict[str, int],
        pending_stubs: list[dict[str, Any]],
    ) -> None:
        self.raw_bytes = raw_bytes
        self.elf_alloc_sections: list[tuple[int, bytes]] = []
        with open(elf_path, "rb") as f:
            elf = ELFFile(f)
            for sec in elf.iter_sections():
                if (sec["sh_flags"] & 0x2) != 0 and sec["sh_type"] != "SHT_NOBITS" and sec["sh_size"] > 0:
                    self.elf_alloc_sections.append((sec["sh_addr"], sec.data()))
        self.func_map = func_map
        self.elf_syms = elf_syms
        self.pending_stubs = pending_stubs
        self.vm_raw = ReusableArmMachine(False, raw_bytes, self.elf_alloc_sections, elf_syms, pending_stubs)
        self.vm_elf = ReusableArmMachine(True, raw_bytes, self.elf_alloc_sections, elf_syms, pending_stubs)

    @staticmethod
    def _compare(spec: CaseSpec, raw_out: dict[str, Any], elf_out: dict[str, Any]) -> list[str]:
        diffs: list[str] = []
        if raw_out["halted_err"] is not None or elf_out["halted_err"] is not None:
            if raw_out["halted_err"] != elf_out["halted_err"] or elf_out["halted_err"] is not None:
                diffs.append(f"halt raw={raw_out['halted_err']} elf={elf_out['halted_err']}")
                return diffs
        if not spec.ignore_r0 and raw_out["r0"] != elf_out["r0"]:
            diffs.append(f"r0 raw=0x{raw_out['r0']:08x} elf=0x{elf_out['r0']:08x}")
        if spec.ret64 and raw_out["r1"] != elf_out["r1"]:
            diffs.append(f"r1 raw=0x{raw_out['r1']:08x} elf=0x{elf_out['r1']:08x}")
        for idx in range(spec.check_fpu):
            rs = raw_out["fpu"][idx]
            es = elf_out["fpu"][idx]
            if rs != es:
                if _is_f32_nan(rs) and _is_f32_nan(es):
                    continue
                if spec.fpu_ulp_tol > 0 and abs(int(rs) - int(es)) <= spec.fpu_ulp_tol:
                    continue
                diffs.append(f"s{idx} raw=0x{rs:08x} elf=0x{es:08x}")
        if raw_out["scratch"] != elf_out["scratch"]:
            for i in range(len(raw_out["scratch"])):
                if raw_out["scratch"][i] != elf_out["scratch"][i]:
                    diffs.append(
                        f"scratch+0x{i:x} raw=0x{raw_out['scratch'][i]:02x} elf=0x{elf_out['scratch'][i]:02x}"
                    )
                    break
        if raw_out["extra"] != elf_out["extra"]:
            diffs.append("extra_mem_spans mismatch")
        if spec.check_globals and raw_out["globals"] != elf_out["globals"]:
            for span_idx, (g_base, _) in enumerate(GLOBAL_DIFF_SPANS):
                rb = raw_out["globals"][span_idx]
                eb = elf_out["globals"][span_idx]
                if rb != eb:
                    for off in range(len(rb)):
                        if rb[off] != eb[off]:
                            diffs.append(f"sram@0x{g_base + off:08x} raw=0x{rb[off]:02x} elf=0x{eb[off]:02x}")
                            break
                    break
        if raw_out["mmio"] != elf_out["mmio"]:
            diffs.append(f"mmio raw={raw_out['mmio'][:4]} elf={elf_out['mmio'][:4]}")
        if raw_out["vt_calls"] != elf_out["vt_calls"]:
            diffs.append(f"vt_calls raw={raw_out['vt_calls'][:4]} elf={elf_out['vt_calls'][:4]}")
        if spec.stub_pending and raw_out["stub_calls"] != elf_out["stub_calls"]:
            diffs.append(f"stub_calls raw={raw_out['stub_calls'][:4]} elf={elf_out['stub_calls'][:4]}")
        if spec.check_sysregs and raw_out["sysregs"] != elf_out["sysregs"]:
            diffs.append(f"sysregs raw={raw_out['sysregs']} elf={elf_out['sysregs']}")
        return diffs

    def run_spec(self, spec: CaseSpec) -> tuple[dict[str, Any], dict[str, Any]]:
        raw_rva, elf_addr = self.func_map[spec.c_symbol]
        raw_out = self.vm_raw.exec_spec(spec, raw_rva, self.pending_stubs)
        elf_out = self.vm_elf.exec_spec(spec, elf_addr, self.pending_stubs)
        diffs = self._compare(spec, raw_out, elf_out)
        if spec.check_fpu > 0 and spec.ignore_r0:
            raw_ret_s = hex(raw_out["fpu"][0])
            elf_ret_s = hex(elf_out["fpu"][0])
        elif spec.ret64:
            raw_ret_s = hex((raw_out["r1"] << 32) | raw_out["r0"])
            elf_ret_s = hex((elf_out["r1"] << 32) | elf_out["r0"])
        elif spec.ignore_r0:
            raw_ret_s = "void"
            elf_ret_s = "void"
        else:
            raw_ret_s = hex(raw_out["r0"])
            elf_ret_s = hex(elf_out["r0"])

        res = {
            "case": spec.name,
            "c_symbol": spec.c_symbol,
            "raw_rva": hex(raw_rva),
            "elf_addr": hex(elf_addr),
            "raw_ret": raw_ret_s,
            "elf_ret": elf_ret_s,
            "mmio_events": len(elf_out["mmio"]),
            "vt_events": len(elf_out["vt_calls"]),
            "stub_events": len(elf_out["stub_calls"]),
            "passed": len(diffs) == 0,
            "diffs": diffs,
        }
        return res, raw_out

    def check_stub_mutations(
        self, c_symbol: str, specs_and_raw: list[tuple[CaseSpec, dict[str, Any]]], raw_size: int
    ) -> tuple[dict[str, bool], str | None]:
        raw_rva, elf_addr = self.func_map[c_symbol]
        raw_slice = self.raw_bytes[raw_rva : raw_rva + raw_size]
        lit_stub_kind: str | None = None
        for mname, mbytes in STUB_MUTATIONS.items():
            if raw_slice == mbytes:
                lit_stub_kind = mname
                break

        killed: dict[str, bool] = {}
        for mut_name, mut_bytes in STUB_MUTATIONS.items():
            if mut_name == lit_stub_kind:
                killed[mut_name] = True
                continue
            detected = False
            for spec, raw_out in specs_and_raw:
                mut_out = self.vm_elf.exec_spec(
                    spec, elf_addr, self.pending_stubs, patch=(elf_addr, mut_bytes)
                )
                if len(self._compare(spec, raw_out, mut_out)) > 0:
                    detected = True
                    break
            killed[mut_name] = detected
        return killed, lit_stub_kind


def build_specs() -> list[CaseSpec]:
    specs: list[CaseSpec] = []
    B0 = SCRATCH_BASE + 0x000
    B1 = SCRATCH_BASE + 0x200
    B2 = SCRATCH_BASE + 0x400
    PAT64 = bytes(((i * 37 + 0x83) & 0xFF) for i in range(96))

    # --- 1. Libc memory & string primitives (multi-case aligned/unaligned/overlap/high-bit) ---
    specs.append(CaseSpec("memchr_hit_mid", "aopf_memchr", (B0, ord("Z"), 16),
                          setup_fn=lambda uc: uc.mem_write(B0, b"abcdefZ123456789")))
    specs.append(CaseSpec("memchr_miss", "aopf_memchr", (B0, ord("Q"), 16),
                          setup_fn=lambda uc: uc.mem_write(B0, b"abcdefZ123456789")))
    specs.append(CaseSpec("memchr_len0", "aopf_memchr", (B0, ord("a"), 0),
                          setup_fn=lambda uc: uc.mem_write(B0, b"abcdef")))
    specs.append(CaseSpec("memchr_highbit_fe", "aopf_memchr", (B0, 0xFE, 8),
                          setup_fn=lambda uc: uc.mem_write(B0, b"\x01\x80\xfe\xff\x00\x11\x22\x33")))

    specs.append(CaseSpec("memcmp_eq_12", "aopf_memcmp", (B0, B1, 12),
                          setup_fn=lambda uc: (uc.mem_write(B0, b"AppleSPU1234"), uc.mem_write(B1, b"AppleSPU1234"))))
    specs.append(CaseSpec("memcmp_word_lt", "aopf_memcmp", (B0, B1, 8),
                          setup_fn=lambda uc: (uc.mem_write(B0, b"\x01\x02\x03\x04\x10\x20\x30\x40"), uc.mem_write(B1, b"\x01\x02\x03\x04\x10\x20\x31\x40"))))
    specs.append(CaseSpec("memcmp_unaligned_byte_diff", "aopf_memcmp", (B0 + 1, B1 + 1, 7),
                          setup_fn=lambda uc: (uc.mem_write(B0 + 1, b"\xaa\xfe\x11\x22\x33\x44\x55"), uc.mem_write(B1 + 1, b"\xaa\x7f\x11\x22\x33\x44\x55"))))
    specs.append(CaseSpec("memcmp_len0", "aopf_memcmp", (B0, B1, 0),
                          setup_fn=lambda uc: (uc.mem_write(B0, b"abc"), uc.mem_write(B1, b"xyz"))))

    for length, dst_off, src_off in [(0, 0, 0), (1, 0, 0), (4, 0, 0), (31, 1, 3), (64, 0, 0), (64, 2, 1)]:
        dst_a = B1 + dst_off
        src_a = B0 + src_off
        delta = (dst_a - src_a) & 0xFFFFFFFF
        specs.append(CaseSpec(
            f"memcpy_len{length}_d{dst_off}_s{src_off}", "rtk_memcpy",
            (dst_a, src_a, length, delta),
            setup_fn=lambda uc: uc.mem_write(B0, PAT64),
        ))
    specs.append(CaseSpec(
        "memcpy_mismatched_r3_fwd", "rtk_memcpy", (B0 + 5, B0 + 1, 24, 0x100),
        setup_fn=lambda uc: uc.mem_write(B0, PAT64),
    ))
    specs.append(CaseSpec(
        "memcpy_delta_lt_len_bwd", "rtk_memcpy", (B0 + 5, B0 + 1, 24, 4),
        setup_fn=lambda uc: uc.mem_write(B0, PAT64),
    ))

    specs.append(CaseSpec("memcpy_wrap_19", "rtk_memcpy_wrap", (B1, B0, 19),
                          setup_fn=lambda uc: uc.mem_write(B0, PAT64[:32])))
    specs.append(CaseSpec("memcpy_wrap_0", "rtk_memcpy_wrap", (B1 + 4, B0, 0),
                          setup_fn=lambda uc: uc.mem_write(B0, PAT64[:16])))

    for cname, d_off, s_off, length in [
        ("memmove_no_overlap_31", 64, 0, 31),
        ("memmove_fwd_overlap_24", 4, 0, 24),
        ("memmove_bwd_overlap_24", 0, 4, 24),
        ("memmove_same_ptr_16", 8, 8, 16),
        ("memmove_len0", 8, 0, 0),
        ("memmove_len1", 16, 0, 1),
    ]:
        def _mk_mm(nm: str, doff: int, soff: int, ln: int) -> CaseSpec:
            return CaseSpec(nm, "rtk_memmove_entry", (B0 + doff, B0 + soff, ln),
                            setup_fn=lambda uc: uc.mem_write(B0, PAT64))
        specs.append(_mk_mm(cname, d_off, s_off, length))

    for length, off, byte_val in [(0, 0, 0x5A), (1, 1, 0xFF), (4, 0, 0x00), (31, 3, 0xA5), (64, 0, 0x81)]:
        specs.append(CaseSpec(
            f"memset_len{length}_off{off}_v{byte_val:02x}", "rtk_memset",
            (B0 + off, byte_val, length),
            setup_fn=lambda uc: uc.mem_write(B0, b"\x3c" * 80),
        ))

    for cname, lhs_b, rhs_b in [
        ("strcmp_eq", b"VoiceTrigger\x00", b"VoiceTrigger\x00"),
        ("strcmp_lt", b"AudioA\x00", b"AudioB\x00"),
        ("strcmp_gt", b"AudioZ\x00", b"AudioB\x00"),
        ("strcmp_empty", b"\x00", b"\x00"),
        ("strcmp_highbit_unsigned", b"\x80\x00", b"\x7f\x00"),
    ]:
        def _mk_sc(lb: bytes, rb: bytes, nm: str) -> CaseSpec:
            return CaseSpec(nm, "aopf_strcmp", (B0, B1),
                            setup_fn=lambda uc: (uc.mem_write(B0, lb), uc.mem_write(B1, rb)))
        specs.append(_mk_sc(lhs_b, rhs_b, cname))

    for cname, s_bytes in [
        ("strlen_0", b"\x00"),
        ("strlen_1", b"X\x00"),
        ("strlen_4", b"RTK!\x00"),
        ("strlen_31", b"0123456789abcdef0123456789abcde\x00"),
        ("strlen_64", b"A" * 64 + b"\x00"),
    ]:
        def _mk_sl(sb: bytes, nm: str) -> CaseSpec:
            return CaseSpec(nm, "aopf_strlen", (B0 + 1,),
                            setup_fn=lambda uc: uc.mem_write(B0 + 1, sb))
        specs.append(_mk_sl(s_bytes, cname))

    for cname, lb, rb, n in [
        ("strncmp_prefix_eq", b"laguna_a\x00", b"laguna_b\x00", 7),
        ("strncmp_diff", b"laguna_a\x00", b"laguna_b\x00", 8),
        ("strncmp_zero_n", b"abc\x00", b"xyz\x00", 0),
        ("strncmp_highbit", b"\xf0\x00", b"\x10\x00", 2),
    ]:
        def _mk_snc(l_b: bytes, r_b: bytes, mx: int, nm: str) -> CaseSpec:
            return CaseSpec(nm, "aopf_strncmp", (B0, B1, mx),
                            setup_fn=lambda uc: (uc.mem_write(B0, l_b), uc.mem_write(B1, r_b)))
        specs.append(_mk_snc(lb, rb, n, cname))

    specs.append(CaseSpec("strnlen_bounded_6", "rtk_strnlen", (B0, 6),
                          setup_fn=lambda uc: uc.mem_write(B0, b"0123456789\x00")))
    specs.append(CaseSpec("strnlen_short_3", "rtk_strnlen", (B0, 16),
                          setup_fn=lambda uc: uc.mem_write(B0, b"aop\x00")))
    specs.append(CaseSpec("strnlen_zero", "rtk_strnlen", (B0, 0),
                          setup_fn=lambda uc: uc.mem_write(B0, b"aop\x00")))

    specs.append(CaseSpec("stpncpy_padded", "rtk_stpncpy", (B1, B0, 12),
                          setup_fn=lambda uc: (uc.mem_write(B0, b"aop\x00"), uc.mem_write(B1, b"\xff" * 16))))
    specs.append(CaseSpec("stpncpy_trunc", "rtk_stpncpy", (B1, B0, 4),
                          setup_fn=lambda uc: (uc.mem_write(B0, b"AppleSPU\x00"), uc.mem_write(B1, b"\xff" * 16))))

    specs.append(CaseSpec("strlcpy_trunc", "rtk_strlcpy", (B1, B0, 6),
                          setup_fn=lambda uc: uc.mem_write(B0, b"AppleSPU\x00")))
    specs.append(CaseSpec("strlcpy_size0", "rtk_strlcpy", (B1, B0, 0),
                          setup_fn=lambda uc: uc.mem_write(B0, b"RTKit\x00")))

    specs.append(CaseSpec("strlcat_append", "rtk_strlcat", (B1, B0, 12),
                          setup_fn=lambda uc: (uc.mem_write(B1, b"RTK_\x00"), uc.mem_write(B0, b"3514.0.15\x00"))))
    specs.append(CaseSpec("strlcat_size0", "rtk_strlcat", (B1, B0, 0),
                          setup_fn=lambda uc: uc.mem_write(B0, b"abc\x00")))

    # --- 2. 64-bit shifts (incl. >=64 & 256), u64_div_u32, i64_to_dec, abs, CRC, ASCII ---
    for sh in (0, 1, 13, 31, 32, 37, 63, 64, 95, 255, 256):
        specs.append(CaseSpec(f"u64_shl_{sh}", "rtk_u64_shl", (0x89ABCDEF, 0x01234567, sh), ret64=True))
    for sh in (0, 1, 19, 31, 32, 41, 63, 64, 95, 255, 256):
        specs.append(CaseSpec(f"u64_shr_{sh}", "rtk_u64_shr", (0x89ABCDEF, 0xFEDCBA98, sh), ret64=True))

    specs.append(CaseSpec("u64_div_u32_1e12", "rtk_u64_div_u32", (0xD4A51000, 0x000000E8, 1000000), ret64=True))
    specs.append(CaseSpec("u64_div_u32_small", "rtk_u64_div_u32", (42, 0, 7), ret64=True))
    specs.append(CaseSpec("u64_div_u32_zero_num", "rtk_u64_div_u32", (0, 0, 10), ret64=True))

    specs.append(CaseSpec("i64_to_dec_neg", "rtk_i64_to_dec", (B0, 0, 0xFFFFFFCE, 0xFFFFFFFF), stack_words=(1, 0)))
    specs.append(CaseSpec("i64_to_dec_plus", "rtk_i64_to_dec", (B0, 0, 123456789, 0), stack_words=(1, 1)))
    specs.append(CaseSpec("i64_to_dec_unsigned_hi", "rtk_i64_to_dec", (B0, 0, 0xFFFFFFFE, 0xFFFFFFFF), stack_words=(0, 0)))

    specs.append(CaseSpec("abs_i32_neg", "rtk_abs_i32", (0xFFFFFF9C,)))
    specs.append(CaseSpec("abs_i32_zero", "rtk_abs_i32", (0,)))
    specs.append(CaseSpec("abs_i64_neg", "rtk_abs_i64", (0x00000001, 0xFFFFFFFE), ret64=True))
    specs.append(CaseSpec("abs_i64_zero", "rtk_abs_i64", (0, 0), ret64=True))

    specs.append(CaseSpec("crc8_step_a5", "rtk_crc8_step", (0xFF, 0xA5)))
    specs.append(CaseSpec("crc8_step_00", "rtk_crc8_step", (0x00, 0x00)))
    specs.append(CaseSpec("crc8_buf_9bytes", "rtk_crc8_buf", (0x00, B0, 9),
                          setup_fn=lambda uc: uc.mem_write(B0, b"123456789")))
    specs.append(CaseSpec("crc8_buf_empty", "rtk_crc8_buf", (0x00, B0, 0)))

    specs.append(CaseSpec("crc32_crash76_pat", "rtk_crc32_crash76", (B0,),
                          setup_fn=lambda uc: uc.mem_write(B0, bytes((i * 13 + 5) & 0xFF for i in range(76)))))
    specs.append(CaseSpec("crc32_crash76_zero", "rtk_crc32_crash76", (B0,),
                          setup_fn=lambda uc: uc.mem_write(B0, b"\x00" * 76)))

    specs.append(CaseSpec("ascii_printable_A", "rtk_is_ascii_printable", (0x41,)))
    specs.append(CaseSpec("ascii_printable_tilde", "rtk_is_ascii_printable", (0x7E,)))
    specs.append(CaseSpec("ascii_printable_del", "rtk_is_ascii_printable", (0x7F,)))
    specs.append(CaseSpec("ascii_printable_ctrl", "rtk_is_ascii_printable", (0x1F,)))

    specs.append(CaseSpec(
        "vsnprintf_putc_write", "rtk_vsnprintf_putc", (B0, ord("K")), ignore_r0=True,
        setup_fn=lambda uc: uc.mem_write(B0, struct.pack("<IIIII", 0, 8, B1, 0, 0)),
    ))
    specs.append(CaseSpec(
        "vsnprintf_putc_full", "rtk_vsnprintf_putc", (B0, ord("X")), ignore_r0=True,
        setup_fn=lambda uc: (uc.mem_write(B0, struct.pack("<IIIII", 0, 2, B1, 1, 0)), uc.mem_write(B1, b"A\xff")),
    ))

    # --- 3. Segregated Free-List Heap, Spinlock, Fastlock Trylock & Stack Watermark ---
    def _setup_heap_base(uc: Uc) -> None:
        uc.mem_write(B0, b"\x00" * 0x70)
        uc.mem_write(B1 + 128, struct.pack("<III", 16, 0, 0))
        uc.mem_write(0x20819844, struct.pack("<I", 0x20820100))

    def _setup_heap_free128(uc: Uc) -> None:
        _setup_heap_base(uc)
        uc.mem_write(B1, struct.pack("<III", 128 | 1, 0, 0))
        uc.mem_write(B1 + 124, struct.pack("<I", 128))
        uc.mem_write(B0 + 0x48, struct.pack("<I", B1))

    specs.append(CaseSpec("heap_init_free_hdr", "rtk_heap_init_free_hdr", (B1, 96),
                          ignore_r0=True, setup_fn=_setup_heap_base))
    specs.append(CaseSpec("heap_insert_block", "rtk_heap_insert_block", (B0, B1),
                          ignore_r0=True,
                          setup_fn=lambda uc: (_setup_heap_base(uc), uc.mem_write(B1, struct.pack("<III", 64 | 1, 0, 0)), uc.mem_write(B1 + 60, struct.pack("<I", 64)))))
    specs.append(CaseSpec("heap_unlink_head", "rtk_heap_unlink_block", (B0, B1),
                          ignore_r0=True, setup_fn=_setup_heap_free128))
    specs.append(CaseSpec("heap_alloc_split", "rtk_heap_alloc", (B0, 32), setup_fn=_setup_heap_free128))
    specs.append(CaseSpec("heap_alloc_oom", "rtk_heap_alloc", (B0, 512), setup_fn=_setup_heap_free128))

    specs.append(CaseSpec("spinlock_acquire_free_b0", "rtk_spinlock_acquire", (B0,), check_sysregs=True,
                          setup_fn=lambda uc: uc.mem_write(B0, struct.pack("<I", 0))))
    specs.append(CaseSpec("spinlock_acquire_free_b40", "rtk_spinlock_acquire", (B0,), check_sysregs=True,
                          setup_fn=lambda uc: (uc.mem_write(B0, struct.pack("<I", 0)), uc.reg_write(UC_ARM_REG_BASEPRI, 0x40))))
    specs.append(CaseSpec("spinlock_acquire_busy", "rtk_spinlock_acquire", (B0,), check_sysregs=True,
                          setup_fn=lambda uc: uc.mem_write(B0, struct.pack("<I", 1))))

    specs.append(CaseSpec("fastlock_trylock_free", "rtk_fastlock_trylock", (B0,),
                          setup_fn=lambda uc: (uc.mem_write(B0, struct.pack("<I", 0)), uc.mem_write(0x20819844, struct.pack("<I", 0x20820100)))))
    specs.append(CaseSpec("fastlock_trylock_recursive", "rtk_fastlock_trylock", (B0,),
                          setup_fn=lambda uc: (uc.mem_write(B0, struct.pack("<I", 0x20820100)), uc.mem_write(0x20819844, struct.pack("<I", 0x20820100)))))
    specs.append(CaseSpec("fastlock_trylock_contended", "rtk_fastlock_trylock", (B0,),
                          setup_fn=lambda uc: (uc.mem_write(B0, struct.pack("<I", 0x20820200)), uc.mem_write(0x20819844, struct.pack("<I", 0x20820100)))))

    specs.append(CaseSpec("stack_unused_48", "rtk_stack_unused_bytes", (B0, 64),
                          setup_fn=lambda uc: uc.mem_write(B0, struct.pack("<II", 0x534B5452, 0x4B434154) * 6 + struct.pack("<II", 1, 2) * 2)))
    specs.append(CaseSpec("stack_unused_0", "rtk_stack_unused_bytes", (B0, 64),
                          setup_fn=lambda uc: uc.mem_write(B0, b"\x11" * 64)))

    # --- 4. Stack Guard, Critical Sections, IRQ Mask & NVIC ---
    specs.append(CaseSpec("stack_chk_guard_init_stkg", "rtk_stack_chk_guard_init", (), ignore_r0=True,
                          setup_fn=lambda uc: uc.mem_write(0x2080C2D0, b"\x00\x00\x00\x00"),
                          extra_mem_spans=[(0x2080C2D0, 4)]))

    specs.append(CaseSpec("enter_critical_from_zero", "rtk_enter_critical", (), check_sysregs=True))
    specs.append(CaseSpec("enter_critical_from_higher_prio_0x40", "rtk_enter_critical", (),
                          setup_fn=lambda uc: uc.reg_write(UC_ARM_REG_BASEPRI, 0x40), check_sysregs=True))
    specs.append(CaseSpec("enter_critical_from_lower_prio_0xc0", "rtk_enter_critical", (),
                          setup_fn=lambda uc: uc.reg_write(UC_ARM_REG_BASEPRI, 0xC0), check_sysregs=True))

    specs.append(CaseSpec("exit_critical_to_zero", "rtk_exit_critical", (0,), ignore_r0=True,
                          setup_fn=lambda uc: uc.reg_write(UC_ARM_REG_BASEPRI, 0x80), check_sysregs=True))
    specs.append(CaseSpec("exit_critical_to_0x60", "rtk_exit_critical", (0x60,), ignore_r0=True,
                          setup_fn=lambda uc: uc.reg_write(UC_ARM_REG_BASEPRI, 0x80), check_sysregs=True))

    specs.append(CaseSpec("are_irqs_enabled_true", "rtk_are_irqs_enabled", (),
                          setup_fn=lambda uc: uc.reg_write(UC_ARM_REG_R0, 0x55AA1234)))
    specs.append(CaseSpec("are_irqs_enabled_basepri_masked", "rtk_are_irqs_enabled", (),
                          setup_fn=lambda uc: uc.reg_write(UC_ARM_REG_BASEPRI, 0x80)))
    specs.append(CaseSpec("are_irqs_enabled_primask_masked", "rtk_are_irqs_enabled", (),
                          setup_fn=lambda uc: uc.reg_write(UC_ARM_REG_PRIMASK, 1)))

    specs.append(CaseSpec("irq_enable_all", "rtk_irq_enable_all", (), ignore_r0=True,
                          setup_fn=lambda uc: (uc.reg_write(UC_ARM_REG_PRIMASK, 1), uc.reg_write(UC_ARM_REG_BASEPRI, 0x80)),
                          check_sysregs=True))
    specs.append(CaseSpec("irq_save_if_thread_unmasked", "rtk_irq_save_if_thread", (), check_sysregs=True))
    specs.append(CaseSpec("irq_save_if_thread_already_masked", "rtk_irq_save_if_thread", (),
                          setup_fn=lambda uc: uc.reg_write(UC_ARM_REG_PRIMASK, 1), check_sysregs=True))
    specs.append(CaseSpec("irq_restore_if_saved_1", "rtk_irq_restore_if_saved", (1,), ignore_r0=True,
                          setup_fn=lambda uc: uc.reg_write(UC_ARM_REG_PRIMASK, 1), check_sysregs=True))
    specs.append(CaseSpec("irq_restore_if_saved_0", "rtk_irq_restore_if_saved", (0,), ignore_r0=True,
                          setup_fn=lambda uc: uc.reg_write(UC_ARM_REG_PRIMASK, 1), check_sysregs=True))

    specs.append(CaseSpec("device_claim_mmio_ok", "rtk_device_claim_mmio", (0x2080CBE8,),
                          extra_mem_spans=[(0x2080CBE8, 0x18)]))
    specs.append(CaseSpec("device_claim_mmio_missing_lo", "rtk_device_claim_mmio", (B0,),
                          setup_fn=lambda uc: uc.mem_write(B0, struct.pack("<IIIIII", 0, 0, 0, 1, 0, 0))))

    def _setup_nvic(uc: Uc) -> None:
        uc.mem_write(0x20819830, struct.pack("<I", 64))

    specs.append(CaseSpec("nvic_register_irq_32", "nvic_register_irq", (0x20819808, 32, 0x12345679, 0xCAFE0000),
                          setup_fn=_setup_nvic, extra_mem_spans=[(0x2089BA10 + 32 * 4, 4), (0x2089BB50 + 32 * 4, 4)]))
    specs.append(CaseSpec("nvic_register_irq_bad", "nvic_register_irq", (0x20819808, 99, 0, 0),
                          setup_fn=_setup_nvic))
    specs.append(CaseSpec("nvic_set_irq_enabled_from0", "nvic_set_irq_enabled", (0x20819808, 35, 1),
                          setup_fn=_setup_nvic))
    specs.append(CaseSpec("nvic_set_irq_enabled_from1", "nvic_set_irq_enabled", (0x20819808, 35, 0),
                          setup_fn=lambda uc: (_setup_nvic(uc), uc.mem_write(0xE000E100, struct.pack("<I", 1 << (35 - 16))))))
    specs.append(CaseSpec("nvic_set_irq_priority_20", "nvic_set_irq_priority", (0x20819808, 20, 6),
                          setup_fn=_setup_nvic))
    specs.append(CaseSpec("nvic_set_irq_priority_bad", "nvic_set_irq_priority", (0x20819808, 99, 6),
                          setup_fn=_setup_nvic))
    specs.append(CaseSpec("nvic_is_pending_0", "nvic_is_irq_pending", (0x20819808, 24),
                          setup_fn=_setup_nvic))
    specs.append(CaseSpec("nvic_is_pending_1", "nvic_is_irq_pending", (0x20819808, 24),
                          setup_fn=lambda uc: (_setup_nvic(uc), uc.mem_write(0xE000E200, struct.pack("<I", 1 << (24 - 16))))))
    specs.append(CaseSpec("nvic_is_pending_systick_1", "nvic_is_irq_pending", (0x20819808, 15),
                          setup_fn=lambda uc: (_setup_nvic(uc), uc.mem_write(0xE000ED04, struct.pack("<I", 1 << 26)))))
    specs.append(CaseSpec("nvic_is_pending_nmi_2", "nvic_is_irq_pending", (0x20819808, 2),
                          setup_fn=lambda uc: (_setup_nvic(uc), uc.mem_write(0xE000ED04, struct.pack("<I", 0x80000000)))))
    specs.append(CaseSpec("nvic_is_active_0", "nvic_is_irq_active", (0x20819808, 27),
                          setup_fn=_setup_nvic))
    specs.append(CaseSpec("nvic_is_active_1", "nvic_is_irq_active", (0x20819808, 27),
                          setup_fn=lambda uc: (_setup_nvic(uc), uc.mem_write(0xE000E300, struct.pack("<I", 1 << (27 - 16))))))
    specs.append(CaseSpec("nvic_is_active_systick_1", "nvic_is_irq_active", (0x20819808, 15),
                          setup_fn=lambda uc: (_setup_nvic(uc), uc.mem_write(0xE000ED24, struct.pack("<I", 1 << 11)))))
    specs.append(CaseSpec("nvic_is_active_nmi_0", "nvic_is_irq_active", (0x20819808, 2),
                          setup_fn=lambda uc: (_setup_nvic(uc), uc.mem_write(0xE000ED24, struct.pack("<I", 0x70000)))))
    specs.append(CaseSpec("nvic_is_active_nmi_1", "nvic_is_irq_active", (0x20819808, 2),
                          setup_fn=lambda uc: (_setup_nvic(uc), uc.mem_write(0xE000ED24, struct.pack("<I", 0x70000)), uc.reg_write(UC_ARM_REG_IPSR, 2))))
    specs.append(CaseSpec("nvic_clear_pending_24", "nvic_clear_irq_pending", (0x20819808, 24),
                          setup_fn=_setup_nvic))
    specs.append(CaseSpec("nvic_clear_pending_bad", "nvic_clear_irq_pending", (0x20819808, 5),
                          setup_fn=_setup_nvic))
    specs.append(CaseSpec("nvic_trigger_irq_24", "nvic_trigger_irq", (0x20819808, 24),
                          setup_fn=_setup_nvic))
    specs.append(CaseSpec("nvic_trigger_irq_bad", "nvic_trigger_irq", (0x20819808, 5),
                          setup_fn=_setup_nvic))

    # --- 5. SysTick, Timebase (0x42016000) & TimerBank (0x42020000) ---
    SYST_DEV = 0x2080CBA8
    TB_DEV = 0x2080CBE8
    TMR_DEV = 0x2080C7F8

    specs.append(CaseSpec("systick_set_cb", "SysTick_setCallback", (SYST_DEV, CB_STUB_ADDR | 1, 0x12340000),
                          ignore_r0=True, extra_mem_spans=[(SYST_DEV + 0x20, 8)]))
    specs.append(CaseSpec("systick_program_reload", "SysTick_programReload", (SYST_DEV, 0, 50000, 0),
                          ignore_r0=True, extra_mem_spans=[(SYST_DEV + 0x30, 8)]))
    specs.append(CaseSpec("systick_arm_ticks_ok", "SysTick_armTicks", (SYST_DEV, 0, 24000, 0),
                          extra_mem_spans=[(SYST_DEV + 0x30, 12)]))
    specs.append(CaseSpec("systick_arm_ticks_zero", "SysTick_armTicks", (SYST_DEV, 0, 0, 0),
                          extra_mem_spans=[(SYST_DEV + 0x30, 12)]))
    specs.append(CaseSpec("systick_arm_us_ok", "SysTick_armUs", (SYST_DEV, 0, 500, 0),
                          setup_fn=lambda uc: uc.mem_write(SYST_DEV + 0x2C, struct.pack("<I", 24)),
                          extra_mem_spans=[(SYST_DEV + 0x30, 12)]))
    specs.append(CaseSpec("systick_cancel_with_rem", "SysTick_cancel", (SYST_DEV, B0),
                          setup_fn=lambda uc: uc.mem_write(0xE000E018, struct.pack("<I", 4321))))
    specs.append(CaseSpec("systick_us_to_ticks_250", "SysTick_usToTicks", (SYST_DEV, 0, 250, 0),
                          setup_fn=lambda uc: uc.mem_write(SYST_DEV + 0x2C, struct.pack("<I", 24)), ret64=True))
    specs.append(CaseSpec("systick_us_to_ticks_0", "SysTick_usToTicks", (SYST_DEV, 0, 0, 0),
                          setup_fn=lambda uc: uc.mem_write(SYST_DEV + 0x2C, struct.pack("<I", 24)), ret64=True))
    specs.append(CaseSpec("systick_ticks_to_us_4800", "SysTick_ticksToUs", (SYST_DEV, 0, 4800, 0),
                          setup_fn=lambda uc: uc.mem_write(SYST_DEV + 0x2C, struct.pack("<I", 24)), ret64=True))
    specs.append(CaseSpec("systick_ticks_to_us_0", "SysTick_ticksToUs", (SYST_DEV, 0, 0, 0),
                          setup_fn=lambda uc: uc.mem_write(SYST_DEV + 0x2C, struct.pack("<I", 24)), ret64=True))
    specs.append(CaseSpec("systick_isr_fires_cb", "SysTick_isr", (SYST_DEV,), ignore_r0=True,
                          setup_fn=lambda uc: (
                              uc.mem_write(SYST_DEV + 0x20, struct.pack("<II", CB_STUB_ADDR | 1, 0)),
                              uc.mem_write(SYST_DEV + 0x30, struct.pack("<II", 0, 0)),
                          )))

    def _setup_tb(uc: Uc) -> None:
        uc.mem_write(0x42016000 + 0x08, struct.pack("<II", 0x12345678, 0x00000003))

    specs.append(CaseSpec("timebase_read_raw48", "Timebase_readRaw48", (TB_DEV,), setup_fn=_setup_tb, ret64=True))
    specs.append(CaseSpec("timebase_read_raw48_zero", "Timebase_readRaw48", (TB_DEV,), ret64=True))
    specs.append(CaseSpec("timebase_set_epoch_pos", "Timebase_setEpochDelta",
                          (TB_DEV, 0, 1000, 0), stack_words=(2500, 0), ignore_r0=True,
                          extra_mem_spans=[(TB_DEV + 0x30, 8), (TB_DEV + 0x54, 4)]))
    specs.append(CaseSpec("timebase_get_ticks", "Timebase_getTicks", (TB_DEV,), setup_fn=_setup_tb, ret64=True))
    specs.append(CaseSpec("timebase_get_ticks_wrap", "Timebase_getTicksWrap", (TB_DEV,), setup_fn=_setup_tb, ret64=True))
    specs.append(CaseSpec("timebase_us_to_ticks_12500", "Timebase_usToTicks", (TB_DEV, 0, 12500, 0), ret64=True))
    specs.append(CaseSpec("timebase_us_to_ticks_0", "Timebase_usToTicks", (TB_DEV, 0, 0, 0), ret64=True))
    specs.append(CaseSpec("timebase_ticks_to_us_98765", "Timebase_ticksToUs", (TB_DEV, 0, 98765, 0), ret64=True))
    specs.append(CaseSpec("timebase_ticks_to_us_0", "Timebase_ticksToUs", (TB_DEV, 0, 0, 0), ret64=True))
    specs.append(CaseSpec("timebase_ticks_to_us_slow", "Timebase_ticksToUsSlow", (TB_DEV, 0, 48000, 0), ret64=True))
    specs.append(CaseSpec("rtk_timer_get_us_live", "rtk_timer_get_us", (TB_DEV,), setup_fn=_setup_tb, ret64=True))

    specs.append(CaseSpec("timerbank_config_ch0", "TimerBank_configChannel", (TMR_DEV, 0, 24, 24000)))
    specs.append(CaseSpec("timerbank_config_bad_ch", "TimerBank_configChannel", (TMR_DEV, 99, 24, 24000)))
    specs.append(CaseSpec("timerbank_stop_ch0", "TimerBank_stopChannel", (TMR_DEV, 0)))
    specs.append(CaseSpec("timerbank_stop_bad_ch", "TimerBank_stopChannel", (TMR_DEV, 99)))
    specs.append(CaseSpec("timerbank_clear_all_irqs", "TimerBank_clearAllIrqs", (TMR_DEV,), ignore_r0=True))

    # --- 6. PMGR, Watchdog & AKF Mailbox ---
    PMGR_DEV = 0x2080D888
    WDT_DEV = 0x2080CCD8
    MBOX_DEV = 0x2080C5D0

    specs.append(CaseSpec("pmgr_lookup_zero_id", "PMGR_lookupRegOffset", (PMGR_DEV, 0, 0, B0)))
    specs.append(CaseSpec("pmgr_lookup_locked", "PMGR_lookupRegOffset", (PMGR_DEV, 1, 0, B0),
                          setup_fn=lambda uc: uc.mem_write(PMGR_DEV + 0x24, b"\x01")))
    specs.append(CaseSpec("pmgr_lookup_valid", "PMGR_lookupRegOffset", (PMGR_DEV, 0xDEAD, 0, B0),
                          setup_fn=lambda uc: (
                              uc.mem_write(PMGR_DEV + 0x24, b"\x00"),
                              uc.mem_write(PMGR_DEV + 0x18, struct.pack("<I", B1)),
                              uc.mem_write(B1, struct.pack("<IIII", 0xDEAD, 0x100, 0x104, 0)),
                          )))
    specs.append(CaseSpec("pmgr_get_ps_active_f", "PMGR_getPowerState", (PMGR_DEV, 0xDEAD, B0),
                          setup_fn=lambda uc: (
                              uc.mem_write(PMGR_DEV + 0x24, b"\x00"),
                              uc.mem_write(PMGR_DEV + 0x18, struct.pack("<I", B1)),
                              uc.mem_write(B1, struct.pack("<IIII", 0xDEAD, 0x98, 0x100, 0)),
                              uc.mem_write(0x42440100, struct.pack("<I", 0xF0)),
                          )))
    specs.append(CaseSpec("pmgr_get_ps_bad_nibble", "PMGR_getPowerState", (PMGR_DEV, 0xDEAD, B0),
                          setup_fn=lambda uc: (
                              uc.mem_write(PMGR_DEV + 0x24, b"\x00"),
                              uc.mem_write(PMGR_DEV + 0x18, struct.pack("<I", B1)),
                              uc.mem_write(B1, struct.pack("<IIII", 0xDEAD, 0x98, 0x100, 0)),
                              uc.mem_write(0x42440100, struct.pack("<I", 0x90)),
                          )))

    specs.append(CaseSpec("wdt_kick", "WDT_kick", (WDT_DEV,)))
    specs.append(CaseSpec("wdt_set_running_1", "WDT_setRunning", (WDT_DEV, 1),
                          setup_fn=lambda uc: uc.mem_write(0x42200000, struct.pack("<I", 0x8002))))
    specs.append(CaseSpec("wdt_set_running_0", "WDT_setRunning", (WDT_DEV, 0)))
    specs.append(CaseSpec("wdt_update_bark_stopped", "WDT_updateBarkPrescaler", (WDT_DEV, 3),
                          setup_fn=lambda uc: uc.mem_write(0x42200000, struct.pack("<I", 0x8002))))
    specs.append(CaseSpec("wdt_set_bark_ok", "WDT_setBarkTimeoutUs", (WDT_DEV, 0, 250000, 0),
                          setup_fn=lambda uc: uc.mem_write(0x42200000, struct.pack("<I", 0x8002))))
    specs.append(CaseSpec("wdt_set_bark_invalid", "WDT_setBarkTimeoutUs", (WDT_DEV, 0, 12345, 0)))
    specs.append(CaseSpec("wdt_get_bark_shift4", "WDT_getBarkTimeoutUs", (WDT_DEV,),
                          setup_fn=lambda uc: uc.mem_write(0x42200000, struct.pack("<I", 4 << 9)), ret64=True))
    specs.append(CaseSpec("wdt_set_bite_en_disallowed", "WDT_setBiteEnable", (WDT_DEV, 1),
                          setup_fn=lambda uc: (uc.mem_write(WDT_DEV + 0x24, struct.pack("<I", 0)), uc.mem_write(WDT_DEV + 0x1C, b"\x00"))))
    specs.append(CaseSpec("wdt_set_bite_en_allowed", "WDT_setBiteEnable", (WDT_DEV, 1),
                          setup_fn=lambda uc: (uc.mem_write(WDT_DEV + 0x1C, b"\x01"), uc.mem_write(WDT_DEV + 0x20, struct.pack("<I", 5)))))
    specs.append(CaseSpec("wdt_set_bite_timeout_ok", "WDT_setBiteTimeoutUs", (WDT_DEV, 0, 375000, 0)))
    specs.append(CaseSpec("wdt_set_bite_timeout_bad", "WDT_setBiteTimeoutUs", (WDT_DEV, 0, 99999, 0)))
    specs.append(CaseSpec("wdt_get_bite_timeout_3", "WDT_getBiteTimeoutUs", (WDT_DEV,),
                          setup_fn=lambda uc: uc.mem_write(0x42200000, struct.pack("<I", 3 << 12)), ret64=True))
    specs.append(CaseSpec("wdt_get_bite_timeout_0", "WDT_getBiteTimeoutUs", (WDT_DEV,),
                          setup_fn=lambda uc: uc.mem_write(0x42200000, struct.pack("<I", 0)), ret64=True))
    specs.append(CaseSpec("wdt_set_callback", "WDT_setCallback", (WDT_DEV, CB_STUB_ADDR | 1, 0x55AA),
                          setup_fn=_setup_nvic, extra_mem_spans=[(WDT_DEV + 0x24, 8)]))

    def _setup_mbox_rx(uc: Uc, empty: bool = False, claimed: bool = True, arm_irq: bool = False) -> None:
        flags = uc.mem_read(MBOX_DEV + 0x14, 1)[0]
        uc.mem_write(MBOX_DEV + 0x14, bytes([(flags | 1) if claimed else (flags & ~1)]))
        err_off = struct.unpack("<I", uc.mem_read(MBOX_DEV + 0xB0, 4))[0]
        rx_off = struct.unpack("<I", uc.mem_read(MBOX_DEV + 0xB8, 4))[0]
        mmio_base = struct.unpack("<I", uc.mem_read(MBOX_DEV + 0x08, 4))[0]
        uc.mem_write(mmio_base + err_off, struct.pack("<I", 0x00020000 if empty else 0))
        uc.mem_write(mmio_base + rx_off, struct.pack("<IIII", 0x10203040, 0x50607080, 0x90A0B0C0, 0xD0E0F001))
        if arm_irq:
            uc.mem_write(MBOX_DEV + 0x90, struct.pack("<I", 1))
            uc.mem_write(MBOX_DEV + 0x48, struct.pack("<I", 0))
            uc.mem_write(MBOX_DEV + 0x64, b"\x01")
            uc.mem_write(MBOX_DEV + 0x68, struct.pack("<I", 5))
            uc.mem_write(MBOX_DEV + 0x58, struct.pack("<I", mmio_base + 0x20))
        else:
            uc.mem_write(MBOX_DEV + 0x90, struct.pack("<I", 0))

    specs.append(CaseSpec("mbox_read128", "Mbox_read128", (0x42220000, 0x80, B0), ignore_r0=True,
                          setup_fn=lambda uc: uc.mem_write(0x42220080, struct.pack("<IIII", 11, 22, 33, 44))))
    specs.append(CaseSpec("mbox_arm_rx_empty_active", "Mbox_armRxEmptyIrq", (MBOX_DEV,), ignore_r0=True,
                          setup_fn=lambda uc: _setup_mbox_rx(uc, empty=True, arm_irq=True)))
    specs.append(CaseSpec("mbox_arm_rx_empty_disabled", "Mbox_armRxEmptyIrq", (MBOX_DEV,), ignore_r0=True,
                          setup_fn=lambda uc: _setup_mbox_rx(uc, empty=True, arm_irq=False)))
    specs.append(CaseSpec("mbox_pop_one_ready", "Mbox_popOne", (MBOX_DEV, B0),
                          setup_fn=lambda uc: _setup_mbox_rx(uc, empty=False),
                          extra_mem_spans=[(MBOX_DEV + 0x95, 1)]))
    specs.append(CaseSpec("mbox_pop_one_empty_bit17_no_arm", "Mbox_popOne", (MBOX_DEV, B0),
                          setup_fn=lambda uc: (uc.mem_write(B0, b"\xaa" * 16), _setup_mbox_rx(uc, empty=True, arm_irq=False)),
                          extra_mem_spans=[(MBOX_DEV + 0x95, 1)]))
    specs.append(CaseSpec("mbox_pop_one_empty_bit17_with_arm", "Mbox_popOne", (MBOX_DEV, B0),
                          setup_fn=lambda uc: (uc.mem_write(B0, b"\xaa" * 16), _setup_mbox_rx(uc, empty=True, arm_irq=True)),
                          extra_mem_spans=[(MBOX_DEV + 0x95, 1)]))
    specs.append(CaseSpec("mbox_try_read_ready", "Mbox_tryReadOne", (MBOX_DEV, B0),
                          setup_fn=lambda uc: _setup_mbox_rx(uc, empty=False, claimed=True)))
    specs.append(CaseSpec("mbox_try_read_unclaimed", "Mbox_tryReadOne", (MBOX_DEV, B0),
                          setup_fn=lambda uc: _setup_mbox_rx(uc, empty=False, claimed=False)))
    specs.append(CaseSpec("mbox_set_irq_handler", "Mbox_setIrqHandler", (MBOX_DEV, CB_STUB_ADDR | 1, 0x7788),
                          setup_fn=lambda uc: (
                              _setup_nvic(uc),
                              uc.mem_write(MBOX_DEV + 0x30, struct.pack("<I", 0x20819808)),
                              uc.mem_write(MBOX_DEV + 0x38, struct.pack("<I", 0x37)),
                          ),
                          extra_mem_spans=[(0x2089BA10 + 0x37 * 4, 4), (0x2089BB50 + 0x37 * 4, 4)]))

    # --- 7. AOP Application Subsystems ---
    def _setup_aud_log(uc: Uc, cap: int = 32) -> None:
        uc.mem_write(B0, struct.pack("<III", B1, cap, 0))

    specs.append(CaseSpec("aud_log_tlv_ok", "aud_log_buf_append_tlv", (B0, 1, B2, 4), ignore_r0=True,
                          setup_fn=lambda uc: (_setup_aud_log(uc, 32), uc.mem_write(B2, b"\xde\xad\xbe\xef"))))
    specs.append(CaseSpec("aud_log_tlv_overflow", "aud_log_buf_append_tlv", (B0, 1, B2, 4), ignore_r0=True,
                          setup_fn=lambda uc: (_setup_aud_log(uc, 3), uc.mem_write(B2, b"\xde\xad\xbe\xef"))))
    specs.append(CaseSpec("aud_log_append_u32", "aud_log_append_u32", (B0, 0xCAFEBABE), setup_fn=_setup_aud_log))
    specs.append(CaseSpec("aud_log_append_str", "aud_log_append_str", (B0, B2),
                          setup_fn=lambda uc: (_setup_aud_log(uc), uc.mem_write(B2, b"spu\x00"))))
    specs.append(CaseSpec("aud_log_append_end", "aud_log_append_end", (B0,), setup_fn=_setup_aud_log))

    def _setup_prop_tbl(uc: Uc) -> None:
        uc.mem_write(0x00077FD0, bytes.fromhex("00f003007047"))
        uc.mem_write(0x00077FC8, bytes.fromhex("00207047"))
        arena_addr = B2
        buckets_addr = B2 + 0x20
        pool_addr = B2 + 0x40
        lock_vt_addr = B2 + 0x100
        uc.mem_write(arena_addr, struct.pack("<IIIHH", pool_addr, 4, 16, 3, 0))
        uc.mem_write(lock_vt_addr, struct.pack("<IIII", 0, 0, CB_STUB_ADDR | 1, 0x00077FC9))
        uc.mem_write(pool_addr, struct.pack("<BBHHHII", 0, 0, 0, 0x101, 0xFFFF, 0, 0))
        uc.mem_write(buckets_addr + 4 * 1, struct.pack("<I", pool_addr))
        uc.mem_write(B0, struct.pack("<IIIIIIIII", 0, 0x00077FD1, 4, buckets_addr, 3, pool_addr + 16, arena_addr, 0, lock_vt_addr))

    specs.append(CaseSpec("prop_lookup_hit", "PropertyTable_lookup", (B0, 0x101), setup_fn=_setup_prop_tbl))
    specs.append(CaseSpec("prop_lookup_miss", "PropertyTable_lookup", (B0, 0x202), setup_fn=_setup_prop_tbl))
    specs.append(CaseSpec("prop_find_or_alloc_existing", "PropertyTable_findOrAlloc", (B0, 0x101), setup_fn=_setup_prop_tbl))
    specs.append(CaseSpec("prop_find_or_alloc_new", "PropertyTable_findOrAlloc", (B0, 0x202), setup_fn=_setup_prop_tbl))

    specs.append(CaseSpec("battery_build_str_default", "Battery_getBuildString", ()))
    specs.append(CaseSpec("battery_build_str_override_match", "Battery_getBuildString", (),
                          setup_fn=lambda uc: uc.mem_write(0x2080DAE4, bytes(uc.mem_read(0x00070A10, 32)))))

    specs.append(CaseSpec("aop_get_timestamp_us", "aop_get_timestamp_us", (), setup_fn=_setup_tb, ret64=True))
    specs.append(CaseSpec("j_aop_get_timestamp_us", "j_aop_get_timestamp_us", (), setup_fn=_setup_tb, ret64=True))
    specs.append(CaseSpec("aop_socr_lfsr_mix_a", "aop_socr_lfsr_mix", (0x89ABCDEF, 0x01234567), ret64=True))
    specs.append(CaseSpec("aop_socr_lfsr_mix_zero", "aop_socr_lfsr_mix", (0, 0), ret64=True))

    return specs


# --- Multiprocessing worker pool state ---
_W_HARNESS: DiffHarness | None = None
_W_SPECS_BY_SYM: dict[str, list[CaseSpec]] = {}
_W_INV_SIZES: dict[int, int] = {}


def _worker_init(
    target_dir_s: str,
    elf_path_s: str,
    raw_path_s: str,
    spec_paths_s: list[str],
) -> None:
    global _W_HARNESS, _W_SPECS_BY_SYM, _W_INV_SIZES
    tdir = Path(target_dir_s)
    elf_p = Path(elf_path_s)
    raw_b = Path(raw_path_s).read_bytes()
    inv = json.loads((tdir / "verification/inventory.json").read_text(encoding="utf-8"))
    _W_INV_SIZES = {int(f["rva"], 16): int(f["size"]) for f in inv["functions"]}
    func_map, elf_syms = discover_provenance_funcs(tdir, elf_p)
    pending_stubs, json_specs = load_json_specs([Path(p) for p in spec_paths_s])
    all_specs = build_specs() + json_specs
    _W_SPECS_BY_SYM = {}
    for sp in all_specs:
        _W_SPECS_BY_SYM.setdefault(sp.c_symbol, []).append(sp)
    _W_HARNESS = DiffHarness(raw_b, elf_p, func_map, elf_syms, pending_stubs)


def _worker_run_batch(c_symbols: list[str]) -> list[tuple[str, list[dict[str, Any]], dict[str, Any], str | None]]:
    assert _W_HARNESS is not None
    out_batch = []
    for c_sym in c_symbols:
        raw_rva = _W_HARNESS.func_map[c_sym][0]
        raw_sz = _W_INV_SIZES.get(raw_rva, 4)
        sym_specs = _W_SPECS_BY_SYM.get(c_sym, [])
        case_results: list[dict[str, Any]] = []
        passing_pairs: list[tuple[CaseSpec, dict[str, Any]]] = []
        for sp in sym_specs:
            res, raw_out = _W_HARNESS.run_spec(sp)
            case_results.append(res)
            if res["passed"]:
                passing_pairs.append((sp, raw_out))
        if passing_pairs:
            killed, lit_kind = _W_HARNESS.check_stub_mutations(c_sym, passing_pairs, raw_sz)
        else:
            killed, lit_kind = {"bx_lr": False, "ret_0": False, "ret_1": False}, None
        all_killed = all(killed.values())
        mut_info = {
            "rva": hex(raw_rva),
            "case_count": len(passing_pairs),
            "killed": killed,
            "survived_stub_check": all_killed,
        }
        if lit_kind is not None:
            mut_info["literal_raw_stub_identity"] = lit_kind
        out_batch.append((c_sym, case_results, mut_info, lit_kind))
    return out_batch


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--target-dir", type=Path, default=Path("."))
    ap.add_argument("--elf", type=Path, required=True)
    ap.add_argument("--raw", type=Path, required=True)
    ap.add_argument("--report", type=Path, required=True)
    ap.add_argument("--extra-specs", type=Path, nargs="*", default=[])
    ap.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 4))
    args = ap.parse_args()

    tdir = args.target_dir.resolve()
    elf_path = args.elf.resolve()
    raw_path = args.raw.resolve()
    default_p2 = tdir / "verification/p2_semantics_specs.json"
    spec_paths = ([default_p2] if default_p2.is_file() else []) + [p.resolve() for p in args.extra_specs]

    func_map, elf_syms = discover_provenance_funcs(tdir, elf_path)
    pending_stubs, json_specs = load_json_specs(spec_paths)
    all_specs = build_specs() + json_specs

    tested_syms = {sp.c_symbol for sp in all_specs}
    untested = sorted(set(func_map.keys()) - tested_syms)
    if untested:
        print(f"[arm_semantics] UNTESTED @provenance symbols ({len(untested)}): {untested[:20]}", file=sys.stderr)
        return 1

    ordered_syms = sorted(func_map.keys())
    n_workers = max(1, min(args.jobs, len(ordered_syms)))

    # Chunk symbols evenly across workers
    batches: list[list[str]] = [[] for _ in range(n_workers)]
    for idx, sym in enumerate(ordered_syms):
        batches[idx % n_workers].append(sym)
    batches = [b for b in batches if b]

    cases_by_sym: dict[str, list[dict[str, Any]]] = {}
    mutation_results: dict[str, dict[str, Any]] = {}
    literal_stub_identities: dict[str, dict[str, Any]] = {}

    if len(batches) == 1:
        _worker_init(str(tdir), str(elf_path), str(raw_path), [str(p) for p in spec_paths])
        batch_outputs = [_worker_run_batch(batches[0])]
    else:
        ctx = mp.get_context("spawn")
        with ProcessPoolExecutor(
            max_workers=len(batches),
            mp_context=ctx,
            initializer=_worker_init,
            initargs=(str(tdir), str(elf_path), str(raw_path), [str(p) for p in spec_paths]),
        ) as pool:
            batch_outputs = list(pool.map(_worker_run_batch, batches))

    for b_out in batch_outputs:
        for c_sym, c_res, m_info, lit_kind in b_out:
            cases_by_sym[c_sym] = c_res
            mutation_results[c_sym] = m_info
            if lit_kind is not None:
                rva_i = func_map[c_sym][0]
                literal_stub_identities[c_sym] = {
                    "rva": hex(rva_i),
                    "stub_kind": lit_kind,
                }

    # Preserve deterministic case order matching all_specs
    cases: list[dict[str, Any]] = []
    seen_sym_emit: set[str] = set()
    for sp in all_specs:
        if sp.c_symbol in func_map and sp.c_symbol not in seen_sym_emit:
            seen_sym_emit.add(sp.c_symbol)
            cases.extend(cases_by_sym.get(sp.c_symbol, []))

    divergences = sum(1 for c in cases if not c["passed"])
    mutation_failures = [
        f"{c_sym} ({minfo['killed']})"
        for c_sym, minfo in sorted(mutation_results.items())
        if not minfo["survived_stub_check"]
    ]

    # Verify negative control in main process: deliberate 1-instruction corruption in aopf_strlen MUST be caught
    raw_bytes = raw_path.read_bytes()
    ctrl_harness = DiffHarness(raw_bytes, elf_path, func_map, elf_syms, pending_stubs)
    neg_spec = CaseSpec("neg_ctrl", "aopf_strlen", (SCRATCH_BASE,),
                        setup_fn=lambda uc: uc.mem_write(SCRATCH_BASE, b"hello\x00"))
    _, neg_raw = ctrl_harness.run_spec(neg_spec)
    neg_elf_addr = func_map["aopf_strlen"][1]
    neg_mut = ctrl_harness.vm_elf.exec_spec(
        neg_spec, neg_elf_addr, pending_stubs, patch=(neg_elf_addr, b"\x2a\x20\x70\x47")
    )
    neg_control_detected = len(ctrl_harness._compare(neg_spec, neg_raw, neg_mut)) > 0

    fully_passing_funcs = sum(
        1 for s in ordered_syms if cases_by_sym.get(s) and all(c["passed"] for c in cases_by_sym[s])
    )
    passed = (
        (divergences == 0)
        and (len(mutation_failures) == 0)
        and neg_control_detected
        and (fully_passing_funcs == len(func_map))
    )

    report = {
        "target": "aopf",
        "mode": "unicorn_armv7em_cortex_m4f_parallel_differential_mmio_rw_fpu_vt_stub_udf_rom_trap",
        "elf_path": str(args.elf),
        "raw_path": str(args.raw),
        "function_count": fully_passing_funcs,
        "case_count": len(cases),
        "divergences": divergences,
        "stub_mutation_verified_functions": sum(1 for v in mutation_results.values() if v["survived_stub_check"]),
        "literal_raw_stub_identities": literal_stub_identities,
        "stub_mutation_failures": mutation_failures,
        "negative_mutation_control_detected": neg_control_detected,
        "passed": passed,
        "per_function_mutation": {k: mutation_results[k] for k in sorted(mutation_results)},
        "cases": cases,
    }

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if not passed:
        for c in cases:
            if not c["passed"]:
                print(f"[arm_semantics] DIVERGENCE in {c['case']} ({c['c_symbol']}): {c['diffs']}", file=sys.stderr)
        for mf in mutation_failures[:30]:
            print(f"[arm_semantics] STUB_MUTATION_SURVIVED: {mf}", file=sys.stderr)
        return 1

    print(
        f"[aopf arm_semantics] PASS: {len(cases)}/{len(cases)} differential Cortex-M4F cases "
        f"(MMIO R+W, FPU, VT/callee trace, udf ROM trap) across {fully_passing_funcs} functions matched raw image "
        f"(0 divergences); {len(mutation_results)}/{len(mutation_results)} passed 3-stub mutation gate "
        f"({len(literal_stub_identities)} literal raw stub identities)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
