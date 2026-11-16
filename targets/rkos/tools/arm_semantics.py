#!/usr/bin/env python3
"""Unicorn + Capstone ARMv7E-M differential execution harness for `targets/rkos/`.

Executes reconstructed C11 functions in `rkos.elf` head-to-head against the
original Apple Thumb-2 machine code in `../../rkos` inside paired Cortex-M4
Unicorn VMs, comparing return registers (`R0` / `R0:R1`) and mutated SRAM/scratch
regions byte-for-byte. Includes a negative mutation control proving the harness
detects a 1-bit divergence.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys
from typing import Any, Callable

from capstone import CS_ARCH_ARM, CS_MODE_MCLASS, CS_MODE_THUMB, Cs
from elftools.elf.elffile import ELFFile
from unicorn import UC_ARCH_ARM, UC_MODE_MCLASS, UC_MODE_THUMB, Uc
from unicorn.arm_const import (
    UC_ARM_REG_BASEPRI,
    UC_ARM_REG_C1_C0_2,
    UC_ARM_REG_FPEXC,
    UC_ARM_REG_IPSR,
    UC_ARM_REG_LR,
    UC_ARM_REG_MSP,
    UC_ARM_REG_PC,
    UC_ARM_REG_PRIMASK,
    UC_ARM_REG_PSP,
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_R3,
    UC_ARM_REG_SP,
    UC_CPU_ARM_CORTEX_M4,
)

ROM_BASE = 0x00000000
ROM_MAP_SIZE = 0x00020000
STOP_ADDR = 0x0001F000  # Thumb stop sentinel (`nop; nop`)
SCRATCH_BASE = 0x02000000
SCRATCH_SIZE = 0x00010000
SRAM_BASE = 0x20000000
SRAM_MAP_SIZE = 0x00030000
HOST_STACK_TOP = 0x2002F000
MMIO_LO_BASE = 0x40000000
MMIO_LO_SIZE = 0x00420000
SCS_BASE = 0xE000E000
SCS_SIZE = 0x00002000


class CortexM4Pair:
    def __init__(self, elf_path: Path, raw_path: Path) -> None:
        self.raw_bytes = raw_path.read_bytes()
        self.rom_raw = self.raw_bytes[0x0:0x17590]
        self.data_raw = self.raw_bytes[0x20000:0x20BA4]

        self.elf_segments: list[tuple[int, bytes]] = []
        self.symbols: dict[str, int] = {}
        with elf_path.open("rb") as fp:
            elf = ELFFile(fp)
            for seg in elf.iter_segments():
                if seg["p_type"] == "PT_LOAD" and int(seg["p_filesz"]) > 0:
                    self.elf_segments.append((int(seg["p_vaddr"]), seg.data()))
            symtab = elf.get_section_by_name(".symtab")
            if symtab is None:
                raise RuntimeError("rkos.elf missing .symtab")
            for sym in symtab.iter_symbols():
                if sym.name:
                    self.symbols[sym.name] = int(sym["st_value"])

        self.cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_MCLASS)

    def sym_addr(self, name: str) -> int:
        if name not in self.symbols:
            raise KeyError(f"symbol {name!r} missing from rkos.elf")
        return self.symbols[name] & ~1

    def verify_thumb_prologue(self, is_elf: bool, addr: int) -> str:
        uc = self._make_vm(is_elf)
        code = bytes(uc.mem_read(addr & ~1, 8))
        insns = list(self.cs.disasm(code, addr & ~1))
        if not insns:
            raise RuntimeError(f"invalid Thumb code at {hex(addr)} (elf={is_elf})")
        return f"{insns[0].mnemonic} {insns[0].op_str}".strip()

    def _make_vm(self, is_elf: bool) -> Uc:
        uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M4)
        uc.mem_map(ROM_BASE, ROM_MAP_SIZE)
        uc.mem_map(SCRATCH_BASE, SCRATCH_SIZE)
        uc.mem_map(SRAM_BASE, SRAM_MAP_SIZE)
        uc.mem_map(MMIO_LO_BASE, MMIO_LO_SIZE)
        uc.mem_map(SCS_BASE, SCS_SIZE)

        if is_elf:
            for vaddr, blob in self.elf_segments:
                uc.mem_write(vaddr, blob)
        else:
            uc.mem_write(0x0, self.rom_raw)
            uc.mem_write(SRAM_BASE, self.data_raw)

        # Thumb stop sentinel at STOP_ADDR
        uc.mem_write(STOP_ADDR, b"\x00\xbf\x00\xbf")

        # Enable Cortex-M4 FPU CP10/CP11 full access
        uc.reg_write(UC_ARM_REG_C1_C0_2, 0xF00000)
        uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
        uc.reg_write(UC_ARM_REG_MSP, HOST_STACK_TOP)
        uc.reg_write(UC_ARM_REG_PSP, HOST_STACK_TOP)
        uc.reg_write(UC_ARM_REG_SP, HOST_STACK_TOP)
        return uc

    def call_fn(
        self,
        uc: Uc,
        func_addr: int,
        args: list[int],
        max_insns: int = 2000000,
    ) -> tuple[int, int]:
        sp = HOST_STACK_TOP
        if len(args) > 4:
            stack_args = args[4:]
            if len(stack_args) % 2 == 1:
                stack_args = list(stack_args) + [0]
            sp -= 4 * len(stack_args)
            uc.mem_write(sp, struct.pack(f"<{len(stack_args)}I", *[a & 0xFFFFFFFF for a in stack_args]))

        reg_ids = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3]
        for i in range(min(4, len(args))):
            uc.reg_write(reg_ids[i], args[i] & 0xFFFFFFFF)

        uc.reg_write(UC_ARM_REG_SP, sp)
        uc.reg_write(UC_ARM_REG_MSP, sp)
        uc.reg_write(UC_ARM_REG_LR, STOP_ADDR | 1)
        uc.emu_start((func_addr & ~1) | 1, STOP_ADDR, count=max_insns)
        pc = uc.reg_read(UC_ARM_REG_PC) & ~1
        if pc != STOP_ADDR:
            raise RuntimeError(f"execution stopped at {hex(pc)} instead of STOP_ADDR (target={hex(func_addr)})")
        dump_va = getattr(uc, "_dump_special_regs_va", 0)
        if dump_va:
            uc.mem_write(
                dump_va,
                struct.pack(
                    "<III",
                    uc.reg_read(UC_ARM_REG_BASEPRI) & 0xFFFFFFFF,
                    uc.reg_read(UC_ARM_REG_PRIMASK) & 0xFFFFFFFF,
                    uc.reg_read(UC_ARM_REG_IPSR) & 0xFFFFFFFF,
                ),
            )
        return uc.reg_read(UC_ARM_REG_R0), uc.reg_read(UC_ARM_REG_R1)

    def run_diff_case(
        self,
        suite: str,
        name: str,
        raw_rva: int,
        elf_sym: str,
        setup_fn: Callable[[Uc], list[int]],
        compare_regions: list[tuple[int, int]] | None = None,
        check_r0: bool = True,
        check_r1: bool = False,
        mutate_candidate_r0: int = 0,
    ) -> dict[str, Any]:
        elf_addr = self.sym_addr(elf_sym)
        uc_raw = self._make_vm(is_elf=False)
        uc_elf = self._make_vm(is_elf=True)
        setattr(uc_raw, "_is_elf", False)
        setattr(uc_elf, "_is_elf", True)

        args_raw = setup_fn(uc_raw)
        args_elf = setup_fn(uc_elf)

        raw_r0, raw_r1 = self.call_fn(uc_raw, raw_rva, args_raw)
        elf_r0, elf_r1 = self.call_fn(uc_elf, elf_addr, args_elf)
        elf_r0 ^= mutate_candidate_r0

        mismatches: list[str] = []
        if check_r0 and raw_r0 != elf_r0:
            mismatches.append(f"R0 raw={hex(raw_r0)} vs elf={hex(elf_r0)}")
        if check_r1 and raw_r1 != elf_r1:
            mismatches.append(f"R1 raw={hex(raw_r1)} vs elf={hex(elf_r1)}")

        for r_addr, r_len in (compare_regions or []):
            b_raw = bytes(uc_raw.mem_read(r_addr, r_len))
            b_elf = bytes(uc_elf.mem_read(r_addr, r_len))
            if b_raw != b_elf:
                first_diff = next(i for i in range(r_len) if b_raw[i] != b_elf[i])
                mismatches.append(
                    f"mem[{hex(r_addr + first_diff)}] raw={hex(b_raw[first_diff])} vs elf={hex(b_elf[first_diff])}"
                )

        return {
            "suite": suite,
            "case": name,
            "raw_rva": hex(raw_rva),
            "elf_symbol": elf_sym,
            "elf_addr": hex(elf_addr),
            "raw_r0": hex(raw_r0),
            "elf_r0": hex(elf_r0),
            "matched": len(mismatches) == 0,
            "mismatches": mismatches,
        }


def build_test_suites(pair: CortexM4Pair) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []

    def add(res: dict[str, Any]) -> None:
        results.append(res)

    # --- Suite 1: Freestanding Libc Memory, String & 64-bit DivMod ---
    highbit_pat = bytes(
        [(0x80 ^ ((i * 73 + 0xFF) & 0xFF)) if (i % 5) != 0 else [0x80, 0xFF, 0xFE, 0x7F, 0x00, 0x01][(i // 5) % 6]
         for i in range(160)]
    )

    # 1a. memcpy (0x12C0, 3-arg entry) & memmove_body (0x12C6, 4-arg fall-through taking r3 = dst - src)
    for ln in (0, 1, 4, 15, 31, 64):
        for d_off, s_off in ((0, 0), (0, 1), (1, 0), (1, 3), (2, 2)):
            def make_copy_nonov(with_r3: bool, length: int = ln, do: int = d_off, so: int = s_off) -> Callable[[Uc], list[int]]:
                def _s(uc: Uc) -> list[int]:
                    uc.mem_write(0x02000000, highbit_pat[:128])
                    uc.mem_write(0x02000100, b"\xCC" * 96)
                    dst = 0x02000108 + do
                    src = 0x02000008 + so
                    return [dst, src, length] + ([(dst - src) & 0xFFFFFFFF] if with_r3 else [])
                return _s

            add(pair.run_diff_case(
                "libc", f"memcpy_len{ln}_d{d_off}_s{s_off}", 0x12C0, "rkos_memcpy",
                make_copy_nonov(False), [(0x02000100, 96)],
            ))
            add(pair.run_diff_case(
                "libc", f"memmove_nonov_len{ln}_d{d_off}_s{s_off}", 0x12C6, "rkos_memmove_body",
                make_copy_nonov(True), [(0x02000100, 96)],
            ))

    # 1b. Overlap cases on both 0x12C0 (entry) and 0x12C6 (body w/ r3 = dst - src): forward, backward, and dst == src
    for ln in (1, 4, 15, 31, 64):
        for base_off, delta in ((0, 1), (0, 3), (1, 4), (2, 7)):
            def make_memmove_fwd(with_r3: bool, length: int = ln, bo: int = base_off, dl: int = delta) -> Callable[[Uc], list[int]]:
                def _s(uc: Uc) -> list[int]:
                    uc.mem_write(0x02000000, b"\xDD" * 8 + highbit_pat[:112] + b"\xEE" * 8)
                    dst = 0x02000008 + bo
                    src = dst + dl
                    return [dst, src, length] + ([(dst - src) & 0xFFFFFFFF] if with_r3 else [])
                return _s

            def make_memmove_bwd(with_r3: bool, length: int = ln, bo: int = base_off, dl: int = delta) -> Callable[[Uc], list[int]]:
                def _s(uc: Uc) -> list[int]:
                    uc.mem_write(0x02000000, b"\xDD" * 8 + highbit_pat[:112] + b"\xEE" * 8)
                    src = 0x02000008 + bo
                    dst = src + dl
                    return [dst, src, length] + ([(dst - src) & 0xFFFFFFFF] if with_r3 else [])
                return _s

            add(pair.run_diff_case(
                "libc", f"memcpy_fwd_len{ln}_o{base_off}_d{delta}", 0x12C0, "rkos_memcpy",
                make_memmove_fwd(False), [(0x02000000, 128)],
            ))
            add(pair.run_diff_case(
                "libc", f"memcpy_bwd_len{ln}_o{base_off}_d{delta}", 0x12C0, "rkos_memcpy",
                make_memmove_bwd(False), [(0x02000000, 128)],
            ))
            add(pair.run_diff_case(
                "libc", f"memmove_fwd_len{ln}_o{base_off}_d{delta}", 0x12C6, "rkos_memmove_body",
                make_memmove_fwd(True), [(0x02000000, 128)],
            ))
            add(pair.run_diff_case(
                "libc", f"memmove_bwd_len{ln}_o{base_off}_d{delta}", 0x12C6, "rkos_memmove_body",
                make_memmove_bwd(True), [(0x02000000, 128)],
            ))

    add(pair.run_diff_case(
        "libc", "memcpy_same_ptr_31", 0x12C0, "rkos_memcpy",
        lambda uc: (uc.mem_write(0x02000000, highbit_pat[:48]), [0x02000004, 0x02000004, 31])[1],
        [(0x02000000, 48)],
    ))
    add(pair.run_diff_case(
        "libc", "memmove_same_ptr_31", 0x12C6, "rkos_memmove_body",
        lambda uc: (uc.mem_write(0x02000000, highbit_pat[:48]), [0x02000004, 0x02000004, 31, 0])[1],
        [(0x02000000, 48)],
    ))

    # 1c. memset (0x13D0): aligned & unaligned dst, lengths 0/1/4/15/31/64, high-bit & zero fill bytes
    for ln in (0, 1, 4, 15, 31, 64):
        for d_off, fill in ((0, 0x00), (0, 0x80), (1, 0xFF), (2, 0x5A), (3, 0x1FE)):
            def make_memset(length: int = ln, do: int = d_off, fv: int = fill) -> Callable[[Uc], list[int]]:
                def _s(uc: Uc) -> list[int]:
                    uc.mem_write(0x02000200, b"\xAA" * 96)
                    return [0x02000208 + do, fv, length]
                return _s

            add(pair.run_diff_case(
                "libc", f"memset_len{ln}_o{d_off}_f{fill:x}", 0x13D0, "rkos_memset",
                make_memset(), [(0x02000200, 96)],
            ))

    # 1d. memcmp (0x118C) & timingsafe_bcmp (0x5CD2): aligned/unaligned, word/tail/len0, high-bit unsigned
    memcmp_vectors = [
        ("eq_0", 0, 0, b"rkos", b"xxxx", 0),
        ("eq_8", 0, 0, b"rkosftab", b"rkosftab", 8),
        ("tail_8", 0, 0, b"rkosftab", b"rkosftac", 8),
        ("embedded_nul", 0, 0, b"abc\x00zzz", b"abc\x00yyy", 7),
        ("highbit_80_vs_7f_aligned", 0, 0, b"abcd\x80xyz", b"abcd\x7fxyz", 8),
        ("highbit_7f_vs_ff_unaligned", 1, 3, b"01234567\x7fabcdef", b"01234567\xffabcdef", 15),
        ("eq_64_unaligned", 1, 2, highbit_pat[:64], highbit_pat[:64], 64),
        ("diff_31_last", 2, 0, highbit_pat[:31], highbit_pat[:30] + b"\x00", 31),
    ]
    for vname, o1, o2, s1, s2, length in memcmp_vectors:
        def make_memcmp(b1: bytes = s1, b2: bytes = s2, ln: int = length, a1: int = o1, a2: int = o2) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                uc.mem_write(0x02000000 + a1, b1 if b1 else b"\x00")
                uc.mem_write(0x02000080 + a2, b2 if b2 else b"\x00")
                return [0x02000000 + a1, 0x02000080 + a2, ln]
            return _s

        add(pair.run_diff_case("libc", f"memcmp_{vname}", 0x118C, "rkos_memcmp", make_memcmp()))
        add(pair.run_diff_case("libc", f"timingsafe_bcmp_{vname}", 0x5CD2, "rkos_timingsafe_bcmp", make_memcmp()))

    def setup_mem_const_true(uc: Uc) -> list[int]:
        uc.mem_write(0x02000001, b"\x80" * 49)
        return [0x02000001, 0x80, 49]

    def setup_mem_const_false(uc: Uc) -> list[int]:
        uc.mem_write(0x02000000, b"\x00" * 31 + b"\x01" + b"\x00" * 16)
        return [0x02000000, 0x00, 48]

    add(pair.run_diff_case("libc", "mem_is_const_byte_true", 0x5C7E, "rkos_mem_is_const_byte", setup_mem_const_true))
    add(pair.run_diff_case("libc", "mem_is_const_byte_false", 0x5C7E, "rkos_mem_is_const_byte", setup_mem_const_false))

    def setup_strlcpy(uc: Uc) -> list[int]:
        uc.mem_write(0x02000040, b"AirPods3,6_Laguna\x00")
        return [0x02000000, 0x02000040, 11]

    add(pair.run_diff_case("libc", "strlcpy_trunc", 0x5D92, "rkos_strlcpy", setup_strlcpy, [(0x02000000, 12)]))

    def setup_stpncpy(uc: Uc) -> list[int]:
        uc.mem_write(0x02000000, b"\xff" * 16)
        uc.mem_write(0x02000040, b"rkos\x00")
        return [0x02000000, 0x02000040, 10]

    add(pair.run_diff_case("libc", "stpncpy_pad", 0x5D4C, "rkos_stpncpy", setup_stpncpy, [(0x02000000, 12)]))

    # 1e. strlen (0x1520) & strnlen (0x5DC4): alignments 0..3, lengths 0/1/4/15/31/64, high-bit bytes before NUL
    nonzero_highbit = bytes([b if b != 0 else 0x81 for b in highbit_pat[:80]])
    for ln in (0, 1, 4, 15, 31, 64):
        for s_off in (0, 1, 2, 3):
            def make_strlen(length: int = ln, so: int = s_off) -> Callable[[Uc], list[int]]:
                def _s(uc: Uc) -> list[int]:
                    uc.mem_write(0x02000000, b"\xFF" * 96)
                    uc.mem_write(0x02000004 + so, nonzero_highbit[:length] + b"\x00\x80\xFF")
                    return [0x02000004 + so]
                return _s

            add(pair.run_diff_case("libc", f"strlen_len{ln}_o{s_off}", 0x1520, "rkos_strlen", make_strlen()))

    for cap in (0, 1, 9, 31, 64):
        add(pair.run_diff_case(
            "libc", f"strnlen_cap{cap}", 0x5DC4, "rkos_strnlen",
            lambda uc, c=cap: (uc.mem_write(0x02000001, nonzero_highbit[:20] + b"\x00"), [0x02000001, c])[1],
        ))

    for idx, (num, den) in enumerate([
        (0x123456789ABCDEF0, 0x100003),
        (0xFFFFFFFFFFFFFFFF, 10),
        (987654321012345, 97),
    ]):
        def make_div(n: int = num, d: int = den) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                return [n & 0xFFFFFFFF, (n >> 32) & 0xFFFFFFFF, d & 0xFFFFFFFF, (d >> 32) & 0xFFFFFFFF, 0x02000300]
            return _s

        add(pair.run_diff_case("libc", f"udivmoddi4_{idx}", 0x1682, "rkos_udivmoddi4", make_div(), [(0x02000300, 8)], check_r1=True))
        add(pair.run_diff_case("libc", f"udivdi3_{idx}", 0x1674, "rkos_udivdi3", lambda uc, n=num, d=den: [n & 0xFFFFFFFF, n >> 32, d & 0xFFFFFFFF, d >> 32], check_r1=True))
        add(pair.run_diff_case("libc", f"umoddi3_{idx}", 0x186A, "rkos_umoddi3", lambda uc, n=num, d=den: [n & 0xFFFFFFFF, n >> 32, d & 0xFFFFFFFF, d >> 32], check_r1=True))

    # --- Suite 2: CRC-32, strtoul & snprintf ---
    for idx, payload in enumerate([b"", b"123456789", b"rkosftab\x02\x00\x00\x00", b"CLHE\x03\x03\x00\x00"]):
        def make_crc(pl: bytes = payload) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                uc.mem_write(0x02000000, pl if pl else b"\x00")
                return [0x02000000, len(pl)]
            return _s

        add(pair.run_diff_case("stdlib_printf", f"crc32_{idx}", 0x5568, "rkos_crc32", make_crc()))

    for idx, (text, base) in enumerate([
        (b"  0xDeadBeef!", 0),
        (b"0755", 0),
        (b"-42", 10),
        (b"99999999999999", 10),
        (b"101101", 2),
    ]):
        def make_strtoul(t: bytes = text, b: int = base) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                uc.mem_write(0x02000000, t + b"\x00")
                return [0x02000000, 0x02000080, b]
            return _s

        add(pair.run_diff_case("stdlib_printf", f"strtoul_{idx}", 0x5C10, "rkos_strtoul", make_strtoul(), [(0x02000080, 4)]))

    def setup_snprintf_1(uc: Uc) -> list[int]:
        uc.mem_write(0x02000080, b"RTKit:%s code=0x%08x dec=%d\x00")
        uc.mem_write(0x020000C0, b"b515d\x00")
        return [0x02000000, 64, 0x02000080, 0x020000C0, 0xCAFEBABE, (-12345) & 0xFFFFFFFF]

    add(pair.run_diff_case("stdlib_printf", "snprintf_fmt", 0x5B7E, "rkos_snprintf", setup_snprintf_1, [(0x02000000, 48)]))

    def setup_snprintf_trunc(uc: Uc) -> list[int]:
        uc.mem_write(0x02000080, b"0123456789abcdef\x00")
        return [0x02000000, 6, 0x02000080]

    add(pair.run_diff_case("stdlib_printf", "snprintf_trunc6", 0x5B7E, "rkos_snprintf", setup_snprintf_trunc, [(0x02000000, 8)]))

    # --- Suite 3: Image4 ASN.1 DER Decoder (`sub_582` / `sub_57C`) ---
    der_vectors = [
        ("der_seq_short", bytes([0x30, 0x04, 0x49, 0x4D, 0x34, 0x50])),
        ("der_oct_long128", bytes([0x04, 0x81, 0x80]) + b"\xAA" * 128),
        ("der_high_tag_999", bytes([0x9F, 0x87, 0x67, 0x03, 1, 2, 3])),
        ("der_reject_nonminimal", bytes([0x04, 0x81, 0x04, 1, 2, 3, 4])),
        ("der_reject_indefinite", bytes([0x30, 0x80, 0x00, 0x00])),
        ("der_reject_trunc", bytes([0x30, 0x10, 0x01, 0x02])),
    ]
    for dname, dbytes in der_vectors:
        def make_der(blob: bytes = dbytes) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                uc.mem_write(0x02000100, blob)
                uc.mem_write(0x02000000, struct.pack("<II", 0x02000100, len(blob)))
                return [0x02000000, 0x02000020]
            return _s

        # Compare return status; when status == 0, compare decoded 16-byte TLV struct!
        uc_probe = pair._make_vm(is_elf=False)
        p_args = make_der()(uc_probe)
        p_r0, _ = pair.call_fn(uc_probe, 0x57C, p_args)
        regions = [(0x02000020, 16)] if p_r0 == 0 else []
        add(pair.run_diff_case("asn1_der", dname, 0x57C, "rkos_der_decode", make_der(), regions))

    # --- Suite 4: Apple Corecrypto SHA-256, SHA-384, SHA-512 & HMAC ---
    def setup_sha256_blk(uc: Uc) -> list[int]:
        iv = bytes(uc.mem_read(0x000160C4, 32))
        uc.mem_write(0x02000000, iv)
        blk = b"abc" + b"\x80" + b"\x00" * 52 + struct.pack(">Q", 24)
        uc.mem_write(0x02000040, blk)
        return [0x02000000, 1, 0x02000040]

    add(pair.run_diff_case("corecrypto", "sha256_compress_abc", 0x11DC8, "rkos_sha256_compress", setup_sha256_blk, [(0x02000000, 32)], check_r0=False))

    def setup_sha512_blk(uc: Uc) -> list[int]:
        iv = bytes(uc.mem_read(0x000160E8, 64))
        uc.mem_write(0x02000000, iv)
        blk = b"abc" + b"\x80" + b"\x00" * 116 + struct.pack(">Q", 24)
        uc.mem_write(0x02000080, blk)
        return [0x02000000, 1, 0x02000080]

    add(pair.run_diff_case("corecrypto", "sha512_compress_abc", 0x11F38, "rkos_sha512_compress", setup_sha512_blk, [(0x02000000, 64)], check_r0=False))

    for idx, msg in enumerate([b"", b"abc", b"The quick brown fox jumps over the lazy dog" * 3]):
        def make_oneshot256(m: bytes = msg) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                uc.mem_write(0x02000100, m if m else b"\x00")
                return [0x0001612C, len(m), 0x02000100, 0x02000000]
            return _s

        def make_oneshot384(m: bytes = msg) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                uc.mem_write(0x02000100, m if m else b"\x00")
                return [0x00016154, len(m), 0x02000100, 0x02000000]
            return _s

        add(pair.run_diff_case("corecrypto", f"ccdigest_sha256_{idx}", 0x11910, "rkos_ccdigest_oneshot", make_oneshot256(), [(0x02000000, 32)], check_r0=False))
        add(pair.run_diff_case("corecrypto", f"ccdigest_sha384_{idx}", 0x11910, "rkos_ccdigest_oneshot", make_oneshot384(), [(0x02000000, 48)], check_r0=False))

    def setup_hmac256(uc: Uc) -> list[int]:
        uc.mem_write(0x02000100, b"Jefe")
        uc.mem_write(0x02000140, b"what do ya want for nothing?")
        return [0x0001612C, 4, 0x02000100, 28, 0x02000140, 0x02000000]

    add(pair.run_diff_case("corecrypto", "cchmac_sha256_rfc4231", 0x11BD4, "rkos_cchmac_oneshot", setup_hmac256, [(0x02000000, 32)], check_r0=False))

    # --- Suite 5: Hardened Kernel Heap (`0x2D84..0x53B4`) ---
    add(pair.run_diff_case(
        "kernel_heap",
        "heap_init_struct",
        0x4B58,
        "rkos_heap_init_struct",
        lambda uc: [0x2001DAA0],
        [(0x2001DAA0, 0x68)],
    ))

    def setup_malloc_first(uc: Uc) -> list[int]:
        uc.mem_write(0x20000198, struct.pack("<I", 0xA55AA55A))
        return [64]

    add(pair.run_diff_case(
        "kernel_heap",
        "rtkit_malloc_lazy_init_64B",
        0x515C,
        "rtkit_malloc",
        setup_malloc_first,
        [(0x2001DC38, 0x68), (0x20001078, 4)],
    ))

    def setup_calloc(uc: Uc) -> list[int]:
        uc.mem_write(0x20000198, struct.pack("<I", 0x12345678))
        return [6, 16]

    add(pair.run_diff_case(
        "kernel_heap",
        "rkos_calloc_6x16",
        0x5304,
        "rkos_calloc",
        setup_calloc,
        [(0x2001DC38, 0x68)],
    ))

    def setup_memalign(uc: Uc) -> list[int]:
        uc.mem_write(0x20000198, struct.pack("<I", 0x12345678))
        return [64, 80]

    add(pair.run_diff_case(
        "kernel_heap",
        "rkos_memalign_64_80",
        0x5344,
        "rkos_memalign",
        setup_memalign,
        [(0x2001DC38, 0x68)],
    ))

    def fn_target(uc: Uc, raw_rva: int, sym: str) -> int:
        return pair.sym_addr(sym) if getattr(uc, "_is_elf", False) else raw_rva

    def setup_alloc_free_coalesce(uc: Uc) -> list[int]:
        uc.mem_write(0x20000198, struct.pack("<I", 0xCAFE1234))
        malloc_addr = fn_target(uc, 0x515C, "rtkit_malloc")
        free_addr = fn_target(uc, 0x53B4, "rtkit_free")
        p1, _ = pair.call_fn(uc, malloc_addr, [48])
        p2, _ = pair.call_fn(uc, malloc_addr, [96])
        pair.call_fn(uc, free_addr, [p1])
        return [p2]

    add(pair.run_diff_case(
        "kernel_heap",
        "rtkit_free_and_coalesce",
        0x53B4,
        "rtkit_free",
        setup_alloc_free_coalesce,
        [(0x2001DC38, 0x68)],
        check_r0=False,
    ))

    # --- Suite 6: NVIC, Scheduler TailQ, Stack Watermark, Mutex & Memory Regions ---
    add(pair.run_diff_case(
        "nvic_sched",
        "nvic_register_handler_vec22",
        0x3620,
        "rkos_nvic_register_handler",
        lambda uc: [0, 22, 0x000042E1, 0x20000500],
        [(0x20000D50, 0x248)],
    ))

    add(pair.run_diff_case(
        "nvic_sched",
        "nvic_set_priority_vec22",
        0x3664,
        "rkos_nvic_set_priority",
        lambda uc: [0, 22, 8],
        [(0xE000E400, 16)],
    ))

    def setup_tailq(uc: Uc) -> list[int]:
        head_slot = 0x2001DCD8 + 8 * 7
        uc.mem_write(head_slot + 4, struct.pack("<I", head_slot))
        return [0x02000400, 7]

    add(pair.run_diff_case(
        "nvic_sched",
        "sched_tailq_enqueue_prio7",
        0x10668,
        "rkos_sched_tailq_enqueue",
        setup_tailq,
        [(0x2001DCD0, 0x60), (0x02000400, 8)],
    ))

    def setup_stack_scan(uc: Uc) -> list[int]:
        pair.call_fn(uc, fn_target(uc, 0x3B20, "rkos_stack_paint_words"), [0x02001000, 0x80])
        uc.mem_write(0x02001060, struct.pack("<I", 0xDEADBEEF))
        return [0x02001000, 0x80]

    add(pair.run_diff_case("nvic_sched", "stack_paint_and_scan", 0x108E4, "rkos_stack_scan_unused", setup_stack_scan, [(0x02001000, 0x80)]))

    def setup_mutex_lock(uc: Uc) -> list[int]:
        uc.mem_write(0x2000084C, struct.pack("<I", 0x20010000))
        return [0x02000500]

    add(pair.run_diff_case("nvic_sched", "mutex_lock_uncontended", 0x10A10, "mutex_lock", setup_mutex_lock, [(0x02000500, 16)]))

    def setup_mutex_recursive(uc: Uc) -> list[int]:
        uc.mem_write(0x2000084C, struct.pack("<I", 0x20010000))
        uc.mem_write(0x02000500, struct.pack("<I", 0x20010000))
        return [0x02000500]

    add(pair.run_diff_case("nvic_sched", "mutex_trylock_recursive", 0x10A30, "rkos_mutex_trylock", setup_mutex_recursive, [(0x02000500, 16)]))
    add(pair.run_diff_case("nvic_sched", "mutex_unlock_owned", 0x10938, "mutex_unlock", setup_mutex_recursive, [(0x02000500, 16)]))

    def setup_mem_region_with_addr(uc: Uc, addr: int) -> list[int]:
        uc.mem_write(0x20000198, struct.pack("<I", 0x55AA55AA))
        pair.call_fn(uc, fn_target(uc, 0x11528, "rkos_mem_region_ensure_init"), [])
        pair.call_fn(uc, fn_target(uc, 0x113E0, "rkos_mem_region_register"), [6, 0x20020000, 0x8000])
        return [addr]

    for mname, maddr in [
        ("ram_slot6_hit", 0x20024000),
        ("ram_slot7_hit", 0x00000100),
        ("nonram_slot8_miss", 0x20000100),
        ("unmapped_miss", 0x40000000),
    ]:
        add(pair.run_diff_case(
            "nvic_sched", f"mem_region_is_ram_{mname}", 0x1137A, "rkos_mem_region_is_ram",
            lambda uc, a=maddr: setup_mem_region_with_addr(uc, a),
        ))

    # --- Suite 7: Platform BMID, Stage-2 Remap, FTAB Stream & Static Constructors ---
    add(pair.run_diff_case(
        "platform",
        "fourcc_store_rkof",
        0x3FAC,
        "rkos_fourcc_store_global",
        lambda uc: [0x726B6F66],
        [(0x2001DA6C, 8)],
    ))

    def setup_bmid(uc: Uc) -> list[int]:
        uc.mem_write(0x02000020, struct.pack("<IIII", 0x44494D42, 0, 0x20251121, 0x40))
        uc.mem_write(0x02000044, struct.pack("<I", 0x60))
        uc.mem_write(0x02000060, struct.pack("<II", 4, 0x100))
        return [0x02000000, 0x02000020, 1, 0x20024000, 0x4000]

    add(pair.run_diff_case("platform", "bmid_validate_ok", 0x4DC0, "rkos_bmid_validate", lambda uc: (setup_bmid(uc), [0x02000020])[1]))
    add(pair.run_diff_case(
        "platform", "bmid_validate_bad_magic", 0x4DC0, "rkos_bmid_validate",
        lambda uc: (uc.mem_write(0x02000020, struct.pack("<IIII", 0x44494D43, 0, 0x20251121, 0x40)), [0x02000020])[1],
    ))
    add(pair.run_diff_case(
        "platform", "bmid_validate_nonzero_w1", 0x4DC0, "rkos_bmid_validate",
        lambda uc: (uc.mem_write(0x02000020, struct.pack("<IIII", 0x44494D42, 1, 0x20251121, 0x40)), [0x02000020])[1],
    ))
    add(pair.run_diff_case(
        "platform", "bmid_validate_bad_ver", 0x4DC0, "rkos_bmid_validate",
        lambda uc: (uc.mem_write(0x02000020, struct.pack("<IIII", 0x44494D42, 0, 0x20251120, 0x40)), [0x02000020])[1],
    ))
    add(pair.run_diff_case("platform", "bmid_validate_null", 0x4DC0, "rkos_bmid_validate", lambda uc: [0]))
    add(pair.run_diff_case("platform", "bmid_write_range", 0x4DE8, "rkos_bmid_write_range", setup_bmid, [(0x02000100, 0x60)]))

    add(pair.run_diff_case(
        "platform",
        "remap_build_table_6entries",
        0x4B84,
        "rkos_remap_build_table",
        lambda uc: [0x20000, 0x90000, 0x200004C8, 6],
        [(0x200004C8, 72), (0x20000FA0, 32), (0x2001DE18, 8)],
    ))

    def setup_stream_seek(uc: Uc) -> list[int]:
        uc.mem_write(0x02000000, struct.pack("<IIII", 0, 0x4000, 0x100, 0))
        return [0x02000000, 0x2800]

    add(pair.run_diff_case("platform", "ftab_stream_seek_ok", 0xEF7C, "rkos_ftab_stream_seek", setup_stream_seek, [(0x02000000, 16)]))
    add(pair.run_diff_case("platform", "ftab_stream_seek_oob", 0xEF7C, "rkos_ftab_stream_seek", lambda uc: (setup_stream_seek(uc), [0x02000000, 0x4001])[1], [(0x02000000, 16)]))

    add(pair.run_diff_case("platform", "ctor_qspi_objects", 0x12B28, "rkos_init_qspi_objects", lambda uc: [], [(0x20000BC0, 0x180)], check_r0=False))
    add(pair.run_diff_case("platform", "ctor_crashlog_plugin", 0x12BF8, "rkos_init_crashlog_plugin", lambda uc: [], [(0x2001DE08, 4)]))
    add(pair.run_diff_case("platform", "ctor_aux_heap", 0x12C24, "rkos_init_aux_heap", lambda uc: [], [(0x2001DAA0, 0x68)], check_r0=False))
    add(pair.run_diff_case("platform", "ctor_platform_drivers", 0x12C30, "rkos_init_platform_drivers", lambda uc: [], [(0x2001DB08, 0x130), (0x20000FC0, 0xB8)], check_r0=False))


    # --- Suite 8: Per-claim coverage (one or more direct cases for every reconstructed_c claim) ---
    def safe(suite: str, name: str, rva: int, sym: str, setup: Callable[[Uc], list[int]],
             regions: list[tuple[int, int]] | None = None, check_r0: bool = True) -> None:
        try:
            add(pair.run_diff_case(suite, name, rva, sym, setup, regions, check_r0=check_r0))
        except Exception as exc:  # crashes count as divergences, never as passes
            add({"suite": suite, "case": name, "raw_rva": hex(rva), "elf_symbol": sym, "elf_addr": "?",
                 "raw_r0": "?", "elf_r0": "?", "matched": False, "mismatches": [f"exception: {exc}"]})

    def wr(uc: Uc, addr: int, data: bytes) -> None:
        uc.mem_write(addr, data)

    cmp_pairs = [
        (0, 0, b"abc", b"abc"),
        (0, 1, b"abc", b"abd"),
        (1, 0, b"abd", b"abc"),
        (1, 3, b"ab", b"abc"),
        (0, 0, b"", b"x"),
        (2, 1, b"\xff", b"\x01"),
        (0, 2, b"0123456789abcdef_long_word_path\x80xyz", b"0123456789abcdef_long_word_path\x7fxyz"),
        (3, 1, nonzero_highbit[:35], nonzero_highbit[:35]),
    ]
    for idx, (o1, o2, x, y) in enumerate(cmp_pairs):
        def mk_cmp(a: bytes = x, b: bytes = y, a1: int = o1, a2: int = o2, n: int | None = None) -> Callable[[Uc], list[int]]:
            def _s(uc: Uc) -> list[int]:
                wr(uc, 0x02000000 + a1, a + b"\x00")
                wr(uc, 0x02000080 + a2, b + b"\x00")
                return [0x02000000 + a1, 0x02000080 + a2] + ([] if n is None else [n])
            return _s
        safe("coverage", f"strcmp_{idx}", 0x1478, "rkos_strcmp", mk_cmp())
        for n in (0, 2, 5, 24, 40):
            safe("coverage", f"strncmp_{idx}_{n}", 0x15A8, "rkos_strncmp", mk_cmp(n=n))

    for idx, ch in enumerate([ord("f"), ord("z"), 0, 0x80, 0x1FF]):
        safe("coverage", f"strchr_{idx}", 0x1448, "rkos_strchr",
             lambda uc, c=ch: (wr(uc, 0x02000001, b"rkos\x80\xffftab\x00"), [0x02000001, c])[1])
    for idx, (ch, n) in enumerate([(ord("t"), 11), (ord("q"), 11), (ord("r"), 0), (0, 11), (0x80, 11), (0x1FF, 11)]):
        safe("coverage", f"memchr_{idx}", 0x1168, "rkos_memchr",
             lambda uc, c=ch, ln=n: (wr(uc, 0x02000001, b"rkos\x00\x80\xffftab"), [0x02000001, c, ln])[1])
    for idx, (do, ln, cap, fv) in enumerate([(0, 0, 32, 0xA5), (1, 1, 32, 0x80), (0, 20, 32, 0xA5), (3, 31, 31, 0xFF)]):
        safe("coverage", f"memset_chk_{idx}", 0x5D34, "rkos_memset_chk",
             lambda uc, d=do, l=ln, dl=cap, f=fv: (wr(uc, 0x02000200, b"\x55" * 48), [0x02000204 + d, f, l, dl])[1],
             [(0x02000200, 48)])
    for idx, (do, so, ln) in enumerate([(0, 0, 0), (1, 0, 1), (0, 2, 19), (1, 3, 33)]):
        safe("coverage", f"memcpy_thunk_{idx}", 0x12B8, "memcpy_thunk",
             lambda uc, d=do, s=so, l=ln: (wr(uc, 0x02000000, highbit_pat[:64]), wr(uc, 0x02000100, b"\xCC" * 64),
                                           [0x02000104 + d, 0x02000004 + s, l])[2],
             [(0x02000100, 64)])

    for rva, sym in [(0x42E0, "rkos_nullsub_2"), (0x42E2, "rkos_nullsub_3"), (0x42E4, "rkos_nullsub_4"),
                     (0x4338, "rkos_nullsub_5"), (0x44C8, "rkos_nullsub_6"), (0x484C, "rkos_nullsub_7"),
                     (0x48B0, "rkos_nullsub_8"), (0x11374, "rkos_nullsub_1")]:
        safe("coverage", f"{sym}_passthrough", rva, sym, lambda uc: [0x13579BDF, 0x2468ACE0])

    def guard(uc: Uc) -> None:
        wr(uc, 0x20000198, struct.pack("<I", 0x5EED1234))

    heap_regions = [(0x2001DC38, 0x68), (0x20001078, 0xC8), (0x20001140, 0x200)]
    for rva, sym in [(0x7684, "j_rtkit_malloc"), (0x766C, "cxx_operator_new"), (0x768C, "cxx_operator_new_array")]:
        safe("coverage", f"{sym}_48", rva, sym, lambda uc: (guard(uc), [48])[1], heap_regions)

    def alloc_then(uc: Uc) -> list[int]:
        guard(uc)
        p1, _ = pair.call_fn(uc, fn_target(uc, 0x515C, "rtkit_malloc"), [40])
        pair.call_fn(uc, fn_target(uc, 0x515C, "rtkit_malloc"), [24])
        return [p1]

    for rva, sym in [(0x6B58, "j_rtkit_free"), (0x6CE4, "j_rtkit_free_0"), (0x6F18, "j_rtkit_free_1"),
                     (0x6F78, "j_rtkit_free_2"), (0x7050, "j_rtkit_free_3"), (0x7220, "j_rtkit_free_4"),
                     (0x72E4, "j_rtkit_free_5"), (0x73C4, "j_rtkit_free_6"), (0x73F0, "j_rtkit_free_7"),
                     (0x7460, "j_rtkit_free_8"), (0x74F8, "j_rtkit_free_9"), (0x7688, "j_rtkit_free_10")]:
        safe("coverage", f"{sym}_after_alloc", rva, sym, alloc_then, heap_regions, check_r0=False)
    safe("coverage", "heap_free_blk_after_alloc", 0x32C4, "heap_free_blk",
         lambda uc: [0x2001DC38, alloc_then(uc)[0]], heap_regions)
    for off in (0, -16, 4):
        safe("coverage", f"rkos_heap_blk_alloc_valid_off{off}", 0x2F44, "rkos_heap_blk_alloc_valid",
             lambda uc, o=off: [0x2001DC38, (alloc_then(uc)[0] + o) & 0xFFFFFFFF])
    for mname, probe_fn in [
        ("in_arena_blk", lambda p1: (p1 - 16) & 0xFFFFFFFF),
        ("below_arena", lambda _p1: 0x20001100),
        ("near_end_overflow", lambda _p1: 0x2001D130),
        ("null_oob", lambda _p1: 0x00000100),
    ]:
        safe("coverage", f"rkos_heap_blk_min_valid_{mname}", 0x2F3C, "rkos_heap_blk_min_valid",
             lambda uc, pf=probe_fn: [0x2001DC38, pf(alloc_then(uc)[0])])
    for idx, (st, ln) in enumerate([(0x20001200, 0x40), (0x20001100, 0x40), (0x2001D300, 0x1000)]):
        safe("coverage", f"heap_span_valid_{idx}", 0x31B8, "rkos_heap_span_valid",
             lambda uc, a=st, b=ln: (alloc_then(uc), [0x2001DC38, a, b])[1])
    safe("coverage", "alloc_tagged_handle", 0x113BC, "rkos_alloc_tagged_handle",
         lambda uc: (guard(uc), [0x20004000, 0xF0001234])[1], heap_regions)

    for idx, (val, signed, plus, pad) in enumerate([(0, 0, 0, 0), ((-12345) & (2**64 - 1), 1, 0, 0),
                                                     (2**63 + 5, 0, 0, 0), (777, 1, 1, 0), (42, 0, 0, 8)]):
        safe("coverage", f"format_i64_dec_{idx}", 0x5AE8, "rkos_format_i64_dec",
             lambda uc, v=val, sg=signed, pl=plus, pd=pad: (wr(uc, 0x02000000, b"\xcc" * 32),
                 [0x02000000, pd, v & 0xFFFFFFFF, v >> 32, sg, pl])[1], [(0x02000000, 32)])

    for rva, sym in [(0x5BF8, "rkos_vscnprintf"), (0x5B98, "rkos_vsnprintf")]:
        for idx, size in enumerate([64, 7, 0]):
            def mk_v(sz: int = size) -> Callable[[Uc], list[int]]:
                def _s(uc: Uc) -> list[int]:
                    wr(uc, 0x02000080, b"id=%u name=%s hex=%x\x00")
                    wr(uc, 0x020000C0, b"rkos\x00")
                    wr(uc, 0x02000300, struct.pack("<III", 4096, 0x020000C0, 0xBEEF))
                    wr(uc, 0x02000000, b"\xee" * 64)
                    return [0x02000000, sz, 0x02000080, 0x02000300]
                return _s
            safe("coverage", f"{sym}_{idx}", rva, sym, mk_v(), [(0x02000000, 64)])

    for idx, (a, b, n) in enumerate([(b"abcd", b"abcd", 4), (b"abcd", b"abce", 4), (b"x", b"y", 0)]):
        safe("coverage", f"cc_cmp_safe_{idx}", 0x118E6, "rkos_cc_cmp_safe",
             lambda uc, x=a, y=b, ln=n: (wr(uc, 0x02000000, x), wr(uc, 0x02000040, y), [ln, 0x02000000, 0x02000040])[2])
    safe("coverage", "store_be64", 0x12354, "rkos_store_be64", lambda uc: [0x89ABCDEF, 0x01234567, 0x02000000],
         [(0x02000000, 8)], check_r0=False)
    for pre in (0, 1):
        safe("coverage", f"sec_init_flag_{pre}", 0x118D0, "rkos_sec_init_flag",
             lambda uc, v=pre: (wr(uc, 0x2001DDE0, bytes([v])), wr(uc, 0x02000000, b"\xff" * 4), [0x02000000])[2],
             [(0x2001DDE0, 1), (0x02000000, 4)], check_r0=False)
    safe("coverage", "status_unsupported", 0x4998, "rkos_status_unsupported", lambda uc: [])

    def stream(uc: Uc) -> None:
        wr(uc, 0x02000000, struct.pack("<IIIII", 0x11, 0x4000, 0x100, 0x22, 0xB00C))
    safe("coverage", "ftab_stream_get_size", 0xEF98, "rkos_ftab_stream_get_size",
         lambda uc: (stream(uc), [0x02000000, 0x02000100])[1], [(0x02000100, 4)])
    safe("coverage", "ftab_stream_get_backend", 0xEF9E, "rkos_ftab_stream_get_backend",
         lambda uc: (stream(uc), [0x02000000, 0x02000100])[1], [(0x02000100, 4)])
    safe("coverage", "pair_fill_same", 0xEFA4, "rkos_pair_fill_same",
         lambda uc: [0xA1B2C3D4, 7, 0x02000100, 0x02000104], [(0x02000100, 8)])
    for idx, ln in enumerate([0, 7, 8, 36]):
        safe("coverage", f"stack_paint_words_{idx}", 0x3B20, "rkos_stack_paint_words",
             lambda uc, n=ln: [0x02000200, n], [(0x02000200, 48)])

    for ipsr_val in (0, 2, 15, 37):
        safe("coverage", f"get_ipsr_num_{ipsr_val}", 0x3594, "rkos_get_ipsr_num",
             lambda uc, iv=ipsr_val: (uc.reg_write(UC_ARM_REG_IPSR, iv), [0x55AA1234])[1])
    safe("coverage", "nvic_get_noop", 0x3834, "rkos_nvic_get_noop", lambda uc: [0x55AA1234])

    for iname, bpri, pmask in [
        ("open_0_0", 0x00, 0),
        ("open_lowprio_60", 0x60, 0),
        ("masked_eq_40", 0x40, 0),
        ("masked_hiprio_20", 0x20, 0),
        ("masked_primask_1", 0x00, 1),
    ]:
        def setup_irq_en(uc: Uc, bp: int = bpri, pm: int = pmask) -> list[int]:
            wr(uc, 0x2001F060, struct.pack("<I", 0x40))
            uc.reg_write(UC_ARM_REG_BASEPRI, bp)
            uc.reg_write(UC_ARM_REG_PRIMASK, pm)
            return [0x55AA1234]
        safe("coverage", f"irq_are_enabled_{iname}", 0x34A8, "rkos_irq_are_enabled", setup_irq_en)

    for bname, init_bpri in [("from_0", 0x00), ("from_60_raised", 0x60), ("from_20_kept", 0x20)]:
        def setup_raise_bpri(uc: Uc, bp: int = init_bpri) -> list[int]:
            wr(uc, 0x2001F060, struct.pack("<I", 0x40))
            wr(uc, 0x02000F00, b"\xFF" * 12)
            uc.reg_write(UC_ARM_REG_BASEPRI, bp)
            setattr(uc, "_dump_special_regs_va", 0x02000F00)
            return [0x55AA1234]
        safe("coverage", f"irq_save_and_raise_basepri_{bname}", 0x3470, "rkos_irq_save_and_raise_basepri",
             setup_raise_bpri, [(0x02000F00, 12)])

    def nvic(uc: Uc, ipsr_val: int = 0) -> None:
        wr(uc, 0x200002B8, struct.pack("<I", 72))
        wr(uc, 0xE000E100, struct.pack("<III", 0x00010021, 0x80000000, 0x5))
        wr(uc, 0xE000E200, struct.pack("<III", 0x00000300, 0x00000001, 0x0))
        wr(uc, 0xE000E300, struct.pack("<III", 0x00000004, 0x00000000, 0x2))
        wr(uc, 0xE000ED04, struct.pack("<I", 1 << 26))  # PENDSTSET (vec 15 pending)
        wr(uc, 0xE000ED24, struct.pack("<I", 1 << 11))  # SYSTICKACT (vec 15 active)
        uc.reg_write(UC_ARM_REG_IPSR, ipsr_val)
    nvic_regions = [(0xE000E100, 0x10), (0xE000E180, 0x10), (0xE000E200, 0x10), (0xE000E280, 0x10)]
    for vec in (15, 24, 18, 2, 3, 14, 16, 21, 48, 63, 66, 81, 87, 88, 200):
        for rva, sym in [(0x359C, "rkos_nvic_is_enabled"), (0x373C, "rkos_nvic_is_pending"),
                         (0x3988, "rkos_nvic_is_active"), (0x37D0, "rkos_nvic_set_pending"),
                         (0x3788, "rkos_nvic_clear_pending")]:
            safe("coverage", f"{sym}_v{vec}", rva, sym, lambda uc, v=vec: (nvic(uc), [0x20000F00, v])[1], nvic_regions)
    safe("coverage", "rkos_nvic_is_active_v2_ipsr2", 0x3988, "rkos_nvic_is_active",
         lambda uc: (nvic(uc, ipsr_val=2), [0x20000F00, 2])[1], nvic_regions)

    region_state = [(0x20001078, 0xC8)]
    def setup_regions_init(uc: Uc) -> None:
        guard(uc)
        pair.call_fn(uc, fn_target(uc, 0x11528, "rkos_mem_region_ensure_init"), [])
        pair.call_fn(uc, fn_target(uc, 0x113E0, "rkos_mem_region_register"), [0, 0x42000000, 0x1000])

    for idx, addr in enumerate([0x100, 0x17590, 0x20000100, 0x42000100, 0x2001F070, 0x40000000]):
        safe("coverage", f"mem_region_is_ro_{idx}", 0x11398, "rkos_mem_region_is_ro",
             lambda uc, a=addr: (setup_regions_init(uc), [a])[1])
        safe("coverage", f"mem_region_lookup_{idx}", 0x11634, "rkos_mem_region_lookup",
             lambda uc, a=addr: (setup_regions_init(uc), wr(uc, 0x02000100, b"\xff" * 4), [a, 0x02000100])[2],
             [(0x02000100, 4)])
    safe("coverage", "mem_region_register_thunk", 0x11376, "rkos_mem_region_register_thunk",
         lambda uc: (guard(uc), [3, 0x40000000, 0x1000])[1], region_state + heap_regions)
    safe("coverage", "mem_region_ensure_init", 0x11528, "rkos_mem_region_ensure_init",
         lambda uc: (guard(uc), [])[1], region_state + heap_regions, check_r0=False)

    for di, iv_len in ((0x0001612C, 32), (0x00016154, 64)):
        def ctx_setup(uc: Uc, d: int = di, ivl: int = iv_len, upd: bool = False) -> list[int]:
            iv_va = struct.unpack("<I", bytes(uc.mem_read(d + 0x14, 4)))[0]
            wr(uc, 0x02001000, b"\x00" * 0x200)
            wr(uc, 0x02001008, bytes(uc.mem_read(iv_va, ivl)))
            wr(uc, 0x02000400, bytes((i * 7 + 3) & 0xFF for i in range(300)))
            if upd:
                pair.call_fn(uc, fn_target(uc, 0x11B04, "rkos_ccdigest_update"), [d, 0x02001000, 300, 0x02000400])
            return [d, 0x02001000, 300, 0x02000400]
        ctx_len = 8 + iv_len + 2 * iv_len + 4
        safe("coverage", f"ccdigest_update_{iv_len}", 0x11B04, "rkos_ccdigest_update", ctx_setup,
             [(0x02001000, ctx_len)], check_r0=False)
        fin_rva, fin_sym = (0x119AC, "rkos_ccdigest_final_64be") if iv_len == 32 else (0x1220C, "rkos_ccdigest_final_128be")
        safe("coverage", f"{fin_sym}", fin_rva, fin_sym,
             lambda uc, f=ctx_setup: (f(uc, upd=True)[:2] + [0x02000800]),
             [(0x02000800, 64)], check_r0=False)
        for klen in (4, 200):
            def h_setup(uc: Uc, d: int = di, kl: int = klen) -> list[int]:
                wr(uc, 0x02000400, bytes((i * 13 + 1) & 0xFF for i in range(kl)))
                wr(uc, 0x02001000, b"\x00" * 0x400)
                return [d, 0x02001000, kl, 0x02000400]
            safe("coverage", f"cchmac_init_{iv_len}_{klen}", 0x11CAE, "rkos_cchmac_init", h_setup,
                 [(0x02001000, 0x300)], check_r0=False)
            def hf_setup(uc: Uc, h: Callable[[Uc], list[int]] = h_setup) -> list[int]:
                init_args = h(uc)
                pair.call_fn(uc, fn_target(uc, 0x11CAE, "rkos_cchmac_init"), init_args)
                return [init_args[0], 0x02001000, 0x02000800]
            safe("coverage", f"cchmac_final_{iv_len}_{klen}", 0x11C5C, "rkos_cchmac_final", hf_setup,
                 [(0x02000800, 64)], check_r0=False)

    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True, help="Path to rkos.elf")
    parser.add_argument("--reference", type=Path, required=True, help="Path to raw ../../rkos")
    parser.add_argument("--output", type=Path, required=True, help="Path to arm-semantics.json")
    args = parser.parse_args()

    pair = CortexM4Pair(args.elf, args.reference)
    # Capstone sanity check on entry instruction of SHA-256 compress in both images
    raw_insn = pair.verify_thumb_prologue(is_elf=False, addr=0x11DC8)
    elf_insn = pair.verify_thumb_prologue(is_elf=True, addr=pair.sym_addr("rkos_sha256_compress"))

    cases = build_test_suites(pair)
    # Divergences on functions the ledger classifies as pending_c (C shim only) are
    # recorded as evidence for the demotion, not as failures. Any divergence on a
    # reconstructed_c claim fails here and again in tools/verify_claims.py.
    ledger = json.loads((Path(__file__).resolve().parent.parent / "verification/ledger.json").read_text(encoding="utf-8"))
    pending = {int(f["rva"], 16) for f in ledger["functions"] if f["classification"] != "reconstructed_c"}
    documented = [c for c in cases if not c["matched"] and int(c["raw_rva"], 16) in pending]
    divergences = [c for c in cases if not c["matched"] and int(c["raw_rva"], 16) not in pending]

    # Negative mutation control: flip bit 0 of candidate R0 on crc32("123456789") and prove detection!
    neg_ctrl = pair.run_diff_case(
        "negative_control",
        "mutated_crc32_bit0_detected",
        0x5568,
        "rkos_crc32",
        lambda uc: (uc.mem_write(0x02000000, b"123456789"), [0x02000000, 9])[1],
        mutate_candidate_r0=1,
    )
    negative_control_detected = not neg_ctrl["matched"]

    passed = len(divergences) == 0 and len(cases) > 0 and negative_control_detected

    suite_counts: dict[str, int] = {}
    for c in cases:
        s = str(c["suite"])
        suite_counts[s] = suite_counts.get(s, 0) + 1

    report = {
        "target": "rkos",
        "engine": "unicorn-arm-mclass + capstone-thumb2",
        "capstone_sanity": {
            "raw_0x11dc8": raw_insn,
            "elf_rkos_sha256_compress": elf_insn,
        },
        "case_count": len(cases),
        "divergences": len(divergences),
        "documented_pending_divergences": len(documented),
        "suite_breakdown": suite_counts,
        "negative_mutation_control_detected": negative_control_detected,
        "cases": cases,
        "passed": passed,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if not passed:
        for d in divergences:
            print(f"[arm_semantics] DIVERGENCE in {d['suite']}::{d['case']}: {d['mismatches']}", file=sys.stderr)
        if not negative_control_detected:
            print("[arm_semantics] FAIL: negative mutation control did not trigger!", file=sys.stderr)
        return 1

    print(
        f"[arm_semantics] PASS: {len(cases) - len(documented)}/{len(cases)} ARMv7E-M Thumb-2 differential cases matched "
        f"(0 divergences on reconstructed_c across {len(suite_counts)} suites; "
        f"{len(documented)} documented divergences on pending_c shims; negative mutation control=DETECTED)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
