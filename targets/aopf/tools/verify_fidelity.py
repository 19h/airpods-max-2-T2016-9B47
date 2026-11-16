#!/usr/bin/env python3
"""Provenance, @shim discipline, anti-idiom, and ledger accounting verifier for targets/aopf/."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
from elftools.elf.elffile import ELFFile

COMMENT_OR_STRING_RE = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
    re.DOTALL,
)

BANNED_IDIOM_RE = re.compile(
    r"\b(?:HIWORD|LOWORD|LODWORD|HIDWORD|"
    r"BYTE[0-9]+|SBYTE[0-9]+|WORD[0-9]+|SWORD[0-9]+|DWORD[0-9]+|"
    r"__PAIR64__|__PAIR32__|__OFSUB__|__CFADD__|__CFSUB__|"
    r"COERCE_FLOAT|COERCE_UNSIGNED_INT)\b|"
    r"\bgoto\s+LABEL_[0-9A-Za-z_]*\b|"
    r"\b(?:v[1-9][0-9]*|a[1-9][0-9]*)\b"
)

TAG_RE = re.compile(
    r"/\*\s*@(provenance|shim|retained_native)\s+RVA\s+(0x[0-9a-fA-F]+)\s+([^\*\n]+?)\s*\*/"
)
IDENT_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
SKIP_IDENTS = {"__attribute__", "section", "aligned"}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--target-dir", type=Path, default=Path("."))
    ap.add_argument("--elf", type=Path, required=True)
    args = ap.parse_args()

    tdir = args.target_dir.resolve()
    inv_path = tdir / "verification/inventory.json"
    led_path = tdir / "verification/ledger.json"

    inv = json.loads(inv_path.read_text(encoding="utf-8"))
    led = json.loads(led_path.read_text(encoding="utf-8"))

    errors: list[str] = []

    if len(inv.get("functions", [])) != 3915:
        errors.append(f"inventory.json function count {len(inv.get('functions', []))} != 3915")
    if len(led.get("functions", [])) != 3915:
        errors.append(f"ledger.json function count {len(led.get('functions', []))} != 3915")

    inv_by_rva = {int(f["rva"], 16): f for f in inv["functions"]}
    led_by_rva = {int(f["rva"], 16): f for f in led["functions"]}

    for rva, lfunc in led_by_rva.items():
        if rva not in inv_by_rva:
            errors.append(f"ledger RVA {hex(rva)} missing from inventory.json")
        st = lfunc.get("status")
        if st not in {"reconstructed_c", "retained_native", "pending_c"}:
            errors.append(f"invalid status {st!r} at RVA {hex(rva)}")

    with open(args.elf, "rb") as f:
        elf = ELFFile(f)
        symtab = elf.get_section_by_name(".symtab")
        elf_funcs = {
            s.name: (s["st_value"] & ~1)
            for s in symtab.iter_symbols()
            if s["st_info"]["type"] == "STT_FUNC" and s["st_value"] != 0
        }

    src_files = sorted((tdir / "src").rglob("*.c"))
    hdr_files = sorted((tdir / "include").rglob("*.h"))
    for path in src_files + hdr_files:
        raw_txt = path.read_text(encoding="utf-8")
        cleaned = COMMENT_OR_STRING_RE.sub(
            lambda m: "\n" * m.group(0).count("\n"), raw_txt
        )
        for lineno, line in enumerate(cleaned.splitlines(), start=1):
            for match in BANNED_IDIOM_RE.finditer(line):
                errors.append(
                    f"banned Hex-Rays idiom {match.group(0)!r} in {path.relative_to(tdir)}:{lineno}"
                )

    seen_prov: dict[int, str] = {}
    seen_shim: dict[int, str] = {}
    seen_ret: dict[int, str] = {}
    for c_file in src_files:
        if c_file.name == "vt_trampolines.c":
            continue
        txt = c_file.read_text(encoding="utf-8")
        for m in TAG_RE.finditer(txt):
            kind, rva_s = m.group(1), m.group(2)
            rva = int(rva_s, 16)
            tail = txt[m.end() : m.end() + 350]
            c_sym = None
            for im in IDENT_RE.finditer(tail):
                if im.group(1) not in SKIP_IDENTS:
                    c_sym = im.group(1)
                    break
            if not c_sym:
                errors.append(f"could not locate C function after {rva_s} in {c_file}")
                continue
            if rva not in inv_by_rva:
                errors.append(f"tag RVA {rva_s} ({c_sym}) not in inventory.json")
                continue
            if c_sym not in elf_funcs:
                errors.append(f"C symbol {c_sym} ({rva_s}) missing from {args.elf}")

            expected_status = {
                "provenance": "reconstructed_c",
                "shim": "pending_c",
                "retained_native": "retained_native",
            }[kind]
            actual_status = led_by_rva[rva].get("status")
            if actual_status != expected_status:
                errors.append(
                    f"RVA {rva_s} ({c_sym}) tagged @{kind} but ledger status is {actual_status!r}"
                )
            if kind == "provenance":
                if "__asm__" in tail[:250] and "naked" in tail[:250]:
                    errors.append(f"naked asm function {c_sym} ({rva_s}) must not be reconstructed_c")
                if rva in seen_prov:
                    errors.append(f"duplicate @provenance RVA {hex(rva)}")
                seen_prov[rva] = c_sym
            elif kind == "shim":
                seen_shim[rva] = c_sym
            else:
                seen_ret[rva] = c_sym

    rec_in_ledger = sum(1 for f in led["functions"] if f["status"] == "reconstructed_c")
    ret_in_ledger = sum(1 for f in led["functions"] if f["status"] == "retained_native")
    pend_in_ledger = sum(1 for f in led["functions"] if f["status"] == "pending_c")
    if rec_in_ledger != len(seen_prov):
        errors.append(f"ledger reconstructed_c={rec_in_ledger} != source @provenance={len(seen_prov)}")
    if ret_in_ledger != len(seen_ret):
        errors.append(f"ledger retained_native={ret_in_ledger} != source @retained_native={len(seen_ret)}")

    if "known_deviations" not in led:
        errors.append("ledger.json missing known_deviations accounting section")

    if errors:
        for err in errors[:30]:
            print(f"[verify_fidelity] ERROR: {err}", file=sys.stderr)
        return 1

    print(
        f"[aopf verify_fidelity] PASS: {len(src_files)} C files, 0 banned idioms, "
        f"3915/3915 functions accounted ({rec_in_ledger} reconstructed_c, "
        f"{ret_in_ledger} retained_native, {pend_in_ledger} pending_c incl. {len(seen_shim)} @shim)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
