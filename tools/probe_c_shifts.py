#!/usr/bin/env python3
"""Compare recovered C 64-bit shifts with the original ARM helpers.

The matrix covers all low-byte amounts with four upper-bit patterns and eight
64-bit values. Checks observe the numerical result, AAPCS callee-saved registers,
and SP with all memory hooks removed. Caller-clobbered registers, flags, timing
and arbitrary entry/stack aliases are outside this finite comparison.
"""
import argparse
import hashlib
import json
from pathlib import Path

from unicorn import arm_const as arm
from arm_semantics import Machine, REFERENCE_SHA256, STACK


VALUES = (0, 1, 0xFFFFFFFF, 0x100000000, 1 << 63, (1 << 64) - 1,
          0x0123456789ABCDEF, 0xFEDCBA9876543210)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    original = args.elf.read_bytes()
    counts = list(range(512)) + [0x80000000 | n for n in range(256)] + [0xFFFFFF00 | n for n in range(256)]
    saved_registers = [getattr(arm, f'UC_ARM_REG_R{i}') for i in range(4, 12)]
    saved_values = [0xA5A50000 + i * 0x123 for i in range(8)]
    results = []
    for name, address, left in (('__aeabi_llsl', 0x5F54, True), ('__aeabi_llsr', 0x5F76, False)):
        machines = [Machine(reference, args.elf) for reference in (True, False)]
        for machine in machines:
            for handle in machine.observer_handles:
                machine.cpu.hook_del(handle)
        for value in VALUES:
            for count in counts:
                amount = count & 0xFF
                expected = 0 if amount >= 64 else ((value << amount) & ((1 << 64) - 1)) if left else value >> amount
                observations = []
                for index, machine in enumerate(machines):
                    for register, saved in zip(saved_registers, saved_values):
                        machine.cpu.reg_write(register, saved)
                    low = machine.call(address if index == 0 else name,
                                       [value & 0xFFFFFFFF, value >> 32, count])
                    result = low | (machine.cpu.reg_read(arm.UC_ARM_REG_R1) << 32)
                    preserved = all(machine.cpu.reg_read(register) == saved
                                    for register, saved in zip(saved_registers, saved_values))
                    observations.append({'result': hex(result), 'saved_registers_preserved': preserved,
                                         'SP_preserved': machine.cpu.reg_read(arm.UC_ARM_REG_SP) == STACK})
                results.append({'name': name, 'value': hex(value), 'shift_bits': hex(count),
                                'effective_amount': amount, 'expected': hex(expected),
                                'reference': observations[0], 'rebuilt': observations[1],
                                'match': all(o['result'] == hex(expected) and o['saved_registers_preserved']
                                             and o['SP_preserved'] for o in observations)})
    unchanged = original == args.elf.read_bytes()
    report = {'scope': __doc__, 'reference_sha256': REFERENCE_SHA256,
              'elf_sha256': hashlib.sha256(original).hexdigest(),
              'case_count': len(results), 'divergences': sum(not c['match'] for c in results),
              'complete': True, 'source_elf_unchanged': unchanged, 'cases': results}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"C shifts: {len(results)} cases, {report['divergences']} divergences")
    return report['divergences'] != 0 or not unchanged


if __name__ == '__main__':
    raise SystemExit(main())
