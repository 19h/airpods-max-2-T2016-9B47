#!/usr/bin/env python3
"""Run the outstanding domain gates without stopping after the first failure."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from arm_semantics import ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    before = hashlib.sha256(args.elf.read_bytes()).hexdigest()
    commands = [
        [sys.executable, str(ROOT / "tools/probe_rom_observability.py"),
         "--elf", str(args.elf), "--output", str(args.build / "rom-observability.json"),
         "--require-equivalent"],
        [sys.executable, str(ROOT / "tools/probe_frs_scan.py"),
         "--elf", str(args.elf), "--output", str(args.build / "frs-selector-zero.json"),
         "--selector-zero"],
        [sys.executable, str(ROOT / "tools/qemu_frs_process.py"),
         "--build", str(args.build), "--selector-zero"],
        [sys.executable, str(ROOT / "tools/audit_rom_domain.py"),
         "--elf", str(args.elf), "--output", str(args.build / "rom-domain.json"),
         "--require-equivalent"],
    ]
    results = [{"command": command, "exit_code": subprocess.run(command, cwd=ROOT).returncode}
               for command in commands]
    unchanged = before == hashlib.sha256(args.elf.read_bytes()).hexdigest()
    passed = unchanged and all(result["exit_code"] == 0 for result in results)
    report = {"scope": __doc__, "elf_sha256": before, "source_elf_unchanged": unchanged,
              "passed": passed, "results": results}
    (args.build / "semantic-domains.json").write_text(json.dumps(report, indent=2) + "\n")
    return int(not passed)


if __name__ == "__main__":
    raise SystemExit(main())
