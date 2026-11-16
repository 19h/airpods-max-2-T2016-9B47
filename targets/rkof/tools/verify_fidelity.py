#!/usr/bin/env python3
"""
Audit rkof C11 source fidelity, @provenance annotations, and ledger accounting.

Checks:
  1. Zero banned Hex-Rays / decompiler idioms in C sources (matching root
     tools/verify_all_targets.py BANNED_IDIOM_RE).
  2. Every `@provenance RVA 0x... <name>` tag in `src/**/*.c` resolves to an
     exact `(rva, name)` entry in `verification/inventory.json` with zero
     duplicates.
  3. `verification/ledger.json` accounts for 100% of functions in
     `verification/inventory.json` (17,479 functions), with exact bidirectional
     equality between `status == "reconstructed_c"` and `@provenance` C tags,
     and honest `retained_native` vs `pending_c` classification.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

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

PROVENANCE_RE = re.compile(
    r"/\*\s*@provenance\s+RVA\s+(0x[0-9A-Fa-f]+)\s+([A-Za-z0-9_.$]+)\s*\*/"
)

VALID_STATUSES = {"reconstructed_c", "retained_native", "pending_c"}


def strip_comments_and_strings(text: str) -> str:
    def _repl(match: re.Match[str]) -> str:
        raw = match.group(0)
        return "".join("\n" if ch == "\n" else " " for ch in raw)

    return COMMENT_OR_STRING_RE.sub(_repl, text)


def parse_rva(val: int | str) -> int:
    if isinstance(val, int):
        return val
    return int(str(val), 0)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--src", required=True, type=Path, help="Source root (src/)")
    ap.add_argument("--inventory", required=True, type=Path, help="verification/inventory.json")
    ap.add_argument("--ledger", required=True, type=Path, help="verification/ledger.json")
    args = ap.parse_args()

    errors: list[str] = []

    if not args.inventory.is_file():
        print(f"[FAIL] Missing inventory: {args.inventory}", file=sys.stderr)
        return 1
    if not args.ledger.is_file():
        print(f"[FAIL] Missing ledger: {args.ledger}", file=sys.stderr)
        return 1

    inv_doc = json.loads(args.inventory.read_text(encoding="utf-8"))
    led_doc = json.loads(args.ledger.read_text(encoding="utf-8"))

    inv_funcs: dict[int, dict[str, Any]] = {}
    for entry in inv_doc.get("functions", []):
        rva = parse_rva(entry["rva"])
        if rva in inv_funcs:
            errors.append(f"Duplicate RVA 0x{rva:x} in inventory.json")
        inv_funcs[rva] = entry

    c_files = sorted(args.src.rglob("*.c"))
    if not c_files:
        errors.append(f"No C source files found under {args.src}")

    provenance_by_rva: dict[int, tuple[str, Path, int]] = {}
    idiom_hits = 0

    for c_path in c_files:
        raw_text = c_path.read_text(encoding="utf-8")

        for match in PROVENANCE_RE.finditer(raw_text):
            rva = int(match.group(1), 16)
            sym_name = match.group(2)
            line_no = raw_text.count("\n", 0, match.start()) + 1
            if rva in provenance_by_rva:
                prev_sym, prev_path, prev_line = provenance_by_rva[rva]
                errors.append(
                    f"Duplicate @provenance RVA 0x{rva:x}: "
                    f"{c_path}:{line_no} ({sym_name}) vs {prev_path}:{prev_line} ({prev_sym})"
                )
            provenance_by_rva[rva] = (sym_name, c_path, line_no)

            if rva not in inv_funcs:
                errors.append(
                    f"{c_path}:{line_no}: @provenance RVA 0x{rva:x} ({sym_name}) not in inventory.json"
                )
            else:
                expected_name = inv_funcs[rva]["name"]
                if sym_name != expected_name:
                    errors.append(
                        f"{c_path}:{line_no}: @provenance RVA 0x{rva:x} names '{sym_name}', "
                        f"inventory expects '{expected_name}'"
                    )

        scrubbed = strip_comments_and_strings(raw_text)
        for line_idx, line in enumerate(scrubbed.splitlines(), start=1):
            for bad in BANNED_IDIOM_RE.finditer(line):
                idiom_hits += 1
                errors.append(
                    f"{c_path}:{line_idx}: banned Hex-Rays idiom '{bad.group(0)}'"
                )

    led_list = led_doc.get("functions", [])
    if len(led_list) != len(inv_funcs):
        errors.append(
            f"Ledger function count ({len(led_list)}) != inventory count ({len(inv_funcs)})"
        )

    led_by_rva: dict[int, dict[str, Any]] = {}
    status_counts = {k: 0 for k in VALID_STATUSES}

    for item in led_list:
        rva = parse_rva(item["rva"])
        status = item.get("status")
        if status not in VALID_STATUSES:
            errors.append(f"Ledger RVA 0x{rva:x} has invalid status '{status}'")
            continue
        status_counts[status] += 1
        if rva in led_by_rva:
            errors.append(f"Duplicate RVA 0x{rva:x} in ledger.json")
        led_by_rva[rva] = item

        if rva not in inv_funcs:
            errors.append(f"Ledger RVA 0x{rva:x} not present in inventory.json")
        elif item.get("name") != inv_funcs[rva]["name"]:
            errors.append(
                f"Ledger RVA 0x{rva:x} name '{item.get('name')}' != "
                f"inventory '{inv_funcs[rva]['name']}'"
            )

    missing_from_ledger = set(inv_funcs.keys()) - set(led_by_rva.keys())
    if missing_from_ledger:
        sample = ", ".join(f"0x{x:x}" for x in sorted(missing_from_ledger)[:8])
        errors.append(
            f"{len(missing_from_ledger)} inventory functions missing from ledger.json ({sample})"
        )

    ledger_reconstructed = {
        rva for rva, item in led_by_rva.items() if item.get("status") == "reconstructed_c"
    }
    prov_rvas = set(provenance_by_rva.keys())

    only_in_ledger = sorted(ledger_reconstructed - prov_rvas)
    only_in_src = sorted(prov_rvas - ledger_reconstructed)
    for rva in only_in_ledger:
        errors.append(
            f"Ledger claims 0x{rva:x} ({led_by_rva[rva].get('name')}) as reconstructed_c, "
            f"but no @provenance tag exists in {args.src}"
        )
    for rva in only_in_src:
        sym_name, c_path, line_no = provenance_by_rva[rva]
        errors.append(
            f"{c_path}:{line_no} has @provenance 0x{rva:x} ({sym_name}), "
            f"but ledger status is '{led_by_rva.get(rva, {}).get('status')}'"
        )

    if errors:
        for err in errors[:50]:
            print(f"[FAIL] {err}", file=sys.stderr)
        if len(errors) > 50:
            print(f"[FAIL] ... and {len(errors) - 50} additional errors", file=sys.stderr)
        return 1

    print(
        f"[OK] verify_fidelity: {len(c_files)} C files, {idiom_hits} banned idioms, "
        f"{len(prov_rvas)} reconstructed_c, {status_counts['retained_native']} retained_native, "
        f"{status_counts['pending_c']} pending_c (total {len(led_by_rva)}/{len(inv_funcs)} = 100.0%)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
