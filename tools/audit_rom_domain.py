#!/usr/bin/env python3
"""Cover readable ROM bytes through accepted non-null DPD actuation pointers.

This extends the finite scalar model; ROM readability, supplied GTB=10, absent
interrupts and autonomous device writes remain assumptions. It does not prove
physical reachability or whole-program equivalence.
"""
import argparse
import hashlib
import json
from pathlib import Path

from elftools.elf.elffile import ELFFile

from arm_semantics import Machine, REFERENCE_SHA256, check_tx, elf_bytes
from verify_load_image import loaded_bytes

ROM_END = 0x88D2
COPY_BYTES = 0x3C0


def runs(addresses):
    result = []
    for address in addresses:
        if result and result[-1][1] == address:
            result[-1][1] += 1
        else:
            result.append([address, address + 1])
    return [[hex(start), hex(end)] for start, end in result]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-equivalent", action="store_true")
    args = parser.parse_args()
    reference = Machine(True, args.elf).read(0, ROM_END)
    rebuilt = Machine(False, args.elf).read(0, ROM_END)
    with args.elf.open("rb") as stream:
        image = ELFFile(stream)
        loaded = loaded_bytes(image)
    pointers = list(range(1, ROM_END - COPY_BYTES + 1, COPY_BYTES))
    if pointers[-1] != ROM_END - COPY_BYTES:
        pointers.append(ROM_END - COPY_BYTES)
    covered = bytearray(ROM_END)
    rows = []
    fields = ("return", "state", "test_memory", "hardware_buffer", "trace",
              "telemetry", "result_status", "result_response")
    for pointer in pointers:
        modes = []
        for observe in (True, False):
            case = check_tx(args.elf, dpd_profile=1, dpd_actuation_pointer=pointer,
                            dpd_observe=observe)
            row = {"pointer": hex(pointer), "observer": "instruction" if observe else "none",
                   "match": case["match"], "outputs": {}}
            for side, rom in (("reference", reference), ("rebuilt", rebuilt)):
                if "error" in case[side]:
                    raise RuntimeError(f"{pointer:#x} {side}: {case[side]['error']}")
                output = bytes.fromhex(case[side]["hardware_buffer"])[0x800:0xBC0]
                if output != rom[pointer:pointer + COPY_BYTES]:
                    raise RuntimeError("copy did not expose the expected ROM source bytes")
                row["outputs"][side] = output.hex()
            expected_differences = sum(a != b for a, b in zip(
                reference[pointer:pointer + COPY_BYTES], rebuilt[pointer:pointer + COPY_BYTES]))
            actual_differences = sum(a != b for a, b in zip(
                bytes.fromhex(row["outputs"]["reference"]), bytes.fromhex(row["outputs"]["rebuilt"])))
            if expected_differences != actual_differences:
                raise RuntimeError("static/runtime byte differences disagree")
            row["differing_byte_count"] = actual_differences
            rows.append(row)
            modes.append({side: {key: case[side][key] for key in fields}
                          for side in ("reference", "rebuilt")})
        if modes[0] != modes[1]:
            raise RuntimeError("observer changed a final state")
        covered[pointer:pointer + COPY_BYTES] = bytes([1]) * COPY_BYTES
    if any(not value for value in covered[1:]):
        raise RuntimeError("ROM byte coverage has a gap")
    if args.elf.read_bytes() != elf_bytes(args.elf):
        raise RuntimeError("ELF changed during comparisons")
    different = [address for address, (a, b) in enumerate(zip(reference, rebuilt)) if a != b]
    report = {"scope": __doc__, "reference_sha256": REFERENCE_SHA256,
              "elf_sha256": hashlib.sha256(elf_bytes(args.elf)).hexdigest(),
              "rom_byte_count": ROM_END, "copy_byte_count": COPY_BYTES,
              "pointer_count": len(pointers), "comparison_count": len(rows),
              "divergences": sum(not row["match"] for row in rows),
              "covered_byte_count": sum(covered), "uncovered_byte_runs": runs(
                  address for address, value in enumerate(covered) if not value),
              "uncovered_byte_zero_matches": reference[0] == rebuilt[0],
              "static_differing_byte_count": len(different),
              "readonly_bank_differing_bytes": sum(a != b for a, b in zip(
                  reference[0x6B4C:ROM_END], rebuilt[0x6B4C:ROM_END])),
              "static_difference_runs": runs(different),
              "rebuilt_unbacked_rom_bytes": sum(address not in loaded for address in range(ROM_END)),
              "reference_rom_sha256": hashlib.sha256(reference).hexdigest(),
              "rebuilt_rom_sha256": hashlib.sha256(rebuilt).hexdigest(),
              "audit_consistent": True, "physical_mapping_verified": False,
              "cases": rows}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"ROM domain: {len(pointers)} pointers, {sum(covered)}/{ROM_END} bytes observed, "
          f"{len(different)} static byte differences, {report['divergences']}/{len(rows)} divergent comparisons")
    return int(args.require_equivalent and bool(different or report["divergences"]))


if __name__ == "__main__":
    raise SystemExit(main())
