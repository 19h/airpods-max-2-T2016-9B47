#!/usr/bin/env python3
"""Check executed diagnostic descriptor/header access widths and order.

Instruction observation supplies addresses/widths without memory hooks. This
fixed, disjoint ordinary-RAM case checks the order independently of whether
aliases happen to alter the final bytes. It is not an all-context proof.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from unicorn import UC_HOOK_CODE
from arm_semantics import Machine, REFERENCE_SHA256, TEST
from instruction_observer import InstructionObserver


def check(elf, reference):
    descriptor, output = 0x4D28A2D0, TEST + 0x4000
    expected = [('read', descriptor + 12, 4, 0x11223344),
                ('read', descriptor + 8, 2, 0),
                ('write', output + 1, 2, 0x1234),
                ('write', output, 1, 0),
                ('write', output + 6, 4, 0x11223344),
                ('write', output + 3, 2, 0),
                ('write', output + 5, 1, 0),
                ('read', descriptor + 4, 4, TEST + 0x1000)]
    machine = Machine(reference, elf)
    for handle in machine.observer_handles:
        machine.cpu.hook_del(handle)
    events = []

    class Capture(InstructionObserver):
        def access(self, cpu, address, width, write=False, value=0):
            if descriptor <= address < descriptor + 16 or output <= address < output + 16:
                if not write:
                    value = int.from_bytes(cpu.mem_read(address, width), 'little')
                events.append(('write' if write else 'read', address, width,
                               value & ((1 << (8 * width)) - 1)))
            super().access(cpu, address, width, write, value)

    machine.cpu.hook_add(UC_HOOK_CODE, Capture(machine))
    machine.write(descriptor, struct.pack('<B3xIHHI', 1, TEST + 0x1000, 0, 0, 0x11223344))
    result = machine.call(0x4DBC if reference else 'diag_buffer_read',
                          [output, TEST + 0x3000, 0, 0x1234, 0, 0xA6])
    copied = int.from_bytes(machine.read(TEST + 0x3000, 4), 'little')
    return {'expected': expected, 'events': events, 'return': result, 'copied': copied,
            'match': events == expected and result == 0 and copied == 0xA6}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    original = args.elf.read_bytes()
    left, right = check(args.elf, True), check(args.elf, False)
    unchanged = original == args.elf.read_bytes()
    report = {'scope': __doc__, 'reference_sha256': REFERENCE_SHA256,
              'elf_sha256': hashlib.sha256(original).hexdigest(),
              'reference': left, 'rebuilt': right, 'passed': left['match'] and right['match'] and unchanged,
              'source_elf_unchanged': unchanged}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Diagnostic sampling: reference={left['match']}, rebuilt={right['match']}")
    return not report['passed']


if __name__ == '__main__':
    raise SystemExit(main())
