#!/usr/bin/env python3
"""
Mechanical claim, stub-mutation sensitivity, and partial-link deviation audit for rkof.

Enforces:
  1. 100% of `reconstructed_c` entries in `verification/ledger.json` have >= 1
     passing direct Unicorn Cortex-M4F differential test at their exact raw RVA
     in `arm-semantics.json` (0 divergences, 0 uncovered claims).
  2. Every `reconstructed_c` symbol resolves in `rkof.elf` `.symtab` inside
     compiled `.text` (never `.text.pending.*`) with `st_size > 4`.
  3. Live Unicorn stub-mutation sensitivity audit: replacing any `reconstructed_c`
     symbol in `rkof.elf` with `bx lr` (`70 47`), `movs r0, #0; bx lr`
     (`00 20 70 47`), or `movs r0, #1; bx lr` (`01 20 70 47`) causes differential
     failure against `../../../rkof` for all 3 stubs across that symbol's cases
     (0 mutation-insensitive claims).
  4. All 5 `retained_native` veneers are pinned at their exact raw RVAs in
     `rkof.elf` and verified byte-for-byte in `load-image.json`.
  5. Mechanical recount of raw Thumb code pointers in `.rodata.image`,
     `CTOR_EARLY`, and `.data.image` plus `pending_boundary.S` stub count matches
     `verification/ledger.json["known_deviations"]`.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import re
import struct
import sys
from pathlib import Path
from typing import Any

from elftools.elf.elffile import ELFFile

STUBS: dict[str, bytes] = {
    "bx_lr": b"\x70\x47",
    "ret0": b"\x00\x20\x70\x47",
    "ret1": b"\x01\x20\x70\x47",
}

ROM_TEXT_START = 0x000000C0
ROM_TEXT_END = 0x001BFA00


def load_arm_semantics_module(script_path: Path) -> Any:
    spec = importlib.util.spec_from_file_location("rkof_arm_semantics", script_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load {script_path}")
    mod = importlib.util.module_from_spec(spec)
    sys.modules["rkof_arm_semantics"] = mod
    spec.loader.exec_module(mod)
    return mod


def count_raw_code_pointers(
    blob: bytes, fn_status_by_rva: dict[int, str]
) -> tuple[int, dict[str, int]]:
    rom_thumb_words = 0
    by_status = {"pending_c": 0, "reconstructed_c": 0, "retained_native": 0}
    for off in range(0, len(blob) - 3, 4):
        word = struct.unpack_from("<I", blob, off)[0]
        if (word & 1) == 1 and ROM_TEXT_START <= (word & ~1) < ROM_TEXT_END:
            rom_thumb_words += 1
            target = word & ~1
            st = fn_status_by_rva.get(target)
            if st in by_status:
                by_status[st] += 1
    return rom_thumb_words, by_status


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--elf", required=True, type=Path, help="Built rkof.elf")
    ap.add_argument("--reference", required=True, type=Path, help="Raw rkof image")
    ap.add_argument("--ledger", required=True, type=Path, help="verification/ledger.json")
    ap.add_argument("--semantics", required=True, type=Path, help="arm-semantics.json")
    ap.add_argument("--load-image", required=True, type=Path, help="load-image.json")
    ap.add_argument(
        "--pending-asm",
        default=Path("src/rtkit/pending_boundary.S"),
        type=Path,
        help="Path to pending_boundary.S",
    )
    args = ap.parse_args()

    errors: list[str] = []

    raw = args.reference.read_bytes()
    led = json.loads(args.ledger.read_text(encoding="utf-8"))
    sem_doc = json.loads(args.semantics.read_text(encoding="utf-8"))
    load_doc = json.loads(args.load_image.read_text(encoding="utf-8"))

    if not sem_doc.get("passed", False) or int(sem_doc.get("divergence_count", 1)) != 0:
        errors.append("arm-semantics.json did not pass with 0 divergences")
    if not load_doc.get("passed", False):
        errors.append("load-image.json did not pass")

    # 1. Parse ELF symbols & section names
    elf_sym_info: dict[str, tuple[int, int, str]] = {}
    with args.elf.open("rb") as fh:
        elf = ELFFile(fh)
        symtab = elf.get_section_by_name(".symtab")
        if symtab is None:
            print("[FAIL] Missing .symtab in ELF", file=sys.stderr)
            return 1
        for sym in symtab.iter_symbols():
            if not sym.name:
                continue
            shndx = sym["st_shndx"]
            sec_name = ""
            if isinstance(shndx, int) and 0 < shndx < elf.num_sections():
                sec_name = elf.get_section(shndx).name
            val = int(sym["st_value"])
            sz = int(sym["st_size"])
            prev = elf_sym_info.get(sym.name)
            if prev is None or sz > prev[1]:
                elf_sym_info[sym.name] = (val, sz, sec_name)

    # 2. Index passing Unicorn cases from arm-semantics.json
    passing_by_rva: dict[int, list[tuple[str, str]]] = {}
    for suite in sem_doc.get("suites", []):
        sname = suite["suite"]
        for case in suite.get("cases", []):
            if case.get("passed"):
                rva = int(case["ref_rva"], 16)
                passing_by_rva.setdefault(rva, []).append(
                    (f"{sname}::{case['name']}", case["symbol"])
                )

    fn_status_by_rva: dict[int, str] = {}
    rec_entries: list[dict[str, Any]] = []
    ret_entries: list[dict[str, Any]] = []

    for fn in led.get("functions", []):
        rva = int(fn["rva"], 16)
        st = fn["status"]
        fn_status_by_rva[rva] = st
        if st == "reconstructed_c":
            rec_entries.append(fn)
        elif st == "retained_native":
            ret_entries.append(fn)

    # 3. Verify 100% claim coverage + ELF symbol screen
    for fn in rec_entries:
        rva = int(fn["rva"], 16)
        c_sym = fn.get("c_symbol") or fn["name"]
        cases_for_rva = passing_by_rva.get(rva, [])
        if not cases_for_rva:
            errors.append(
                f"reconstructed_c RVA 0x{rva:x} ({fn['name']} / {c_sym}) has 0 passing Unicorn cases"
            )
        info = elf_sym_info.get(c_sym)
        if info is None:
            errors.append(f"reconstructed_c symbol '{c_sym}' missing from ELF .symtab")
        else:
            _val, sz, sec_name = info
            if sz <= 4:
                errors.append(
                    f"reconstructed_c symbol '{c_sym}' (0x{rva:x}) has trivial ELF st_size={sz} (<= 4)"
                )
            if sec_name.startswith(".text.pending"):
                errors.append(
                    f"reconstructed_c symbol '{c_sym}' (0x{rva:x}) resolves to pending stub section {sec_name}"
                )

    # 4. Verify all 5 retained_native veneers are pinned at their exact raw RVAs
    veneer_checks = {v["name"]: v for v in load_doc.get("retained_native_checks", [])}
    for fn in ret_entries:
        rva = int(fn["rva"], 16)
        vname = fn["name"]
        info = elf_sym_info.get(vname)
        if info is None:
            errors.append(f"retained_native veneer '{vname}' missing from ELF .symtab")
        elif (info[0] & ~1) != rva:
            errors.append(
                f"retained_native veneer '{vname}' at 0x{info[0] & ~1:x} != raw RVA 0x{rva:x}"
            )
        vc = veneer_checks.get(vname)
        if vc is None or not vc.get("matched", False):
            errors.append(
                f"retained_native veneer '{vname}' not byte-verified in load-image.json"
            )

    # 5. Live Unicorn stub-mutation sensitivity audit across all 108 reconstructed_c functions
    sem_mod = load_arm_semantics_module(Path(__file__).resolve().parent / "arm_semantics.py")
    elf_segs, elf_syms = sem_mod.load_elf_segments_and_symbols(args.elf)
    cases = sem_mod.build_cases()

    orig_create_vm = sem_mod.create_vm
    active_mut: dict[str, Any] = {"addr": None, "stub": b""}

    def mutated_create_vm(
        is_elf: bool, raw_bytes: bytes, segs: list[tuple[int, bytes]]
    ) -> Any:
        mu = orig_create_vm(is_elf, raw_bytes, segs)
        if is_elf and active_mut["addr"] is not None:
            mu.mem_write(active_mut["addr"], active_mut["stub"])
        return mu

    sem_mod.create_vm = mutated_create_vm

    detections_by_rva: dict[int, list[dict[str, bool]]] = {}
    for case in cases:
        ref_out = sem_mod.run_one_vm(False, raw, elf_segs, elf_syms, case)
        det: dict[str, bool] = {}
        target_addr = elf_syms[case.symbol] & ~1
        for stub_name, stub_bytes in STUBS.items():
            active_mut["addr"] = target_addr
            active_mut["stub"] = stub_bytes
            try:
                mut_out = sem_mod.run_one_vm(True, raw, elf_segs, elf_syms, case)
                det[stub_name] = len(sem_mod.compare_outcomes(case, ref_out, mut_out)) > 0
            except Exception:  # noqa: BLE001
                det[stub_name] = True
            finally:
                active_mut["addr"] = None
        detections_by_rva.setdefault(case.ref_rva, []).append(det)

    weak_claims: list[str] = []
    for fn in rec_entries:
        rva = int(fn["rva"], 16)
        c_sym = fn.get("c_symbol") or fn["name"]
        case_dets = detections_by_rva.get(rva, [])
        if not case_dets or not all(
            any(cd[k] for cd in case_dets) for k in STUBS
        ):
            weak_claims.append(f"0x{rva:x} ({c_sym}): {case_dets}")

    if weak_claims:
        for wc in weak_claims:
            errors.append(f"Stub-mutation-insensitive reconstructed_c claim: {wc}")

    # 6. Mechanical recount of known_deviations against raw image & pending_boundary.S
    kd = led.get("known_deviations", {})
    ro_kd = kd.get("rodata_image_raw_code_pointers", {})
    da_kd = kd.get("data_image_raw_code_pointers", {})
    pb_kd = kd.get("pending_boundary_link_stubs", {})

    ro_total, ro_by_st = count_raw_code_pointers(raw[0x1BFA00:0x1F389A], fn_status_by_rva)
    ct_total, ct_by_st = count_raw_code_pointers(raw[0x1F37B8:0x1F388C], fn_status_by_rva)
    da_total, da_by_st = count_raw_code_pointers(
        raw[0x200000 : 0x200000 + 0xF138], fn_status_by_rva
    )

    if ro_kd.get("rom_text_thumb_words_total") != ro_total or ro_kd.get("idb_function_entry_words") != ro_by_st:
        errors.append(
            f".rodata.image pointer recount ({ro_total}, {ro_by_st}) != ledger known_deviations ({ro_kd})"
        )
    ctor_kd = ro_kd.get("ctor_early_subrange", {})
    if (
        ctor_kd.get("rom_text_thumb_words") != ct_total
        or ctor_kd.get("idb_pending_c_entry_words") != ct_by_st["pending_c"]
    ):
        errors.append(
            f"CTOR_EARLY pointer recount ({ct_total}, {ct_by_st['pending_c']}) != ledger ({ctor_kd})"
        )
    if da_kd.get("rom_text_thumb_words_total") != da_total or da_kd.get("idb_function_entry_words") != da_by_st:
        errors.append(
            f".data.image pointer recount ({da_total}, {da_by_st}) != ledger known_deviations ({da_kd})"
        )

    if args.pending_asm.is_file():
        stub_count = len(
            re.findall(
                r"^\s*PENDING_STUB\s+[A-Za-z0-9_]+",
                args.pending_asm.read_text(encoding="utf-8"),
                flags=re.MULTILINE,
            )
        )
        if pb_kd.get("stub_count") != stub_count:
            errors.append(
                f"pending_boundary.S stub count ({stub_count}) != ledger ({pb_kd.get('stub_count')})"
            )
    else:
        errors.append(f"Missing {args.pending_asm}")

    if errors:
        for err in errors:
            print(f"[FAIL] {err}", file=sys.stderr)
        return 1

    print(
        f"[OK] verify_claims: {len(rec_entries)}/{len(rec_entries)} reconstructed_c claims "
        f"Unicorn-verified & stub-mutation-sensitive (3/3 stubs defeated), "
        f"{len(ret_entries)}/{len(ret_entries)} retained_native veneers pinned & byte-matched, "
        f"known_deviations mechanically verified"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
