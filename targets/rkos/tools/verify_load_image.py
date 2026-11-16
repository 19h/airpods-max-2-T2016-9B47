#!/usr/bin/env python3
"""Load-image byte-identity & PT_LOAD segment verifier for `targets/rkos/`."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

from elftools.elf.elffile import ELFFile

EXPECTED_RAW_SHA256 = "adf98c119724a5c6bec397f70fd08d65b3108c261cf0e6ad4b81ede766f18ecf"
EXPECTED_RAW_SIZE = 134176


def sha256_hex(blob: bytes) -> str:
    return hashlib.sha256(blob).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True, help="Path to rkos.elf")
    parser.add_argument("--reference", type=Path, required=True, help="Path to raw ../../rkos")
    parser.add_argument("--output", type=Path, required=True, help="Path to load-coverage.json")
    args = parser.parse_args()

    raw = args.reference.read_bytes()
    raw_sha = sha256_hex(raw)
    errors: list[str] = []

    if len(raw) != EXPECTED_RAW_SIZE or raw_sha != EXPECTED_RAW_SHA256:
        errors.append(f"reference binary mismatch: size={len(raw)} sha256={raw_sha}")

    section_checks: list[dict[str, object]] = []
    pt_load_segments: list[dict[str, object]] = []
    loaded_bytes = 0

    with args.elf.open("rb") as fp:
        elf = ELFFile(fp)
        for seg in elf.iter_segments():
            if seg["p_type"] != "PT_LOAD":
                continue
            p_vaddr = int(seg["p_vaddr"])
            p_filesz = int(seg["p_filesz"])
            p_memsz = int(seg["p_memsz"])
            loaded_bytes += p_filesz
            pt_load_segments.append({
                "vaddr": hex(p_vaddr),
                "filesz": hex(p_filesz),
                "memsz": hex(p_memsz),
            })

        def check_region(label: str, sec_name: str, sec_off: int, length: int, raw_off: int) -> None:
            sec = elf.get_section_by_name(sec_name)
            if sec is None:
                errors.append(f"missing ELF section {sec_name}")
                return
            sdata = sec.data()
            elf_slice = sdata[sec_off : sec_off + length]
            ref_slice = raw[raw_off : raw_off + length]
            matched = elf_slice == ref_slice and len(elf_slice) == length
            if not matched:
                errors.append(f"byte mismatch in {label} ({sec_name}+{hex(sec_off)})")
            section_checks.append({
                "name": label,
                "elf_section": sec_name,
                "vma": hex(int(sec["sh_addr"]) + sec_off),
                "byte_length": length,
                "raw_file_offset": hex(raw_off),
                "sha256": sha256_hex(elf_slice),
                "match": matched,
            })

        # 1. Boot vector table (0x00000000..0x000000C0)
        check_region("ROM_VECTORS", ".vectors", 0x0, 0xC0, 0x0)
        # 2. RTKit 'uuid' image header (0x000000C0..0x00000108)
        check_region("RTKIT_UUID_HDR", ".text", 0x0, 0x48, 0xC0)
        # 3. Entire ROM_RODATA (0x00013090..0x00017590: VTOR + strings + K512 + CRC32 + TLVs + init_array)
        check_region("ROM_RODATA", ".rodata", 0x0, 0x4500, 0x13090)
        # 4. Entire RAM_DATA (0x20000000..0x20000BA4: initialized globals + SRAM trampoline @ 0x20000988)
        check_region("RAM_DATA", ".data", 0x0, 0xBA4, 0x20000)

    all_matched = len(errors) == 0 and all(bool(c["match"]) for c in section_checks)
    exact_bytes = sum(int(c["byte_length"]) for c in section_checks if c["match"])

    report = {
        "target": "rkos",
        "elf": str(args.elf),
        "reference": str(args.reference),
        "reference_sha256": raw_sha,
        "pt_load_segments": pt_load_segments,
        "loaded_byte_count": loaded_bytes,
        "verified_exact_bytes": exact_bytes,
        "section_checks": section_checks,
        "reference_readonly_check": {
            "match": all_matched,
            "verified_bytes": exact_bytes,
        },
        "errors": errors,
        "passed": all_matched,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if not all_matched:
        for err in errors:
            print(f"[verify_load_image] FAIL: {err}", file=sys.stderr)
        return 1

    print(
        f"[verify_load_image] PASS: {len(section_checks)} binary-backed regions "
        f"({exact_bytes} bytes: .vectors + uuid_hdr + .rodata + .data) match ../../rkos 100% byte-for-byte"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
