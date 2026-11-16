#!/usr/bin/env python3
"""
Verify linked rkof.elf linker map (rkof.map) against include/rkof/memmap.h.

Checks:
  1. Key ROM and SRAM loadable sections (.vectors, .rtkit.header, .text.entry,
     .text.veneers, .text.default_handler, .rodata.image, .data.image) are
     placed at their exact firmware VMAs and sizes.
  2. Every `#define STATE_*_BASE 0x...` macro in `include/rkof/memmap.h` has a
     matching `.state.<name>` output section in `rkof.map` at that exact VMA.
  3. Architectural veneer symbols (`Reset_Handler`, `Fault_Handler`,
     `NMI_Handler`, `SVCall_Handler`, `Default_Handler`) sit at their exact
     vector-table target VMAs (`0x1F8`, `0x290`, `0x2E0`, `0x374`, `0x16F70`).
  4. `RAM_BSS` ends at `0x200D46B8` and `.bss.floating` size is `0`.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

STATE_DEFINE_RE = re.compile(
    r"^\s*#define\s+(STATE_([A-Z0-9_]+)_BASE)\s+(0x[0-9A-Fa-f]+)u?\b"
)

# LLD map top-level output section line:
#  VMA      LMA        Size     Align Out     In      Symbol
MAP_OUT_SEC_RE = re.compile(
    r"^\s*([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+\d+\s+(\.[A-Za-z0-9_.]+)\s*$"
)

MAP_SYM_RE = re.compile(
    r"^\s*([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+\d+\s+([A-Za-z_][A-Za-z0-9_.$]*)\s*$"
)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--memmap", required=True, type=Path, help="include/rkof/memmap.h")
    ap.add_argument("--map", required=True, type=Path, help="build/rkof.map")
    args = ap.parse_args()

    if not args.memmap.is_file():
        print(f"[FAIL] Missing memmap header: {args.memmap}", file=sys.stderr)
        return 1
    if not args.map.is_file():
        print(f"[FAIL] Missing linker map: {args.map}", file=sys.stderr)
        return 1

    expected_states: dict[str, tuple[str, int]] = {}
    for line in args.memmap.read_text(encoding="utf-8").splitlines():
        m = STATE_DEFINE_RE.match(line)
        if m:
            macro_name, stem, hex_val = m.group(1), m.group(2), m.group(3)
            sec_name = f".state.{stem.lower()}"
            expected_states[sec_name] = (macro_name, int(hex_val, 16))

    sections: dict[str, tuple[int, int]] = {}
    symbols: dict[str, int] = {}

    for line in args.map.read_text(encoding="utf-8").splitlines():
        m_sec = MAP_OUT_SEC_RE.match(line)
        if m_sec:
            vma = int(m_sec.group(1), 16)
            size = int(m_sec.group(3), 16)
            sec_name = m_sec.group(4)
            sections[sec_name] = (vma, size)
            continue
        m_sym = MAP_SYM_RE.match(line)
        if m_sym:
            vma = int(m_sym.group(1), 16)
            sym = m_sym.group(4)
            symbols[sym] = vma

    errors: list[str] = []

    fixed_sections = [
        (".vectors", 0x00000000, 0x000000C0),
        (".rtkit.header", 0x000000C0, 0x00000048),
        (".text.entry", 0x00000108, None),
        (".text.veneers", 0x000001F8, None),
        (".text.default_handler", 0x00016F70, None),
        (".rodata.image", 0x001BFA00, 0x00033E9A),
        (".data.image", 0x20000000, 0x0000F138),
        (".state.bss_start", 0x2000F138, 0x00000004),
    ]

    for sec_name, exp_vma, exp_size in fixed_sections:
        if sec_name not in sections:
            errors.append(f"Missing required output section '{sec_name}' in {args.map}")
            continue
        got_vma, got_size = sections[sec_name]
        if got_vma != exp_vma:
            errors.append(
                f"Section {sec_name} VMA 0x{got_vma:08x} != expected 0x{exp_vma:08x}"
            )
        if exp_size is not None and got_size != exp_size:
            errors.append(
                f"Section {sec_name} size 0x{got_size:x} != expected 0x{exp_size:x}"
            )

    for sec_name, (macro_name, exp_vma) in sorted(
        expected_states.items(), key=lambda kv: kv[1][1]
    ):
        if sec_name not in sections:
            errors.append(
                f"Missing pinned state section '{sec_name}' for {macro_name} (0x{exp_vma:08x})"
            )
            continue
        got_vma, got_size = sections[sec_name]
        if got_vma != exp_vma:
            errors.append(
                f"Pinned section {sec_name} ({macro_name}) VMA 0x{got_vma:08x} != 0x{exp_vma:08x}"
            )
        if got_size == 0:
            errors.append(f"Pinned section {sec_name} ({macro_name}) has zero size")

    if ".state.irq_prio" in sections:
        prio_vma, prio_sz = sections[".state.irq_prio"]
        if prio_vma + prio_sz != 0x200D46B8:
            errors.append(
                f"RAM_BSS end 0x{prio_vma + prio_sz:08x} != expected 0x200D46B8"
            )

    if ".bss.floating" in sections and sections[".bss.floating"][1] != 0:
        errors.append(
            f".bss.floating must be empty, got 0x{sections['.bss.floating'][1]:x} bytes"
        )

    expected_veneers = {
        "rtkit_entry": 0x00000108,
        "Reset_Handler": 0x000001F8,
        "Fault_Handler": 0x00000290,
        "NMI_Handler": 0x000002E0,
        "SVCall_Handler": 0x00000374,
        "Default_Handler": 0x00016F70,
    }
    for sym, exp_addr in expected_veneers.items():
        got_addr = symbols.get(sym)
        if got_addr is None or (got_addr & ~1) != exp_addr or (got_addr & 1) != 1:
            errors.append(
                f"Thumb symbol {sym} at {hex(got_addr) if got_addr is not None else 'MISSING'} "
                f"!= expected 0x{exp_addr | 1:08x} (VMA 0x{exp_addr:08x} | Thumb)"
            )

    if errors:
        for err in errors:
            print(f"[FAIL] {err}", file=sys.stderr)
        return 1

    print(
        f"[OK] verify_layout: {len(fixed_sections)} fixed sections + "
        f"{len(expected_states)} pinned STATE_*_BASE blocks + "
        f"{len(expected_veneers)} architectural handler VMAs verified"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
