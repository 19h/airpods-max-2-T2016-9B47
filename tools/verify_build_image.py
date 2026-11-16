#!/usr/bin/env python3
"""Compare tested/rebuilt ELF runtime images, keeping their file hashes distinct.

A rebuild can differ in unallocated debug information while supplying identical
loadable bytes, segment actions, entry point and symbol lookup. This transfers
only results under the declared ELF-loading/symbol-lookup model; it does not
establish firmware equivalence or physical loader behavior.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path

from elftools.elf.elffile import ELFFile
from verify_load_image import loaded_bytes

ROOT = Path(__file__).resolve().parents[1]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def inspect(data):
    image = ELFFile(io.BytesIO(data))
    segments = [(s['p_vaddr'], s['p_paddr'], s['p_filesz'], s['p_memsz'],
                 s['p_flags'], s['p_align'], sha(s.data()))
                for s in image.iter_segments() if s['p_type'] == 'PT_LOAD']
    symbols = [(s.name, s['st_value'], s['st_size'], s['st_info']['type'],
                s['st_info']['bind'], s['st_other']['visibility'], s['st_shndx'])
               for s in image.get_section_by_name('.symtab').iter_symbols()]
    allocated = [(s.name, s['sh_addr'], s['sh_size'], s['sh_type'], s['sh_flags'],
                  s['sh_addralign'], None if s['sh_type'] == 'SHT_NOBITS' else sha(s.data()))
                 for s in image.iter_sections() if s['sh_flags'] & 2]
    sections = {s.name: (s['sh_flags'], s['sh_size'],
                         None if s['sh_type'] == 'SHT_NOBITS' else sha(s.data()))
                for s in image.iter_sections()}
    architecture = (image.elfclass, image.little_endian, image['e_machine'],
                    image['e_type'], image['e_flags'])
    return architecture, image['e_entry'], segments, symbols, allocated, loaded_bytes(image), sections


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tested', type=Path, required=True)
    parser.add_argument('--rebuilt', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    tested, rebuilt = args.tested.read_bytes(), args.rebuilt.read_bytes()
    left, right = inspect(tested), inspect(rebuilt)
    names = ('architecture/ABI', 'entry point', 'ordered PT_LOAD actions/data',
             'symbol lookup entries', 'allocated sections including NOBITS', 'loaded VMA byte map')
    checks = [{'name': name, 'match': a == b} for name, a, b in zip(names, left[:6], right[:6])]
    changed_sections = [name for name in sorted(set(left[6]) | set(right[6]))
                        if left[6].get(name) != right[6].get(name)]
    unchanged = tested == args.tested.read_bytes() and rebuilt == args.rebuilt.read_bytes()
    passed = unchanged and all(c['match'] for c in checks)
    inputs = [ROOT / 'Makefile', ROOT / 'link/phyb.ld']
    inputs += sorted(p for d in ('src', 'include') for p in (ROOT / d).rglob('*')
                     if p.is_file() and p.suffix in ('.c', '.h', '.inc'))
    report = {'scope': __doc__, 'elf_sha256': sha(tested), 'rebuilt_elf_sha256': sha(rebuilt),
              'file_bytes_identical': tested == rebuilt, 'runtime_image_equal': passed,
              'checks': checks, 'different_sections': changed_sections,
              'source_files': {str(p.relative_to(ROOT)): sha(p.read_bytes()) for p in inputs},
              'inputs_unchanged': unchanged,
              'assumptions': ['Loading follows PT_LOAD data/zero-fill actions in order and the ELF entry point',
                              'Program execution and the stated harness symbol lookup do not consume unallocated debug metadata',
                              'Initial state and external inputs are common; physical loading is not established']}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Build image equivalence: runtime={passed}; whole-file equality={tested == rebuilt}; changed sections={changed_sections}')
    return not passed


if __name__ == '__main__':
    raise SystemExit(main())
