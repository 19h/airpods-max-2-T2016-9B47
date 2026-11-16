#!/usr/bin/env python3
"""Byte-exact load image & PT_LOAD segment verification of acib.elf against raw ftab acib."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

from elftools.elf.elffile import ELFFile

EXPECTED_RAW_SHA256 = "dda0b224409bb5997dd95374312f13a207591e857edf9f2cff234ae86b66795e"
SHF_ALLOC = 0x2

LOAD_CHECKS = [
    (".vectors",           0x00000000, 0x00000000, 0x000000C0),
    (".rtkit.header",      0x000000C0, 0x000000C0, 0x00000048),
    (".vectors.secondary", 0x00000400, 0x00000400, 0x00000240),
    (".text.nmi",          0x000E4A94, 0x000E4A94, 0x00000080),
    (".rom.rodata",        0x000EFEA8, 0x000EFEA8, 0x000099A4),
    (".ram.data",          0x10000000, 0x000F9860, 0x0000AC1E),
]


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract_from_pt_load(pt_loads: list[tuple[int, int, bytes]], vma: int, size: int) -> bytes | None:
    for seg_vaddr, seg_filesz, seg_bytes in pt_loads:
        if seg_vaddr <= vma and (vma + size) <= (seg_vaddr + seg_filesz):
            off = vma - seg_vaddr
            return seg_bytes[off : off + size]
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args()

    raw = args.raw.read_bytes()
    raw_sha = sha256_hex(raw)
    sha_ok = raw_sha == EXPECTED_RAW_SHA256

    section_checks = []
    pt_load_alloc_checks = []
    all_ok = sha_ok
    verified_bytes = 0

    with args.elf.open("rb") as fp:
        elf = ELFFile(fp)
        pt_loads: list[tuple[int, int, bytes]] = []
        for seg in elf.iter_segments():
            if seg["p_type"] == "PT_LOAD" and seg["p_filesz"] > 0:
                pt_loads.append((seg["p_vaddr"], seg["p_filesz"], seg.data()))

        # 1. Verify every allocated file-backed (SHF_ALLOC, non-SHT_NOBITS) section
        #    is covered by and byte-identical to its enclosing PT_LOAD segment.
        for sec in elf.iter_sections():
            flags = int(sec["sh_flags"])
            size = int(sec["sh_size"])
            if (flags & SHF_ALLOC) == 0 or sec["sh_type"] == "SHT_NOBITS" or size == 0:
                continue
            vma = int(sec["sh_addr"])
            sec_blob = sec.data()
            seg_blob = extract_from_pt_load(pt_loads, vma, size)
            covered = seg_blob is not None
            bytes_match = covered and (seg_blob == sec_blob)
            if not bytes_match:
                all_ok = False
            pt_load_alloc_checks.append({
                "section": sec.name,
                "vma": hex(vma),
                "byte_size": size,
                "pt_load_covered": covered,
                "pt_load_byte_match": bytes_match,
            })

        # 2. Verify binary-backed sections against raw ftab acib via BOTH section data and PT_LOAD.
        for sec_name, exp_vma, raw_off, length in LOAD_CHECKS:
            sec = elf.get_section_by_name(sec_name)
            if sec is None:
                all_ok = False
                section_checks.append({
                    "section": sec_name,
                    "present": False,
                    "match": False,
                })
                continue

            elf_blob = sec.data()
            seg_blob = extract_from_pt_load(pt_loads, exp_vma, length)
            raw_blob = raw[raw_off : raw_off + length]
            vma_ok = sec["sh_addr"] == exp_vma
            sec_ok = elf_blob == raw_blob
            pt_load_ok = seg_blob is not None and seg_blob == raw_blob
            ok = vma_ok and sec_ok and pt_load_ok
            if not ok:
                all_ok = False
            else:
                verified_bytes += length

            section_checks.append({
                "section": sec_name,
                "vma": hex(sec["sh_addr"]),
                "raw_file_offset": hex(raw_off),
                "byte_size": length,
                "elf_sha256": sha256_hex(elf_blob),
                "pt_load_sha256": sha256_hex(seg_blob) if seg_blob is not None else None,
                "raw_sha256": sha256_hex(raw_blob),
                "pt_load_match": pt_load_ok,
                "match": ok,
            })

    report = {
        "target": "acib",
        "raw_sha256": raw_sha,
        "raw_sha256_ok": sha_ok,
        "pt_load_segment_count": len(pt_loads),
        "pt_load_alloc_sections_verified": len(pt_load_alloc_checks),
        "pt_load_alloc_checks": pt_load_alloc_checks,
        "section_checks": section_checks,
        "loaded_byte_count": verified_bytes,
        "passed": all_ok,
    }

    out_path = args.output or (args.elf.parent / "load-coverage.json")
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if not all_ok:
        print(f"verify_load_image: FAIL (see {out_path})", file=sys.stderr)
        return 1

    print(
        f"verify_load_image: PASS ({len(section_checks)} binary-backed sections = {verified_bytes} bytes "
        f"byte-exact vs raw acib; {len(pt_load_alloc_checks)}/{len(pt_load_alloc_checks)} SHF_ALLOC sections match PT_LOAD)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
