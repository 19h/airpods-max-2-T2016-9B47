#!/usr/bin/env python3
"""Check the MAC instruction observer against a known Thumb IT load/store oracle."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from elftools.elf.elffile import ELFFile
from unicorn import UC_HOOK_CODE, UcError
from unicorn.arm_const import UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_CPSR
from arm_semantics import Machine
from instruction_observer import InstructionObserver

ADDRESS, DATA = 0x18000, 0x4D160000
ASSEMBLY = '''.syntax unified
.thumb
.text
.global probe
.thumb_func
probe:
 cmp r2, #3
 ittee ls
 ldrbls r3, [r5, #13]
 ldrbls r2, [r5, #14]
 movhi r3, #0
 movhi r2, #0
 b compare
compare:
 cmp r0, #25
 bhi wrong
 movs r6, #7
 b stores
wrong:
 movs r6, #99
stores:
 strb r6, [r5, #20]
 strh r2, [r5, #22]
 str r3, [r5, #24]
 bx lr
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    source = args.build / 'observer-it.S'
    source.write_text(ASSEMBLY)
    obj = args.build / 'observer-it.o'
    subprocess.run(['clang', '-target', 'armv7em-none-eabi', '-mcpu=cortex-m4', '-c', str(source), '-o', str(obj)], check=True)
    with obj.open('rb') as stream:
        code = ELFFile(stream).get_section_by_name('.text').data()
    cases = []
    for left in (16, 32):
        for right in (0, 4):
            runs = {}
            for mode in ('none', 'instruction', 'memory'):
                machine = Machine(False, args.elf)
                if mode != 'memory':
                    for handle in machine.observer_handles:
                        machine.cpu.hook_del(handle)
                if mode == 'instruction':
                    machine.cpu.hook_add(UC_HOOK_CODE, InstructionObserver(machine), begin=0, end=0x1FFFF)
                machine.write(ADDRESS, code)
                machine.write(DATA + 13, bytes((2, 127)))
                machine.cpu.reg_write(UC_ARM_REG_R5, DATA)
                machine.cpu.reg_write(UC_ARM_REG_R6, 0)
                try:
                    machine.call(ADDRESS, (left, 0, right))
                except (UcError, RuntimeError) as error:
                    if mode != 'memory':
                        raise
                    runs[mode] = {'error': str(error)}
                    continue
                runs[mode] = {'registers': [machine.cpu.reg_read(reg) for reg in (UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_R6)],
                              'it_state': machine.cpu.reg_read(UC_ARM_REG_CPSR) & 0x0600FC00,
                              'transactions': machine.transactions, 'memory': machine.read(DATA, 32).hex()}
            r2, r3, r6 = (127 if right <= 3 else 0), (2 if right <= 3 else 0), (99 if left > 25 else 7)
            expected = ([('read', DATA + 13, 1, 2), ('read', DATA + 14, 1, 127)] if right <= 3 else [])
            expected += [('write', DATA + 20, 1, r6), ('write', DATA + 22, 2, r2), ('write', DATA + 24, 4, r3)]
            match = (runs['instruction']['registers'] == [r2, r3, r6]
                     and runs['instruction']['it_state'] == 0
                     and runs['instruction']['transactions'] == expected
                     and runs['instruction']['memory'] == runs['none']['memory']
                     and runs['instruction']['registers'] == runs['none']['registers'])
            cases.append({'inputs': [left, right], 'match': match, 'expected_transactions': expected, 'runs': runs})
    # LDREX is intentionally outside this observer's supported memory subset.
    source.write_text('.syntax unified\n.thumb\n.text\nldrex r0, [r5]\nbx lr\n')
    subprocess.run(['clang', '-target', 'armv7em-none-eabi', '-mcpu=cortex-m4', '-c', str(source), '-o', str(obj)], check=True)
    with obj.open('rb') as stream:
        unsupported = ELFFile(stream).get_section_by_name('.text').data()
    machine = Machine(False, args.elf)
    for handle in machine.observer_handles:
        machine.cpu.hook_del(handle)
    machine.cpu.hook_add(UC_HOOK_CODE, InstructionObserver(machine), begin=0, end=0x1FFFF)
    machine.write(ADDRESS, unsupported)
    machine.cpu.reg_write(UC_ARM_REG_R5, DATA)
    rejected = False
    try:
        machine.call(ADDRESS)
    except RuntimeError as error:
        rejected = 'unsupported peripheral instruction' in str(error)
    report = {'scope': __doc__, 'elf_sha256': hashlib.sha256(args.elf.read_bytes()).hexdigest(),
              'probe_code_sha256': hashlib.sha256(code).hexdigest(), 'probe_assembly': ASSEMBLY,
              'cases': cases, 'unsupported_peripheral_access_rejected': rejected}
    (args.build / 'instruction-observer.json').write_text(json.dumps(report, indent=2) + '\n')
    failures = sum(not case['match'] for case in cases)
    print(f'instruction observer: {len(cases)} oracle cases, {failures} divergences; unsupported access rejected={rejected}')
    return failures != 0 or not rejected


if __name__ == '__main__':
    raise SystemExit(main())
