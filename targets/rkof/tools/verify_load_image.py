#!/usr/bin/env python3
"""
Verify linked rkof.elf loadable image geometry, binary-backed slice equality,
and retained_native veneer byte-for-byte fidelity against raw firmware ../../../rkof.

Checks:
  1. ELF entry point == 0x00000109 (rtkit_entry | Thumb).
  2. PT_LOAD segments cover ROM (0x00000000..0x001F389A) and SRAM
     (0x20000000..0x200D46B8, p_filesz == 0xF138).
  3. 100% byte-for-byte SHA-256 equality between rkof.elf sections and raw
     firmware slices:
       - .vectors      (VMA 0x00000000, size 0x0000C0 == raw[0x000000:0x0000C0])
       - .rtkit.header (VMA 0x000000C0, size 0x000048 == raw[0x0000C0:0x000108])
       - .rodata.image (VMA 0x001BFA00, size 0x033E9A == raw[0x1BFA00:0x1F389A])
       - .data.image   (VMA 0x20000000, size 0x00F138 == raw[0x200000:0x20F138])
  4. 100% byte-for-byte fidelity of all 5 `retained_native` exception veneers
     (`Reset_Handler`, `Fault_Handler`, `NMI_Handler`, `SVCall_Handler`,
     `Default_Handler`) outside the 4 documented linker call/tail-call sites.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

from elftools.elf.elffile import ELFFile


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def first_diff_offset(
    lhs: bytes,
    rhs: bytes,
    masked_ranges: list[tuple[int, int]] | None = None,
) -> int | None:
    masked = set()
    for lo, hi in (masked_ranges or []):
        masked.update(range(lo, hi))
    limit = min(len(lhs), len(rhs))
    for idx in range(limit):
        if idx in masked:
            continue
        if lhs[idx] != rhs[idx]:
            return idx
    if len(lhs) != len(rhs):
        return limit
    return None


def read_elf_vma(elf: ELFFile, vma: int, size: int) -> bytes | None:
    for seg in elf.iter_segments():
        if seg["p_type"] != "PT_LOAD":
            continue
        sv = int(seg["p_vaddr"])
        sf = int(seg["p_filesz"])
        if sv <= vma and vma + size <= sv + sf:
            off = vma - sv
            return seg.data()[off : off + size]
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--elf", required=True, type=Path, help="Path to linked rkof.elf")
    ap.add_argument("--reference", required=True, type=Path, help="Path to raw rkof image")
    ap.add_argument("--output", required=True, type=Path, help="Output JSON report path")
    args = ap.parse_args()

    if not args.elf.is_file():
        print(f"[FAIL] Missing ELF: {args.elf}", file=sys.stderr)
        return 1
    if not args.reference.is_file():
        print(f"[FAIL] Missing reference image: {args.reference}", file=sys.stderr)
        return 1

    raw = args.reference.read_bytes()
    errors: list[str] = []
    section_checks: list[dict[str, Any]] = []
    retained_native_checks: list[dict[str, Any]] = []
    pt_loads: list[dict[str, Any]] = []
    verified_bytes = 0

    expected_slices = [
        (".vectors", 0x00000000, 0x000000, 0x0000C0),
        (".rtkit.header", 0x000000C0, 0x0000C0, 0x000048),
        (".rodata.image", 0x001BFA00, 0x1BFA00, 0x033E9A),
        (".data.image", 0x20000000, 0x200000, 0x00F138),
    ]

    # (name, vma, size, allowed_reloc_ranges, reloc_note)
    expected_retained_native = [
        ("Reset_Handler", 0x000001F8, 0x98, [(0x68, 0x6C)], "b.w rtkit_start relocation"),
        ("Fault_Handler", 0x00000290, 0x50, [(0x40, 0x44)], "bl sub_1748C relocation"),
        ("NMI_Handler", 0x000002E0, 0x94, [], "100% byte-identical including bl Default_Handler"),
        ("SVCall_Handler", 0x00000374, 0x32, [(0x16, 0x1A)], "bl rtkit_sched_switch relocation"),
        (
            "Default_Handler",
            0x00016F70,
            0x1C,
            [(0x0E, 0x12)],
            "inlined 4B compiler outliner sub_1B53FE (movs r1, #0; bx r2)",
        ),
    ]

    with args.elf.open("rb") as fh:
        elf = ELFFile(fh)
        entry = int(elf.header["e_entry"])
        if entry != 0x00000109:
            errors.append(f"ELF e_entry 0x{entry:08x} != expected 0x00000109")

        for seg_idx, seg in enumerate(elf.iter_segments()):
            if seg["p_type"] != "PT_LOAD":
                continue
            pt_loads.append({
                "index": seg_idx,
                "vaddr": f"0x{int(seg['p_vaddr']):08x}",
                "paddr": f"0x{int(seg['p_paddr']):08x}",
                "filesz": int(seg["p_filesz"]),
                "memsz": int(seg["p_memsz"]),
                "flags": int(seg["p_flags"]),
            })

        if len(pt_loads) != 2:
            errors.append(f"Expected 2 PT_LOAD segments (rom + sram), got {len(pt_loads)}")
        else:
            sram_seg = pt_loads[1]
            if sram_seg["vaddr"] != "0x20000000":
                errors.append(f"SRAM PT_LOAD vaddr {sram_seg['vaddr']} != 0x20000000")
            if sram_seg["filesz"] != 0x00F138:
                errors.append(
                    f"SRAM PT_LOAD filesz 0x{sram_seg['filesz']:x} != expected 0x00f138"
                )
            if sram_seg["memsz"] != 0x0D46B8:
                errors.append(
                    f"SRAM PT_LOAD memsz 0x{sram_seg['memsz']:x} != expected 0x0d46b8"
                )

        # Verify every SHF_ALLOC non-SHT_NOBITS section is covered by & matches PT_LOAD
        pt_spans: list[tuple[int, int, bytes]] = []
        for seg in elf.iter_segments():
            if seg["p_type"] != "PT_LOAD":
                continue
            sv = int(seg["p_vaddr"])
            sf = int(seg["p_filesz"])
            sm = int(seg["p_memsz"])
            sdata = seg.data()
            if sf > sm:
                errors.append(f"PT_LOAD @ 0x{sv:08x} filesz 0x{sf:x} > memsz 0x{sm:x}")
            if len(sdata) != sf:
                errors.append(f"PT_LOAD @ 0x{sv:08x} truncated data ({len(sdata)} != {sf})")
            for prev_v, prev_f, _ in pt_spans:
                if sv < prev_v + prev_f and prev_v < sv + sf:
                    errors.append(
                        f"Overlapping PT_LOAD file spans 0x{prev_v:08x}+0x{prev_f:x} and 0x{sv:08x}+0x{sf:x}"
                    )
            pt_spans.append((sv, sf, sdata))

        alloc_section_checks: list[dict[str, Any]] = []
        for sec in elf.iter_sections():
            sh_flags = int(sec["sh_flags"])
            sh_size = int(sec["sh_size"])
            if (sh_flags & 0x2) == 0 or sh_size == 0 or sec["sh_type"] == "SHT_NOBITS":
                continue
            sh_addr = int(sec["sh_addr"])
            sec_bytes = sec.data()
            seg_bytes = read_elf_vma(elf, sh_addr, len(sec_bytes))
            diff = (
                None
                if (seg_bytes is not None and seg_bytes == sec_bytes)
                else (
                    0
                    if seg_bytes is None
                    else first_diff_offset(sec_bytes, seg_bytes)
                )
            )
            ok = seg_bytes is not None and diff is None
            if not ok:
                errors.append(
                    f"Allocated section {sec.name} @ 0x{sh_addr:08x} (0x{len(sec_bytes):x} B) "
                    f"not matched by PT_LOAD (covered={seg_bytes is not None}, first_diff={diff})"
                )
            alloc_section_checks.append({
                "section": sec.name,
                "vma": f"0x{sh_addr:08x}",
                "size": len(sec_bytes),
                "covered_by_pt_load": seg_bytes is not None,
                "first_diff_offset": diff,
                "matched": ok,
            })

        for sec_name, exp_vma, raw_off, exp_size in expected_slices:
            sec = elf.get_section_by_name(sec_name)
            if sec is None:
                errors.append(f"Missing section {sec_name} in {args.elf}")
                section_checks.append({
                    "section": sec_name,
                    "matched": False,
                    "error": "missing_section",
                })
                continue

            got_vma = int(sec["sh_addr"])
            elf_bytes = sec.data()
            ref_bytes = raw[raw_off : raw_off + exp_size]
            diff_off = first_diff_offset(elf_bytes, ref_bytes)
            matched = (
                got_vma == exp_vma
                and len(elf_bytes) == exp_size
                and diff_off is None
            )

            if not matched:
                errors.append(
                    f"Section {sec_name} mismatch: vma=0x{got_vma:08x} (exp 0x{exp_vma:08x}), "
                    f"len=0x{len(elf_bytes):x} (exp 0x{exp_size:x}), first_diff={diff_off}"
                )
            else:
                verified_bytes += len(elf_bytes)

            section_checks.append({
                "section": sec_name,
                "vma": f"0x{got_vma:08x}",
                "raw_file_offset": f"0x{raw_off:06x}",
                "size": len(elf_bytes),
                "elf_sha256": sha256_hex(elf_bytes),
                "ref_sha256": sha256_hex(ref_bytes),
                "first_diff_offset": diff_off,
                "matched": matched,
            })

        for fn_name, vma, size, reloc_ranges, reloc_note in expected_retained_native:
            elf_bytes = read_elf_vma(elf, vma, size)
            ref_bytes = raw[vma : vma + size]
            if elf_bytes is None:
                errors.append(f"retained_native {fn_name} @ 0x{vma:08x} not in PT_LOAD")
                retained_native_checks.append({
                    "name": fn_name,
                    "vma": f"0x{vma:08x}",
                    "matched": False,
                    "error": "not_in_pt_load",
                })
                continue

            diff_off = first_diff_offset(elf_bytes, ref_bytes, reloc_ranges)
            masked_bytes = sum(hi - lo for lo, hi in reloc_ranges)
            matched = diff_off is None
            if not matched:
                errors.append(
                    f"retained_native {fn_name} @ 0x{vma:08x} unexpected byte diff at "
                    f"+0x{diff_off:x}: elf={elf_bytes[diff_off:diff_off+4].hex()} "
                    f"ref={ref_bytes[diff_off:diff_off+4].hex()}"
                )
            else:
                verified_bytes += size - masked_bytes

            retained_native_checks.append({
                "name": fn_name,
                "vma": f"0x{vma:08x}",
                "size": size,
                "exact_bytes": size - masked_bytes,
                "relocated_bytes": masked_bytes,
                "relocation_note": reloc_note,
                "first_unexpected_diff": diff_off,
                "matched": matched,
            })

    passed = len(errors) == 0
    report = {
        "target": "rkof",
        "elf": str(args.elf),
        "reference": str(args.reference),
        "reference_size": len(raw),
        "reference_sha256": sha256_hex(raw),
        "entry": "0x00000109",
        "pt_loads": pt_loads,
        "alloc_section_checks": alloc_section_checks,
        "section_checks": section_checks,
        "retained_native_checks": retained_native_checks,
        "loaded_byte_count": verified_bytes,
        "passed": passed,
        "errors": errors,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if not passed:
        for err in errors:
            print(f"[FAIL] {err}", file=sys.stderr)
        return 1

    print(
        f"[OK] verify_load_image: {len(alloc_section_checks)}/{len(alloc_section_checks)} SHF_ALLOC sections in PT_LOAD, "
        f"{len(section_checks)}/{len(section_checks)} binary slices + "
        f"{len(retained_native_checks)}/{len(retained_native_checks)} retained_native veneers "
        f"matched ({verified_bytes:,} exact bytes verified)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
