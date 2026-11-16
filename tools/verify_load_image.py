#!/usr/bin/env python3
"""Verify file-backed allocated ELF sections against the PT_LOAD memory image.

Section contents alone do not prove that an ELF loader maps those bytes.
NOBITS initialization and the firmware's cold/warm boot policy are separate.
"""
import argparse
import hashlib
import json
from pathlib import Path

from elftools.elf.elffile import ELFFile


def loaded_bytes(image):
    memory = {}
    for segment in image.iter_segments():
        if segment["p_type"] != "PT_LOAD":
            continue
        if segment["p_filesz"] > segment["p_memsz"]:
            raise ValueError("PT_LOAD file size exceeds memory size")
        data = segment.data()
        if len(data) != segment["p_filesz"]:
            raise ValueError("truncated PT_LOAD data")
        for offset, value in enumerate(data):
            address = segment["p_vaddr"] + offset
            if address in memory and memory[address] != value:
                raise ValueError(f"conflicting load bytes at {address:#x}")
            memory[address] = value
    return memory


def range_check(memory, address, data, name):
    missing = [address + i for i in range(len(data)) if address + i not in memory]
    different = [address + i for i, value in enumerate(data)
                 if address + i in memory and memory[address + i] != value]
    return {"name": name, "address": hex(address), "byte_count": len(data),
            "missing_bytes": len(missing), "different_bytes": len(different),
            "first_missing": hex(missing[0]) if missing else None,
            "first_different": hex(different[0]) if different else None,
            "match": not missing and not different}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference", type=Path)
    args = parser.parse_args()
    frozen = args.elf.read_bytes()
    with args.elf.open("rb") as stream:
        image = ELFFile(stream)
        memory = loaded_bytes(image)
        checks = [range_check(memory, section["sh_addr"], section.data(), section.name)
                  for section in image.iter_sections()
                  if section["sh_flags"] & 2 and section["sh_size"]
                  and section["sh_type"] != "SHT_NOBITS"]
    unchanged = frozen == args.elf.read_bytes()
    readonly = None
    if args.reference is not None:
        raw = args.reference.read_bytes()
        if hashlib.sha256(raw).hexdigest() != "15f3ce1c30b82e3f87c45974284ccfa68e10928fea0f09a1a46376ac04cd50d4":
            raise ValueError("reference firmware checksum mismatch")
        readonly = range_check(memory, 0x6B4C, raw[0x6B4C:0x88D2], "reference readonly bank")
    passed = unchanged and all(check["match"] for check in checks) and (readonly is None or readonly["match"])
    report = {"scope": __doc__, "elf_sha256": hashlib.sha256(frozen).hexdigest(),
              "source_elf_unchanged": unchanged, "loaded_byte_count": len(memory),
              "section_checks": checks, "passed": passed}
    report["reference_readonly_check"] = readonly
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    failed = [check for check in checks if not check["match"]]
    print(f"ELF load coverage: {len(checks)} sections, {len(failed)} failures, "
          f"reference readonly={None if readonly is None else readonly['match']}")
    for check in failed:
        print(check)
    return int(not passed)


if __name__ == "__main__":
    raise SystemExit(main())
