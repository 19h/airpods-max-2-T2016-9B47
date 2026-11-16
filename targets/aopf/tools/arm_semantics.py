#!/usr/bin/env python3
"""Unicorn ARMv7E-M Thumb-2 differential & stub-mutation harness for targets/aopf/.

Executes reconstructed C11 functions compiled into aopf.elf side-by-side against
the original firmware machine code in ../../../aopf at each function's original RVA,
comparing return registers (r0 / r0:r1), output memory buffers, global SRAM
mutations, CPU interrupt-mask registers (PRIMASK/BASEPRI), and full ordered MMIO
read+write traces (UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE).

When executing aopf.elf (is_elf=True), ROM_TEXT (0x108..0x64000) is initialized to
Thumb `udf #255` (0xDEFF) traps rather than raw firmware bytes, proving that no
`reconstructed_c` function calls into unported raw ROM_TEXT helpers. Also verifies
that every claimed `reconstructed_c` function kills all three constant-return stub
mutations (bx lr, movs r0,#0;bx lr, movs r0,#1;bx lr).
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import struct
import sys
from typing import Any, Callable

from elftools.elf.elffile import ELFFile
from unicorn import (
    Uc,
    UC_ARCH_ARM,
    UC_HOOK_MEM_READ,
    UC_HOOK_MEM_WRITE,
    UC_MEM_READ,
    UC_MEM_WRITE,
    UC_MODE_MCLASS,
    UC_MODE_THUMB,
    UC_PROT_ALL,
)
from unicorn.arm_const import (
    UC_ARM_REG_APSR,
    UC_ARM_REG_BASEPRI,
    UC_ARM_REG_C1_C0_2,
    UC_ARM_REG_FPEXC,
    UC_ARM_REG_IPSR,
    UC_ARM_REG_LR,
    UC_ARM_REG_PRIMASK,
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_R3,
    UC_ARM_REG_SP,
)

ROM_BASE = 0x00000000
ROM_MAP_SIZE = 0x00078000
STOP_ADDR = 0x00077FF0
CB_STUB_ADDR = 0x00077FE0

SRAM_BASE = 0x20800000
SRAM_MAP_SIZE = 0x000A0000
SCRATCH_BASE = 0x20880000
SCRATCH_SIZE = 0x00002000
INIT_SP = 0x2089A800

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

PROV_RE = re.compile(
    r"/\*\s*@provenance\s+RVA\s+(0x[0-9a-fA-F]+)\s+([^\*\n]+?)\s*\*/"
)
IDENT_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
SKIP_IDENTS = {"__attribute__", "section", "aligned"}


def discover_provenance_funcs(target_dir: Path, elf_path: Path) -> dict[str, tuple[int, int]]:
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
    return func_map


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


class DiffHarness:
    def __init__(self, raw_bytes: bytes, elf_path: Path, func_map: dict[str, tuple[int, int]]) -> None:
        self.raw_bytes = raw_bytes
        self.elf_alloc_sections: list[tuple[int, bytes]] = []
        with open(elf_path, "rb") as f:
            elf = ELFFile(f)
            for sec in elf.iter_sections():
                if (sec["sh_flags"] & 0x2) != 0 and sec["sh_type"] != "SHT_NOBITS" and sec["sh_size"] > 0:
                    self.elf_alloc_sections.append((sec["sh_addr"], sec.data()))
        self.func_map = func_map
        self.udf_rom_trap = b"\xff\xde" * (0x64000 // 2)

    def _make_uc(self, is_elf: bool, patch: tuple[int, bytes] | None = None) -> tuple[Uc, list[tuple[str, int, int, int]]]:
        uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        uc.mem_map(ROM_BASE, ROM_MAP_SIZE, UC_PROT_ALL)
        uc.mem_map(SRAM_BASE, SRAM_MAP_SIZE, UC_PROT_ALL)
        for mbase, msize in MMIO_RANGES:
            uc.mem_map(mbase, msize, UC_PROT_ALL)

        if is_elf:
            # Poison ROM_TEXT [0x0..0x64000) with Thumb `udf #255` (0xDEFF) so any accidental
            # branch into unported raw ROM_TEXT immediately faults instead of executing raw code.
            uc.mem_write(ROM_BASE, self.udf_rom_trap)
            for vaddr, data in self.elf_alloc_sections:
                uc.mem_write(vaddr, data)
        else:
            uc.mem_write(ROM_BASE, self.raw_bytes[:0x77B14])
            uc.mem_write(SRAM_BASE, self.raw_bytes[0x77B20 : 0x77B20 + 0x1A0B0])

        if patch is not None:
            uc.mem_write(patch[0], patch[1])

        uc.mem_write(STOP_ADDR, b"\x00\xbe\x00\xbe")
        # Callback helper at CB_STUB_ADDR: increments uint32 counter at 0x20881FC0 and returns 0
        cb_code = bytes.fromhex("02490a68521c0a6000207047c01f8820")
        uc.mem_write(CB_STUB_ADDR, cb_code)

        uc.reg_write(UC_ARM_REG_C1_C0_2, 0x00F00000)
        uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
        uc.reg_write(UC_ARM_REG_SP, INIT_SP)
        uc.reg_write(UC_ARM_REG_LR, STOP_ADDR | 1)
        uc.reg_write(UC_ARM_REG_APSR, 0)
        uc.reg_write(UC_ARM_REG_PRIMASK, 0)
        uc.reg_write(UC_ARM_REG_BASEPRI, 0)

        uc.mem_write(0x2089BED0, struct.pack("<I", 0x80))  # g_crit_basepri

        mmio_trace: list[tuple[str, int, int, int]] = []

        def on_mmio_access(uc_inst: Uc, access: int, address: int, size: int, value: int, _data: Any) -> None:
            mask = (1 << (size * 8)) - 1
            if access == UC_MEM_WRITE:
                mmio_trace.append(("W", address, size, value & mask))
            elif access == UC_MEM_READ:
                cur_val = int.from_bytes(uc_inst.mem_read(address, size), "little") & mask
                mmio_trace.append(("R", address, size, cur_val))

        for mbase, msize in MMIO_RANGES:
            uc.hook_add(
                UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE,
                on_mmio_access,
                begin=mbase,
                end=mbase + msize - 1,
            )

        return uc, mmio_trace

    def _exec_one(
        self, spec: CaseSpec, is_elf: bool, entry_pc: int, patch: tuple[int, bytes] | None = None
    ) -> dict[str, Any]:
        uc, mmio_trace = self._make_uc(is_elf, patch=patch)
        if spec.stack_words:
            sp = INIT_SP - 4 * ((len(spec.stack_words) + 1) & ~1)
            uc.reg_write(UC_ARM_REG_SP, sp)
            for idx, w in enumerate(spec.stack_words):
                uc.mem_write(sp + idx * 4, struct.pack("<I", w & 0xFFFFFFFF))

        reg_ids = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)
        for idx, val in enumerate(spec.args[:4]):
            uc.reg_write(reg_ids[idx], val & 0xFFFFFFFF)

        if spec.setup_fn is not None:
            spec.setup_fn(uc)
        mmio_trace.clear()

        uc.emu_start(entry_pc | 1, STOP_ADDR, count=50000)

        return {
            "r0": uc.reg_read(UC_ARM_REG_R0) & 0xFFFFFFFF,
            "r1": uc.reg_read(UC_ARM_REG_R1) & 0xFFFFFFFF,
            "scratch": bytes(uc.mem_read(SCRATCH_BASE, SCRATCH_SIZE)),
            "extra": [bytes(uc.mem_read(a, sz)) for (a, sz) in spec.extra_mem_spans],
            "mmio": list(mmio_trace),
            "sysregs": (
                uc.reg_read(UC_ARM_REG_PRIMASK) & 0xFFFFFFFF,
                uc.reg_read(UC_ARM_REG_BASEPRI) & 0xFFFFFFFF,
            ),
        }

    @staticmethod
    def _compare(spec: CaseSpec, raw_out: dict[str, Any], elf_out: dict[str, Any]) -> list[str]:
        diffs: list[str] = []
        if not spec.ignore_r0 and raw_out["r0"] != elf_out["r0"]:
            diffs.append(f"r0 raw=0x{raw_out['r0']:08x} elf=0x{elf_out['r0']:08x}")
        if spec.ret64 and raw_out["r1"] != elf_out["r1"]:
            diffs.append(f"r1 raw=0x{raw_out['r1']:08x} elf=0x{elf_out['r1']:08x}")
        if raw_out["scratch"] != elf_out["scratch"]:
            for i in range(len(raw_out["scratch"])):
                if raw_out["scratch"][i] != elf_out["scratch"][i]:
                    diffs.append(
                        f"scratch+0x{i:x} raw=0x{raw_out['scratch'][i]:02x} elf=0x{elf_out['scratch'][i]:02x}"
                    )
                    break
        if raw_out["extra"] != elf_out["extra"]:
            diffs.append("extra_mem_spans mismatch")
        if raw_out["mmio"] != elf_out["mmio"]:
            diffs.append(f"mmio raw={raw_out['mmio']} elf={elf_out['mmio']}")
        if spec.check_sysregs and raw_out["sysregs"] != elf_out["sysregs"]:
            diffs.append(f"sysregs raw={raw_out['sysregs']} elf={elf_out['sysregs']}")
        return diffs

    def run_spec(self, spec: CaseSpec) -> tuple[dict[str, Any], dict[str, Any]]:
        raw_rva, elf_addr = self.func_map[spec.c_symbol]
        raw_out = self._exec_one(spec, is_elf=False, entry_pc=raw_rva)
        elf_out = self._exec_one(spec, is_elf=True, entry_pc=elf_addr)
        diffs = self._compare(spec, raw_out, elf_out)
        res = {
            "case": spec.name,
            "c_symbol": spec.c_symbol,
            "raw_rva": hex(raw_rva),
            "elf_addr": hex(elf_addr),
            "raw_ret": hex((raw_out["r1"] << 32) | raw_out["r0"]) if spec.ret64 else ("void" if spec.ignore_r0 else hex(raw_out["r0"])),
            "elf_ret": hex((elf_out["r1"] << 32) | elf_out["r0"]) if spec.ret64 else ("void" if spec.ignore_r0 else hex(elf_out["r0"])),
            "mmio_events": len(elf_out["mmio"]),
            "passed": len(diffs) == 0,
            "diffs": diffs,
        }
        return res, raw_out

    def check_stub_mutations(
        self, c_symbol: str, specs_and_raw: list[tuple[CaseSpec, dict[str, Any]]]
    ) -> dict[str, bool]:
        _, elf_addr = self.func_map[c_symbol]
        killed: dict[str, bool] = {}
        for mut_name, mut_bytes in STUB_MUTATIONS.items():
            detected = False
            for spec, raw_out in specs_and_raw:
                mut_out = self._exec_one(spec, is_elf=True, entry_pc=elf_addr, patch=(elf_addr, mut_bytes))
                if len(self._compare(spec, raw_out, mut_out)) > 0:
                    detected = True
                    break
            killed[mut_name] = detected
        return killed


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
    # Explicit 4th-register delta_r3 tests (including mismatched r3 overlap & delta_r3 < len backward copy)
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
        # Bit 17 (0x00020000) is RX FIFO empty
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


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--target-dir", type=Path, default=Path("."))
    ap.add_argument("--elf", type=Path, required=True)
    ap.add_argument("--raw", type=Path, required=True)
    ap.add_argument("--report", type=Path, required=True)
    args = ap.parse_args()

    tdir = args.target_dir.resolve()
    raw_bytes = args.raw.read_bytes()
    func_map = discover_provenance_funcs(tdir, args.elf)

    harness = DiffHarness(raw_bytes, args.elf, func_map)
    specs = build_specs()

    # Enforce 100% coverage of @provenance symbols
    tested_syms = {sp.c_symbol for sp in specs}
    untested = sorted(set(func_map.keys()) - tested_syms)
    if untested:
        print(f"[arm_semantics] UNTESTED @provenance symbols: {untested}", file=sys.stderr)
        return 1

    cases: list[dict[str, Any]] = []
    by_func: dict[str, list[tuple[CaseSpec, dict[str, Any]]]] = {}

    for sp in specs:
        res, raw_out = harness.run_spec(sp)
        cases.append(res)
        if res["passed"]:
            by_func.setdefault(sp.c_symbol, []).append((sp, raw_out))

    divergences = sum(1 for c in cases if not c["passed"])

    mutation_results: dict[str, dict[str, Any]] = {}
    mutation_failures: list[str] = []
    for c_sym, pairs in sorted(by_func.items()):
        killed = harness.check_stub_mutations(c_sym, pairs)
        all_killed = all(killed.values())
        mutation_results[c_sym] = {
            "rva": hex(func_map[c_sym][0]),
            "case_count": len(pairs),
            "killed": killed,
            "survived_stub_check": all_killed,
        }
        if not all_killed:
            mutation_failures.append(f"{c_sym} ({killed})")

    # Verify negative control: a deliberate 1-instruction corruption in aopf_strlen MUST be caught
    neg_spec = CaseSpec("neg_ctrl", "aopf_strlen", (SCRATCH_BASE,),
                        setup_fn=lambda uc: uc.mem_write(SCRATCH_BASE, b"hello\x00"))
    _, neg_raw = harness.run_spec(neg_spec)
    neg_elf_addr = func_map["aopf_strlen"][1]
    neg_mut = harness._exec_one(neg_spec, is_elf=True, entry_pc=neg_elf_addr, patch=(neg_elf_addr, b"\x2a\x20\x70\x47"))
    neg_control_detected = len(harness._compare(neg_spec, neg_raw, neg_mut)) > 0

    passed = (divergences == 0) and (len(mutation_failures) == 0) and neg_control_detected and (len(by_func) == len(func_map))

    report = {
        "target": "aopf",
        "mode": "unicorn_armv7em_thumb2_differential_mmio_rw_udf_rom_trap_plus_stub_mutation",
        "elf_path": str(args.elf),
        "raw_path": str(args.raw),
        "function_count": len(by_func),
        "case_count": len(cases),
        "divergences": divergences,
        "stub_mutation_verified_functions": sum(1 for v in mutation_results.values() if v["survived_stub_check"]),
        "stub_mutation_failures": mutation_failures,
        "negative_mutation_control_detected": neg_control_detected,
        "passed": passed,
        "per_function_mutation": mutation_results,
        "cases": cases,
    }

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if not passed:
        for c in cases:
            if not c["passed"]:
                print(f"[arm_semantics] DIVERGENCE in {c['case']} ({c['c_symbol']}): {c['diffs']}", file=sys.stderr)
        for mf in mutation_failures:
            print(f"[arm_semantics] STUB_MUTATION_SURVIVED: {mf}", file=sys.stderr)
        return 1

    print(
        f"[aopf arm_semantics] PASS: {len(cases)}/{len(cases)} differential ARMv7E-M cases (MMIO R+W, udf ROM trap) "
        f"across {len(by_func)} functions matched raw image (0 divergences); "
        f"{len(by_func)}/{len(by_func)} killed all 3 constant-return stubs (bx lr / ret 0 / ret 1)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
