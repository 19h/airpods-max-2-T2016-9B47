#!/usr/bin/env python3
"""Mechanical claim & deviation gate for targets/aopf/.

Verifies that:
1. Every function marked `reconstructed_c` in verification/ledger.json has >=1
   passing Unicorn differential execution case in arm-semantics.json at its
   exact raw RVA, 0 divergences, and kills all 3 constant-return stubs
   (bx_lr, ret_0, ret_1).
2. Every function marked `retained_native` in verification/ledger.json (0 here)
   contains original raw instruction bytes verified byte-for-byte against raw.
3. All `known_deviations` raw code pointer counts in verification/ledger.json
   match a fresh scan of .rodata.rom_bank (0x64000..0x77B14) and .data.ram_bank
   (0x20800000..0x2081A0B0) against inventory.json and aopf.elf.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys
from elftools.elf.elffile import ELFFile


STUB_RAW_BYTES = {
    "bx_lr": b"\x70\x47",
    "ret_0": b"\x00\x20\x70\x47",
    "ret_1": b"\x01\x20\x70\x47",
}


def scan_region_ptrs(blob: bytes, inv_func_rvas: set[int], bridged_rvas: set[int]) -> dict[str, int]:
    odd_rom = 0
    func_ptrs = 0
    bridged = 0
    unreloc = 0
    for off in range(0, len(blob) - (len(blob) % 4), 4):
        w = struct.unpack_from("<I", blob, off)[0]
        if (w & 1) == 1:
            tgt = w & ~1
            if 0x00000108 <= tgt < 0x00064000:
                odd_rom += 1
                if tgt in inv_func_rvas:
                    func_ptrs += 1
                    if tgt in bridged_rvas:
                        bridged += 1
                    else:
                        unreloc += 1
    return {
        "odd_words_in_rom_text_range": odd_rom,
        "function_entry_pointers": func_ptrs,
        "bridged_to_reconstructed_elf_veneers": bridged,
        "unrelocated_raw_function_pointers": unreloc,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--target-dir", type=Path, default=Path("."))
    ap.add_argument("--elf", type=Path, required=True)
    ap.add_argument("--raw", type=Path, required=True)
    ap.add_argument("--semantics-report", type=Path, required=True)
    args = ap.parse_args()

    tdir = args.target_dir.resolve()
    inv = json.loads((tdir / "verification/inventory.json").read_text(encoding="utf-8"))
    led = json.loads((tdir / "verification/ledger.json").read_text(encoding="utf-8"))
    sem = json.loads(args.semantics_report.read_text(encoding="utf-8"))
    raw = args.raw.read_bytes()

    errors: list[str] = []

    if not sem.get("passed", False) or sem.get("divergences", -1) != 0:
        errors.append("arm-semantics.json did not pass with 0 divergences")
    if not sem.get("negative_mutation_control_detected", False):
        errors.append("arm-semantics.json negative mutation control did not fire")

    mut_map = sem.get("per_function_mutation", {})
    passed_cases_by_sym: dict[str, list[str]] = {}
    for c in sem.get("cases", []):
        if c.get("passed"):
            passed_cases_by_sym.setdefault(c["c_symbol"], []).append(c["case"])

    bridged_rvas: set[int] = set()
    elf_rom: dict[int, bytes] = {}
    with open(args.elf, "rb") as f:
        elf = ELFFile(f)
        for sec in elf.iter_sections():
            nm = sec.name
            if (
                nm in (".text.reset", ".text.corrupt_halt", ".text.hardfault", ".text.nmi", ".text.svc_pendsv", ".rva_4e518")
                or nm.startswith(".vt.")
            ):
                bridged_rvas.add(sec["sh_addr"] & ~1)
            if sec["sh_flags"] & 0x2 and sec["sh_size"] > 0 and sec["sh_type"] != "SHT_NOBITS":
                elf_rom[sec["sh_addr"]] = sec.data()

    rec_checked = 0
    ret_checked = 0
    for fn in led["functions"]:
        st = fn["status"]
        rva = int(fn["rva"], 16)
        if st == "reconstructed_c":
            rec_checked += 1
            csym = fn.get("c_symbol")
            ev = fn.get("evidence", [])
            if not csym or not ev:
                errors.append(f"reconstructed_c {fn['rva']} missing c_symbol or evidence")
                continue
            minfo = mut_map.get(csym)
            if not minfo:
                errors.append(f"reconstructed_c {csym} ({fn['rva']}) missing from arm-semantics.json")
                continue
            if int(minfo["rva"], 16) != rva:
                errors.append(f"RVA mismatch for {csym}: ledger={hex(rva)} sem={minfo['rva']}")
            if not minfo.get("survived_stub_check", False):
                errors.append(f"reconstructed_c {csym} failed 3-stub mutation check: {minfo.get('killed')}")
            lit_kind = minfo.get("literal_raw_stub_identity")
            if lit_kind is not None:
                sz = int(fn["size"])
                exp_b = STUB_RAW_BYTES.get(lit_kind)
                if exp_b is None or raw[rva : rva + sz] != exp_b:
                    errors.append(
                        f"reconstructed_c {csym} ({fn['rva']}) claims literal_raw_stub_identity={lit_kind} "
                        f"but raw[{hex(rva)}:{hex(rva + sz)}]={raw[rva : rva + sz].hex()}"
                    )
                led_lits = led.get("literal_raw_stub_identities", {})
                if led_lits.get(csym, {}).get("stub_kind") != lit_kind:
                    errors.append(f"reconstructed_c {csym} literal_raw_stub_identity missing/mismatched in ledger.json")
            actual_cases = passed_cases_by_sym.get(csym, [])
            if set(ev) != set(actual_cases) or len(ev) != len(actual_cases):
                errors.append(f"evidence mismatch for {csym}: ledger={ev} actual={actual_cases}")
        elif st == "retained_native":
            ret_checked += 1
            sz = int(fn["size"])
            matched = False
            for sec_vma, sec_data in elf_rom.items():
                if sec_vma <= rva and rva + sz <= sec_vma + len(sec_data):
                    if sec_data[rva - sec_vma : rva - sec_vma + sz] == raw[rva : rva + sz]:
                        matched = True
                    break
            if not matched:
                errors.append(f"retained_native {fn['rva']} does not byte-match raw firmware")

    inv_func_rvas = {int(f["rva"], 16) for f in inv["functions"]}
    expected_regions = {
        "runtime_vtor_table": scan_region_ptrs(raw[0x64000:0x64178], inv_func_rvas, bridged_rvas),
        "rom_rodata_vtables_and_tables": scan_region_ptrs(raw[0x64178:0x77AE0], inv_func_rvas, bridged_rvas),
        "cpp_init_array": scan_region_ptrs(raw[0x77AE0:0x77B14], inv_func_rvas, bridged_rvas),
        "ram_data_initialized_globals": scan_region_ptrs(raw[0x77B20 : 0x77B20 + 0x1A0B0], inv_func_rvas, bridged_rvas),
    }
    kd = led.get("known_deviations", {})
    kd_regs = kd.get("regions", {})
    for rname, exp_metrics in expected_regions.items():
        got = kd_regs.get(rname, {})
        for k, v in exp_metrics.items():
            if got.get(k) != v:
                errors.append(f"known_deviations.{rname}.{k}: ledger={got.get(k)} actual={v}")

    exp_unreloc = sum(m["unrelocated_raw_function_pointers"] for m in expected_regions.values())
    if kd.get("total_unrelocated_function_pointer_slots") != exp_unreloc:
        errors.append(
            f"known_deviations.total_unrelocated_function_pointer_slots: "
            f"{kd.get('total_unrelocated_function_pointer_slots')} != {exp_unreloc}"
        )

    if rec_checked != len(mut_map):
        errors.append(f"reconstructed_c count {rec_checked} != semantic tested functions {len(mut_map)}")

    if errors:
        for e in errors[:30]:
            print(f"[verify_claims] ERROR: {e}", file=sys.stderr)
        return 1

    print(
        f"[aopf verify_claims] PASS: {rec_checked}/{len(mut_map)} reconstructed_c verified by Unicorn + 3-stub "
        f"mutation gate, {ret_checked} retained_native, and {exp_unreloc} unrelocated raw pointer slots "
        f"accounted in known_deviations ({kd.get('total_bridged_pointer_slots')} bridged)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
