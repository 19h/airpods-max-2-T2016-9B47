#!/usr/bin/env python3
"""Check native FRS instruction bytes, call targets, table and literal geometry.

This is a local instruction-layout check under explicit call relocation, not a
whole-image equivalence proof. Absolute executable addresses still differ.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import capstone
from capstone import arm
from elftools.elf.elffile import ELFFile

from arm_semantics import ROOT, REFERENCE_SHA256
from verify_load_image import loaded_bytes, range_check


CALLS = {0x498: "systick_delay", 0x474: "systick_now", 0x5D80: "memset",
         0x41C: "dbg_printf", 0x1BA8: "frs_elapsed_systick_arm",
         0x1BB0: "frs_clear_agc_arm", 0x1BBE: "frs_clear_trigger_arm"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mutation-controls", action="store_true")
    args = parser.parse_args()
    raw = (ROOT.parent / "phyb").read_bytes()
    if hashlib.sha256(raw).hexdigest() != REFERENCE_SHA256:
        raise ValueError("reference firmware checksum mismatch")
    frozen = args.elf.read_bytes()
    decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB | capstone.CS_MODE_MCLASS)
    decoder.detail = True
    with args.elf.open("rb") as stream:
        image = ELFFile(stream)
        symbols = {symbol.name: symbol for symbol in image.get_section_by_name(".symtab").iter_symbols()}
        memory = loaded_bytes(image)
        load_checks = []

        def body(name):
            symbol = symbols[name]
            section = image.get_section(symbol["st_shndx"])
            address = symbol["st_value"] & ~1
            offset = address - section["sh_addr"]
            data = bytearray(section.data()[offset:offset + symbol["st_size"]])
            load_checks.append(range_check(memory, address, data, name))
            return address, data

        scan_address, actual = body("frs_scan_pass")
        scan_symbol = symbols["frs_scan_pass"]
        scan_section = image.get_section(scan_symbol["st_shndx"])
        scan_file_offset = scan_section["sh_offset"] + scan_address - scan_section["sh_addr"]
        expected = bytearray(raw[0xD7C:0x1064])
        if len(actual) != len(expected) or scan_address != 0xD7C:
            raise ValueError("scan size/alignment differs from reference")
        call_checks = []
        for start, end in ((0xD7C, 0xE92), (0xE96, 0x1024)):
            instructions = list(decoder.disasm(raw[start:end], start))
            if sum(instruction.size for instruction in instructions) != end - start:
                raise ValueError("reference decoding did not cover its instruction range")
            for instruction in instructions:
                if instruction.id != arm.ARM_INS_BL:
                    continue
                target = instruction.operands[0].imm
                name = CALLS[target]
                offset = instruction.address - 0xD7C
                rebuilt = next(decoder.disasm(bytes(actual[offset:offset + 4]), scan_address + offset))
                match = (rebuilt.id == arm.ARM_INS_BL and rebuilt.size == 4 and
                         rebuilt.operands[0].imm == (symbols[name]["st_value"] & ~1))
                call_checks.append({"reference_call": hex(instruction.address), "target": name, "match": match})
                actual[offset:offset + 4] = expected[offset:offset + 4] = bytes(4)
        scan_match = actual == expected and all(check["match"] for check in call_checks)
        helpers = []
        for name, start, end in (("frs_clear_agc_arm", 0x1BB0, 0x1BBE),
                                 ("frs_clear_trigger_arm", 0x1BBE, 0x1BCC)):
            address, data = body(name)
            helpers.append({"name": name, "byte_count": len(data), "match": data == raw[start:end]})
        address, elapsed = body("frs_elapsed_systick_arm")
        expected_elapsed = bytearray(raw[0x1BA8:0x1BB0] + raw[0x480:0x498])
        branch = next(decoder.disasm(bytes(elapsed[4:8]), address + 4))
        branch_match = (branch.id == arm.ARM_INS_B and branch.size == 4 and
                        branch.operands[0].imm == address + 8)
        elapsed[4:8] = expected_elapsed[4:8] = bytes(4)
        helpers.append({"name": "frs_elapsed_systick_arm", "byte_count": len(elapsed),
                        "match": elapsed == expected_elapsed and branch_match})
        messages = image.get_section_by_name(".frs.scan_log")
        messages_match = (messages is not None and messages["sh_addr"] == 0x7E48
                          and messages.data() == raw[0x7E48:0x7E73])
        if messages is not None:
            load_checks.append(range_check(memory, messages["sh_addr"], messages.data(), "FRS messages"))
        debug_address, debug_data = body("dbg_printf")
        debug_match = debug_address == 0x41C and debug_data == raw[0x41C:0x420]
    unchanged = frozen == args.elf.read_bytes()
    passed = (scan_match and all(helper["match"] for helper in helpers) and messages_match
              and debug_match and all(check["match"] for check in load_checks) and unchanged)
    report = {"scope": __doc__, "reference_sha256": REFERENCE_SHA256,
              "elf_sha256": hashlib.sha256(frozen).hexdigest(), "source_elf_unchanged": unchanged,
              "scan_byte_count": len(expected), "scan_match_with_verified_calls": scan_match,
              "scan_address": hex(scan_address),
              "messages_byte_count": 43, "messages_match": messages_match,
              "load_checks": load_checks,
              "debug_stub_match": debug_match, "debug_stub_address": hex(debug_address),
              "call_checks": call_checks, "helpers": helpers, "passed": passed}
    if args.mutation_controls and passed:
        controls = []
        for name, rva in (("changed-initial-stack-slot", 0xECC),
                          ("changed-call-target", 0xDE4), ("changed-selector-zero-jump", 0xE92)):
            mutant = bytearray(frozen)
            offset = scan_file_offset + rva - 0xD7C
            before = mutant[offset]
            mutant[offset] ^= 1
            path = args.output.parent / ("frs-native-" + name + ".elf")
            output = args.output.parent / ("frs-native-" + name + ".json")
            path.write_bytes(mutant)
            output.unlink(missing_ok=True)
            run = subprocess.run([sys.executable, str(Path(__file__).resolve()), "--elf", str(path),
                                  "--output", str(output)], capture_output=True, text=True)
            result = json.loads(output.read_text()) if output.exists() else None
            detected = (run.returncode == 1 and result is not None and not result["passed"]
                        and result["source_elf_unchanged"]
                        and result["elf_sha256"] == hashlib.sha256(mutant).hexdigest())
            controls.append({"name": name, "file_offset": offset, "before": before,
                             "after": mutant[offset], "detected": detected, "checker_report": result})
        report["mutation_controls"] = controls
        passed = passed and all(control["detected"] for control in controls)
        report["passed"] = passed
    report["source_elf_unchanged"] = frozen == args.elf.read_bytes()
    passed = passed and report["source_elf_unchanged"]
    report["passed"] = passed
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"FRS native layout: scan={scan_match}, {len(call_checks)} calls checked, "
          f"helpers={all(helper['match'] for helper in helpers)}, "
          f"loads={all(check['match'] for check in load_checks)}, "
          f"controls={all(control['detected'] for control in report.get('mutation_controls', []))}, passed={passed}")
    return int(not passed)


if __name__ == "__main__":
    raise SystemExit(main())
