#!/usr/bin/env python3
"""Conditional complete-domain diagnostic alias checks for zero-copy requests.

Each check fixes the output/copy-count pointers, descriptor index, requested=0
and capacity=0xA6. Offset covers all 32-bit values; descriptor/RAM bytes and
initial NZCV are symbolic under Proof's declared model. Neither arbitrary
pointer geometry nor nonzero copies, stack aliases or hardware are proved.
"""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile
import z3

from symbolic_semantics import Proof, REFERENCE_SHA256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--before-elf', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    proof = Proof(args.elf)
    results = []
    for index in range(3):
        for delta in (-4, 0, 3, 6, 7, 8, 10, 11, 12):
            fixed = {0: 0x4D28A2D0 + 16 * index + delta, 1: 0x4D28A500, 2: index}
            result = proof.check('diag_buffer_read', 0x4DBC, (32, 32, 32, 32), True,
                                 fixed_arguments=fixed, fixed_stack_arguments={0: 0, 1: 0xA6})
            result['index'] = index
            result['destination_delta'] = delta
            results.append(result)
            print(json.dumps(result), flush=True)
    fixed = {0: 0x4D28A2D0 + 7, 1: 0x4D28A500, 2: 0}
    old = Proof(args.before_elf)
    old_result = old.check('diag_buffer_read', 0x4DBC, (32, 32, 32, 32), True,
                           fixed_arguments=fixed, fixed_stack_arguments={0: 0, 1: 0xA6})
    if old_result['status'] != 'counterexample':
        raise RuntimeError(f'old alias bug was not detected: {old_result}')
    mutant = Proof(args.elf)
    symbol = mutant.rebuilt.loader.find_symbol('diag_buffer_read')
    instructions = mutant.rebuilt.arch.capstone_thumb.disasm(
        mutant.rebuilt.loader.memory.load(symbol.rebased_addr & ~1, symbol.size),
        symbol.rebased_addr & ~1)
    instruction = next(i for i in instructions if i.mnemonic == 'movs' and i.op_str == 'r0, #0xa6')
    if mutant.rebuilt.loader.memory.load(instruction.address, 2) != b'\xa6\x20':
        raise RuntimeError('unexpected copied-count instruction')
    mutant.rebuilt.loader.memory.store(instruction.address, b'\xa7\x20')
    mutation = mutant.check('diag_buffer_read', 0x4DBC, (32, 32, 32, 32), True,
                            fixed_arguments=fixed, fixed_stack_arguments={0: 0, 1: 0xA6})
    if mutation['status'] != 'counterexample':
        raise RuntimeError(f'changed copied count was not detected: {mutation}')
    directory = args.output.parent / 'diag-symbolic-smt'
    directory.mkdir(exist_ok=True)
    queries = []
    with tarfile.open(args.output.parent / 'diag-symbolic-smt.tar.gz', 'w:gz') as archive:
        for owner, model in (('positive', proof), ('old-alias-bug', old), ('changed-count', mutant)):
            for label, satisfiable, text in model.queries:
                path = directory / f'{len(queries):04d}-{owner}.smt2'
                path.write_text(text)
                solver = z3.Solver()
                solver.set(timeout=10000)
                solver.from_string(text)
                expected = 'sat' if satisfiable else 'unsat'
                actual = str(solver.check())
                if actual != expected:
                    raise RuntimeError(f'SMT replay mismatch: {path}')
                queries.append({'label': label, 'path': str(path), 'result': expected,
                                'z3_python_replay': actual,
                                'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
                archive.add(path, arcname=path.name)
    unchanged = proof.elf == args.elf.read_bytes() and old.elf == args.before_elf.read_bytes()
    passed = unchanged and len(results) == 27 and all(r['status'] == 'unsat for modeled difference'
                                                    for r in results)
    report = {'scope': __doc__, 'reference_sha256': REFERENCE_SHA256,
              'elf_sha256': hashlib.sha256(proof.elf).hexdigest(),
              'before_elf_sha256': hashlib.sha256(old.elf).hexdigest(),
              'check_count': len(results), 'results': results, 'smt_queries': queries,
              'source_elf_unchanged': unchanged, 'passed': passed,
              'self_tests': {'old_alias_bug': old_result, 'changed_copied_count': mutation},
              'assumptions': ['Proof/PyVEX/SMT semantics and initial NZCV model are correct',
                              'All six argument locations follow AAPCS; SP is 0x20008000',
                              'Requests copy zero bytes; fixed output geometry and index per check',
                              'Interrupts and external intervention are absent'],
              'verifier_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'Proof_sha256': hashlib.sha256(Path(__file__).with_name('symbolic_semantics.py').read_bytes()).hexdigest()}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Diagnostic symbolic checks: {len(results)} checks, {len(queries)} replayed queries, passed={passed}')
    return not passed


if __name__ == '__main__':
    raise SystemExit(main())
