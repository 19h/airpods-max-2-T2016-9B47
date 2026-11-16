#!/usr/bin/env python3
"""Reach diagnostic descriptor overlaps through accepted NVV and a MAC request.

Actual parser and MAC ISR entry execute. NVV redirects the mailbox cell so its
response overlaps the diagnostic descriptor. Offset is nonzero and requested
length is zero. This is scalar entry with ordinary modeled IPC registers, not
physical interrupt delivery, autonomous ACK behavior or hardware reachability.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from unicorn import UcError, UC_HOOK_CODE
from arm_semantics import Machine, REFERENCE_SHA256, TEST
from instruction_observer import InstructionObserver


def check(elf, index, delta, length, mode):
    descriptor = 0x4D28A2D0 + 16 * index
    mailbox = descriptor + delta - 0x408
    record = struct.pack('<HHII', 0xC80B, 8, 0x4D28929C, mailbox)
    blob = struct.pack('<HHI', len(record), 0x4E56, 0) + record
    request = bytearray(0xC4)
    struct.pack_into('<H', request, 2, 1)
    request[5] = 9
    struct.pack_into('<HH', request, 8, 0x1C, 5)
    request[12:17] = struct.pack('<BBHB', 1, index, 0x1234, 0)
    observations, memories = [], []
    for reference in (True, False):
        machine = Machine(reference, elf)
        for handle in machine.observer_handles:
            machine.cpu.hook_del(handle)
        if mode == 'instruction':
            machine.cpu.hook_add(UC_HOOK_CODE, InstructionObserver(machine))
        try:
            machine.write(TEST + 0x800, blob)
            machine.call(0x50B0 if reference else 'nvv_parse_blob', [TEST + 0x800, len(blob)])
            if machine.read(0x4D28929C, 4) != struct.pack('<I', mailbox):
                raise RuntimeError('actual NVV parser did not install mailbox cell')
            machine.write(mailbox, bytes(0x800))
            machine.write(mailbox, request)
            machine.write(descriptor, struct.pack('<B3xIHHI', 1, TEST + 0x1000, length, 0, 0x11223344))
            machine.word(0x4D154C68, 1 << 15)
            machine.call(0x524 if reference else 'mac_isr')
            state = machine.read(0x4D280000, 0xD178)
            memories.append(state)
            observations.append({'RAM_sha256': hashlib.sha256(state).hexdigest(),
                                 'response_hex': machine.read(mailbox + 0x400, 0xBC).hex(),
                                 'descriptor_hex': machine.read(descriptor, 16).hex(),
                                 'IPC_registers_hex': machine.read(0x4D154C60, 16).hex(),
                                 'peripheral_accesses': machine.transactions})
        except (UcError, RuntimeError) as error:
            memories.append(None)
            observations.append({'error': str(error)})
    complete = all(memory is not None for memory in memories)
    differences = [(0x4D280000 + i, a, b) for i, (a, b) in enumerate(zip(*memories)) if a != b] if complete else []
    match = complete and memories[0] == memories[1] and observations[0] == observations[1]
    return {'index': index, 'destination_delta': delta, 'descriptor_length': length,
            'mailbox': hex(mailbox), 'NVV_blob_hex': blob.hex(),
            'MAC_request_hex': request.hex(), 'observer': mode, 'match': match,
            'reference': observations[0], 'rebuilt': observations[1],
            'RAM_different_bytes': len(differences) if complete else None,
            'RAM_difference_sample': [{'address': hex(a), 'reference': x, 'rebuilt': y}
                                      for a, x, y in differences[:32]]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    original = args.elf.read_bytes()
    results = [check(args.elf, index, delta, length, mode)
               for index in range(3) for delta in (-4, 0, 3, 6, 7, 8, 10, 11, 12)
               for length in (0, 1, 0x123, 0xFFFF) for mode in ('instruction', 'none')]
    paired_modes = True
    for a, b in zip(results[::2], results[1::2]):
        for side in ('reference', 'rebuilt'):
            paired_modes &= {k: v for k, v in a[side].items() if k != 'peripheral_accesses'} == {
                k: v for k, v in b[side].items() if k != 'peripheral_accesses'}
            # The fully unhooked observation has an empty transaction list.
            paired_modes &= b[side].get('peripheral_accesses') == []
    unchanged = original == args.elf.read_bytes()
    report = {'scope': __doc__, 'reference_sha256': REFERENCE_SHA256,
              'elf_sha256': hashlib.sha256(original).hexdigest(),
              'case_count': len(results), 'divergences': sum(not c['match'] for c in results),
              'complete': True, 'source_elf_unchanged': unchanged,
              'observer_modes_agree': paired_modes, 'cases': results}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"NVV/MAC diagnostic aliases: {len(results)} cases, {report['divergences']} divergences, modes_agree={paired_modes}")
    return report['divergences'] != 0 or not paired_modes or not unchanged


if __name__ == '__main__':
    raise SystemExit(main())
