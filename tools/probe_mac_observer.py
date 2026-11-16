#!/usr/bin/env python3
"""Reproduce the MAC IT discrepancy between Unicorn logging observers.

The logging-disabled comparison retains the isolated ACK response hook. The
instruction observer uses no memory hooks. Only the specified IQMM request is
compared here; this is not a complete validation of Unicorn or Capstone.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from arm_semantics import check_mac


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    runs = {}
    for mode in ('memory', 'instruction', 'logging-disabled'):
        runs[mode] = check_mac(args.elf, 1, 0x15, struct.pack('<BBH', 17, 2, 0x817F),
                               4, 8, 0, mailbox_base=0x4D160000,
                               observe=mode != 'logging-disabled', observer_kind=mode)
    matches = runs['instruction']['match'] and runs['logging-disabled']['match']
    for side in ('reference', 'rebuilt'):
        for key in ('state', 'mailbox'):
            matches &= runs['instruction'][side].get(key) == runs['logging-disabled'][side].get(key)
    reproduced = not runs['memory']['match']
    report = {'scope': __doc__, 'elf_sha256': hashlib.sha256(args.elf.read_bytes()).hexdigest(),
              'instruction_matches_logging_disabled': matches,
              'memory_hook_discrepancy_reproduced': reproduced, 'runs': runs}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'MAC observer: instruction/logging-disabled agree={matches}; memory-hook discrepancy reproduced={reproduced}')
    return not matches


if __name__ == '__main__':
    raise SystemExit(main())
