#!/usr/bin/env python3
"""Expose fixed-address ROM data through accepted NVV writes and TX actuation.

Default success means the probe and its RAM controls executed consistently,
not firmware equivalence. --require-equivalent additionally rejects every
observed reference/reconstruction divergence.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from arm_semantics import Machine, REFERENCE_SHA256, TEST, check_tx, elf_bytes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-equivalent", action="store_true")
    args = parser.parse_args()
    rows = []
    final_fields = ("return", "state", "test_memory", "hardware_buffer", "trace",
                    "telemetry", "result_status", "result_response")
    for pointer in (0x108, 0x10C, 0x200, TEST + 0x5000):
        modes = []
        for observe in (True, False):
            case = check_tx(args.elf, dpd_profile=1,
                            dpd_actuation_pointer=pointer, dpd_observe=observe)
            row = {"pointer": hex(pointer),
                   "observer": "instruction" if observe else "none",
                   "nvv_blob_hex": (struct.pack("<HHI", 12, 0x4E56, 0) +
                                    struct.pack("<HHII", 0xC80B, 8, 0x4D28AA94, pointer)).hex(),
                   "match": case["match"], "executions": {}}
            for reference, side in ((True, "reference"), (False, "rebuilt")):
                result = case[side]
                if "error" in result:
                    raise RuntimeError(f"{pointer:#x} {side}: {result['error']}")
                output = bytes.fromhex(result["hardware_buffer"])[0x800:0xBC0]
                expected = (bytes((i * 17 + 3) & 255 for i in range(0x3C0))
                            if pointer == TEST + 0x5000 else
                            Machine(reference, args.elf).read(pointer, 0x3C0))
                if output != expected:
                    raise RuntimeError("actuation did not expose the declared source bytes")
                writes = [t for t in result["transactions"]
                          if t[0] == "write" and 0x4D148800 <= t[1] < 0x4D148BC0]
                if observe and [(t[1], t[2]) for t in writes] != [
                        (0x4D148800 + i * 4, 4) for i in range(240)]:
                    raise RuntimeError("unexpected actuation write geometry")
                if not observe and result["transactions"]:
                    raise RuntimeError("unobserved execution unexpectedly recorded transactions")
                row["executions"][side] = {
                    "source_hex": expected.hex(), "output_hex": output.hex(),
                    "output_sha256": hashlib.sha256(output).hexdigest(),
                    "observed_actuation_write_count": len(writes),
                    "source_matches_output": True}
            a = bytes.fromhex(row["executions"]["reference"]["output_hex"])
            b = bytes.fromhex(row["executions"]["rebuilt"]["output_hex"])
            row["differing_byte_count"] = sum(x != y for x, y in zip(a, b))
            row["first_differing_address"] = next(
                (hex(0x4D148800 + i) for i, (x, y) in enumerate(zip(a, b)) if x != y), None)
            if pointer == TEST + 0x5000 and not case["match"]:
                raise RuntimeError("ordinary RAM positive control diverged")
            modes.append({side: {key: case[side][key] for key in final_fields}
                          for side in ("reference", "rebuilt")})
            rows.append(row)
        if modes[0] != modes[1]:
            raise RuntimeError("instruction observation changed a final state")
    if hashlib.sha256(args.elf.read_bytes()).digest() != hashlib.sha256(elf_bytes(args.elf)).digest():
        raise RuntimeError("ELF changed during comparison")
    differences = sum(not row["match"] for row in rows)
    report = {"scope": __doc__, "reference_sha256": REFERENCE_SHA256,
              "elf_sha256": hashlib.sha256(elf_bytes(args.elf)).hexdigest(),
              "comparison_count": len(rows), "divergence_count": differences,
              "probe_consistent": True, "equivalent_on_tested_inputs": differences == 0,
              "hook_free_states_agree_with_observed_states": True,
              "assumptions": ["scalar ARMv7E-M Unicorn 2.1.4 model; no interrupt interleaving",
                              "GTB returns 10 and the hardware aperture has no autonomous writes",
                              "ROM is readable at its loaded addresses; physical mapping is unknown"],
              "cases": rows}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"ROM observability: {differences}/{len(rows)} comparisons diverge; "
          "RAM controls and observer-independent states verified")
    return int(args.require_equivalent and differences != 0)


if __name__ == "__main__":
    raise SystemExit(main())
