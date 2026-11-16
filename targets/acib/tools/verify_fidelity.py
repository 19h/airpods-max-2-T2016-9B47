#!/usr/bin/env python3
"""Source fidelity, Hex-Rays anti-idiom, and @provenance ledger audit for acib."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

COMMENT_OR_STRING_RE = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
    re.DOTALL,
)

BANNED_IDIOM_RE = re.compile(
    r"\b(?:HIWORD|LOWORD|LODWORD|HIDWORD|LOBYTE|HIBYTE|"
    r"BYTE[0-9]+|SBYTE[0-9]+|WORD[0-9]+|SWORD[0-9]+|DWORD[0-9]+|"
    r"_BYTE|_WORD|_DWORD|_QWORD|"
    r"__PAIR64__|__PAIR32__|__OFSUB__|__CFADD__|__CFSUB__|"
    r"__fastcall|__usercall|__userpurge|"
    r"COERCE_FLOAT|COERCE_UNSIGNED_INT)\b|"
    r"\bgoto\s+LABEL_[0-9A-Za-z_]*\b|"
    r"\b(?:v[1-9][0-9]*|a[1-9][0-9]*)\b"
)

PROVENANCE_RE = re.compile(
    r"/\*\s*@provenance\s+RVA\s+(0x[0-9a-fA-F]+)\s+([A-Za-z0-9_.$?]+)\s*\*/"
)

RETAINED_RE = re.compile(
    r"/\*\s*@retained_native\s+RVA\s+(0x[0-9a-fA-F]+)\s+([A-Za-z0-9_.$?]+)\s*\*/"
)


def norm_hex(val: str | int) -> int:
    return int(val, 16) if isinstance(val, str) else int(val)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--src-dir", type=Path, required=True)
    parser.add_argument("--include-dir", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--ledger", type=Path, required=True)
    args = parser.parse_args()

    inv = json.loads(args.inventory.read_text(encoding="utf-8"))
    led = json.loads(args.ledger.read_text(encoding="utf-8"))

    inv_by_rva: dict[int, str] = {}
    for fn in inv["functions"]:
        inv_by_rva[norm_hex(fn["rva"])] = fn.get("name") or fn["ida_name"]

    led_rec_by_rva: dict[int, str] = {}
    led_ret_by_rva: dict[int, str] = {}
    status_counts: dict[str, int] = {}
    for fn in led["functions"]:
        st = fn["status"]
        status_counts[st] = status_counts.get(st, 0) + 1
        rva = norm_hex(fn["rva"])
        fname = fn.get("ida_name") or fn["name"]
        if st == "reconstructed_c":
            led_rec_by_rva[rva] = fname
        elif st == "retained_native":
            led_ret_by_rva[rva] = fname

    errors: list[str] = []

    if len(inv["functions"]) != 9917:
        errors.append(f"inventory function count {len(inv['functions'])} != 9917")
    if len(led["functions"]) != 9917:
        errors.append(f"ledger function count {len(led['functions'])} != 9917")

    scan_files = sorted(
        list(args.src_dir.rglob("*.c")) + list(args.include_dir.rglob("*.h"))
    )

    src_provenance: dict[int, tuple[str, str]] = {}
    src_retained: dict[int, tuple[str, str]] = {}

    for fpath in scan_files:
        raw = fpath.read_text(encoding="utf-8")

        for m in PROVENANCE_RE.finditer(raw):
            rva = norm_hex(m.group(1))
            ida_name = m.group(2)
            if rva in src_provenance:
                errors.append(
                    f"duplicate @provenance 0x{rva:06x} in {fpath} and {src_provenance[rva][1]}"
                )
            src_provenance[rva] = (ida_name, str(fpath))

            if rva not in inv_by_rva:
                errors.append(f"{fpath}: @provenance RVA 0x{rva:06x} missing from inventory.json")
            elif inv_by_rva[rva] != ida_name:
                errors.append(
                    f"{fpath}: @provenance RVA 0x{rva:06x} name '{ida_name}' != inventory '{inv_by_rva[rva]}'"
                )

        for m in RETAINED_RE.finditer(raw):
            rva = norm_hex(m.group(1))
            ida_name = m.group(2)
            src_retained[rva] = (ida_name, str(fpath))
            if rva in led_rec_by_rva:
                errors.append(
                    f"{fpath}: @retained_native RVA 0x{rva:06x} falsely marked reconstructed_c in ledger.json"
                )

        cleaned = COMMENT_OR_STRING_RE.sub(
            lambda mm: "\n" * mm.group(0).count("\n"), raw
        )
        for lineno, line in enumerate(cleaned.splitlines(), start=1):
            for bad in BANNED_IDIOM_RE.finditer(line):
                errors.append(
                    f"{fpath}:{lineno}: banned decompiler idiom '{bad.group(0)}'"
                )

    missing_in_src = sorted(set(led_rec_by_rva) - set(src_provenance))
    extra_in_src = sorted(set(src_provenance) - set(led_rec_by_rva))
    for rva in missing_in_src:
        errors.append(
            f"ledger reconstructed_c 0x{rva:06x} ({led_rec_by_rva[rva]}) missing @provenance in src/"
        )
    for rva in extra_in_src:
        errors.append(
            f"src @provenance 0x{rva:06x} ({src_provenance[rva][0]}) not marked reconstructed_c in ledger.json"
        )

    if set(src_retained) != set(led_ret_by_rva):
        errors.append(
            f"@retained_native tags ({len(src_retained)}) != ledger retained_native ({len(led_ret_by_rva)})"
        )

    if errors:
        for err in errors[:40]:
            print(f"ERROR: {err}", file=sys.stderr)
        return 1

    print(
        f"verify_fidelity: PASS ({len(scan_files)} files scanned, "
        f"0 anti-idioms, {len(src_provenance)} reconstructed_c, "
        f"{len(src_retained)} retained_native, "
        f"{status_counts.get('pending_c', 0)} pending_c / 9917 IDB functions)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
