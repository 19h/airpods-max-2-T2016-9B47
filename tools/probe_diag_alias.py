#!/usr/bin/env python3
"""Diagnostic response/descriptor aliases, with all Unicorn hooks removed.

The descriptor checksum and length are sampled before any response store;
its source pointer is sampled after all header stores. These are finite mapped
RAM cases, not a proof of arbitrary stack aliases, faults or physical execution.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from unicorn import UcError
from arm_semantics import Machine, REFERENCE_SHA256, TEST


def cases():
    for index in range(3):
        # Zero copies keep arbitrary pointer-cell overwrites from adding the
        # already-known ROM-layout or unmapped-pointer domains to this matrix.
        for delta in range(-16, 32):
            for length in (0, 1, 0x123, 0xFFFF):
                yield index, delta, 0x1234, 0, length, 0x11223344
        # These destinations preserve base+4..7, while perturbing the fields
        # that must have been sampled before the stores.
        for delta in range(8, 17):
            for requested in (1, 32, 149, 150, 151, 255, 256, 0xFFFFFFFF):
                for length in (0, 1, 32, 149, 150, 256, 0xFFFF):
                    yield index, delta, 0, requested, length, 0x11223344
        # Header checksum overwrites the low half of base. Both resulting
        # addresses are mapped, with different source bytes.
        for requested in (1, 4, 32, 150):
            yield index, -4, 0, requested, 150, 0x2000A55A


def check(elf, case):
    index, delta, offset, requested, length, checksum = case
    descriptor = 0x4D28A2D0 + 16 * index
    observations = []
    for reference in (True, False):
        machine = Machine(reference, elf)
        for handle in machine.observer_handles:
            machine.cpu.hook_del(handle)
        machine.write(0x4D28A2C0, bytes([0x5A]) * 0x200)
        machine.write(TEST + 0x1000, bytes(range(256)) * 2)
        machine.write(TEST + 0x2000, bytes(reversed(range(256))) * 2)
        machine.word(TEST + 0x3000, 0xD00DFEED)
        machine.write(descriptor, struct.pack('<B3xIHHI', 1, TEST + 0x1000, length, 0, checksum))
        try:
            result = machine.call(0x4DBC if reference else 'diag_buffer_read',
                                  [descriptor + delta, TEST + 0x3000, index,
                                   offset, requested, 0xA6])
            observations.append({'return': result,
                                 'copied': machine.read(TEST + 0x3000, 4).hex(),
                                 'RAM': machine.read(0x4D28A2C0, 0x200).hex()})
        except (UcError, RuntimeError) as error:
            observations.append({'error': str(error)})
    return {'index': index, 'destination_delta': delta, 'source_offset': offset,
            'requested': requested, 'descriptor_length': length, 'checksum': hex(checksum),
            'reference': observations[0], 'rebuilt': observations[1],
            'match': observations[0] == observations[1]
                     and all('error' not in observation for observation in observations)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    original = args.elf.read_bytes()
    results = [check(args.elf, case) for case in cases()]
    unchanged = original == args.elf.read_bytes()
    report = {'scope': __doc__, 'reference_sha256': REFERENCE_SHA256,
              'elf_sha256': hashlib.sha256(original).hexdigest(),
              'case_count': len(results), 'divergences': sum(not c['match'] for c in results),
              'complete': True, 'source_elf_unchanged': unchanged,
              'observer': 'none; all Unicorn hooks removed', 'cases': results}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Diagnostic aliases: {report['case_count']} cases, {report['divergences']} divergences")
    return report['divergences'] != 0 or not unchanged


if __name__ == '__main__':
    raise SystemExit(main())
