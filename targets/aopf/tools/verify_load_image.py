#!/usr/bin/env python3
"""Load-image & PT_LOAD segment verifier comparing aopf.elf against raw ../../../aopf."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
from elftools.elf.elffile import ELFFile

BINARY_BACKED_CHECKS = [
    (".vectors",         0x00000000, 0x00000000, 0x000000C0),
    (".rtkit.header",    0x000000C0, 0x000000C0, 0x00000048),
    (".rodata.rom_bank", 0x00064000, 0x00064000, 0x00013B14),
    (".data.ram_bank",   0x20800000, 0x00077B20, 0x0001A0B0),
]


def sha256_hex(buf: bytes) -> str:
    return hashlib.sha256(buf).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--target-dir", type=Path, default=Path("."))
    ap.add_argument("--elf", type=Path, required=True)
    ap.add_argument("--raw", type=Path, required=True)
    ap.add_argument("--report", type=Path, required=True)
    args = ap.parse_args()

    raw_bytes = args.raw.read_bytes()
    section_checks = []
    pt_load_checks = []
    all_passed = True
    exact_matched_bytes = 0
    total_loaded_bytes = 0

    with open(args.elf, "rb") as f:
        elf = ELFFile(f)
        load_segs = []
        for seg in elf.iter_segments():
            if seg["p_type"] == "PT_LOAD":
                total_loaded_bytes += seg["p_filesz"]
                load_segs.append((seg["p_vaddr"], seg["p_paddr"], seg["p_filesz"], seg.data()))

        # 1. Verify every SHF_ALLOC, non-SHT_NOBITS section is covered by and byte-matches PT_LOAD
        for sec in elf.iter_sections():
            if (sec["sh_flags"] & 0x2) == 0 or sec["sh_type"] == "SHT_NOBITS" or sec["sh_size"] == 0:
                continue
            svma = sec["sh_addr"]
            sdata = sec.data()
            slen = len(sdata)
            covered = False
            seg_matched = False
            for pvaddr, _, pfilesz, pdata in load_segs:
                if pvaddr <= svma and svma + slen <= pvaddr + pfilesz:
                    covered = True
                    seg_slice = pdata[svma - pvaddr : svma - pvaddr + slen]
                    seg_matched = seg_slice == sdata
                    break
            ok = covered and seg_matched
            if not ok:
                all_passed = False
            pt_load_checks.append({
                "section": sec.name,
                "vma": hex(svma),
                "size": slen,
                "covered_by_pt_load": covered,
                "pt_load_byte_equal": seg_matched,
                "passed": ok,
            })

        # 2. Verify binary-backed sections against raw ../../../aopf (both section & PT_LOAD slice)
        for sec_name, exp_vma, raw_off, exp_len in BINARY_BACKED_CHECKS:
            sec = elf.get_section_by_name(sec_name)
            if sec is None:
                section_checks.append({
                    "section": sec_name,
                    "passed": False,
                    "error": "missing section",
                })
                all_passed = False
                continue

            elf_slice = sec.data()
            raw_slice = raw_bytes[raw_off : raw_off + exp_len]
            pt_slice = None
            for pvaddr, _, pfilesz, pdata in load_segs:
                if pvaddr <= exp_vma and exp_vma + exp_len <= pvaddr + pfilesz:
                    pt_slice = pdata[exp_vma - pvaddr : exp_vma - pvaddr + exp_len]
                    break

            matched = (
                sec["sh_addr"] == exp_vma
                and len(elf_slice) == exp_len
                and elf_slice == raw_slice
                and pt_slice == raw_slice
            )
            if matched:
                exact_matched_bytes += exp_len
            else:
                all_passed = False

            section_checks.append({
                "section": sec_name,
                "vma": hex(sec["sh_addr"]),
                "raw_fileoff": hex(raw_off),
                "byte_size": exp_len,
                "sha256": sha256_hex(elf_slice),
                "expected_sha256": sha256_hex(raw_slice),
                "pt_load_verified": pt_slice == raw_slice,
                "passed": matched,
            })

    report = {
        "target": "aopf",
        "elf_path": str(args.elf),
        "raw_path": str(args.raw),
        "raw_size": len(raw_bytes),
        "raw_sha256": sha256_hex(raw_bytes),
        "loaded_byte_count": total_loaded_bytes,
        "verified_bytes": exact_matched_bytes,
        "pt_load_alloc_sections_verified": len(pt_load_checks),
        "pt_load_checks": pt_load_checks,
        "section_checks": section_checks,
        "passed": all_passed,
    }

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if not all_passed:
        print(f"[verify_load_image] FAIL -> {args.report}", file=sys.stderr)
        return 1

    print(
        f"[aopf verify_load_image] PASS: 4/4 binary-backed sections ({exact_matched_bytes} bytes) "
        f"& {len(pt_load_checks)}/{len(pt_load_checks)} SHF_ALLOC PT_LOAD slices verified; "
        f"total ELF loaded bytes = {total_loaded_bytes}."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
