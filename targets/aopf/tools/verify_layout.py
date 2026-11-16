#!/usr/bin/env python3
"""ELF section, LMA/VMA segment, and non-overlap layout verifier for targets/aopf/."""
from __future__ import annotations

import argparse
from pathlib import Path
import sys
from elftools.elf.elffile import ELFFile

REQUIRED_EXACT_SECTIONS = {
    ".vectors":       (0x00000000, 0x000000C0),
    ".rtkit.header":  (0x000000C0, 0x00000048),
    ".text.reset":    (0x0000693C, None),
    ".text.hardfault":(0x000069D4, None),
    ".text.nmi":      (0x00006A24, None),
    ".text.svc_pendsv":(0x00006AB8, None),
    ".text":          (0x00007000, None),
    ".rva_4e518":     (0x0004E518, None),
    ".rodata.rom_bank":(0x00064000, 0x00013B14),
    ".data.ram_bank": (0x20800000, 0x0001A0B0),
    ".bss":           (0x2081A0B0, None),
    ".bss.psp_stack": (0x20899490, 0x00001400),
    ".bss.msp_stack": (0x2089B610, 0x00000400),
}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--target-dir", type=Path, default=Path("."))
    ap.add_argument("--elf", type=Path, required=True)
    ap.add_argument("--map", type=Path, required=False)
    args = ap.parse_args()

    errors: list[str] = []

    with open(args.elf, "rb") as f:
        elf = ELFFile(f)

        if elf.header["e_entry"] != 0x0000693D:
            errors.append(f"ELF entry 0x{elf.header['e_entry']:x} != 0x693d (Reset_Handler|1)")

        alloc_spans: list[tuple[int, int, str]] = []
        for sec in elf.iter_sections():
            flags = sec["sh_flags"]
            if (flags & 0x2) and sec["sh_size"] > 0:
                start = sec["sh_addr"]
                end = start + sec["sh_size"]
                alloc_spans.append((start, end, sec.name))

        for name, (exp_addr, exp_size) in REQUIRED_EXACT_SECTIONS.items():
            sec = elf.get_section_by_name(name)
            if sec is None:
                errors.append(f"missing required ELF section {name}")
                continue
            if sec["sh_addr"] != exp_addr:
                errors.append(
                    f"section {name} VMA 0x{sec['sh_addr']:08x} != expected 0x{exp_addr:08x}"
                )
            if exp_size is not None and sec["sh_size"] != exp_size:
                errors.append(
                    f"section {name} size 0x{sec['sh_size']:06x} != expected 0x{exp_size:06x}"
                )

        text_sec = elf.get_section_by_name(".text")
        if text_sec and (text_sec["sh_addr"] + text_sec["sh_size"] > 0x0004E510):
            errors.append(".text overflowed past 0x0004E510")

        # Verify RAM_DATA LMA == 0x00077B20 in PT_LOAD program headers
        ram_lma_ok = False
        for seg in elf.iter_segments():
            if seg["p_type"] == "PT_LOAD" and seg["p_vaddr"] == 0x20800000:
                if seg["p_paddr"] == 0x00077B20 and seg["p_filesz"] == 0x0001A0B0:
                    ram_lma_ok = True
                else:
                    errors.append(
                        f"RAM_DATA PT_LOAD p_paddr=0x{seg['p_paddr']:x} filesz=0x{seg['p_filesz']:x}"
                    )
        if not ram_lma_ok:
            errors.append("missing PT_LOAD for RAM_DATA @ VMA 0x20800000 LMA 0x00077B20")

        # Check zero overlap across all SHF_ALLOC sections
        alloc_spans.sort()
        for idx in range(len(alloc_spans) - 1):
            s0, e0, n0 = alloc_spans[idx]
            s1, e1, n1 = alloc_spans[idx + 1]
            if e0 > s1:
                errors.append(
                    f"overlapping SHF_ALLOC sections: {n0} [0x{s0:x}..0x{e0:x}) vs {n1} [0x{s1:x}..0x{e1:x})"
                )

    if args.map and not args.map.is_file():
        errors.append(f"linker map {args.map} not found")

    if errors:
        for err in errors:
            print(f"[verify_layout] ERROR: {err}", file=sys.stderr)
        return 1

    print(
        f"[aopf verify_layout] PASS: {len(alloc_spans)} non-overlapping SHF_ALLOC sections, "
        f"ROM [0x0..0x77b14) & SRAM [0x20800000..0x2089bed8) verified."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
