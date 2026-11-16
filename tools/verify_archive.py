#!/usr/bin/env python3
"""Check archived verification inputs and evidence, without running firmware.

Success establishes archive consistency, not firmware equivalence. Historical
counterexamples remain in the archive and are deliberately not counted as
current ELF evidence. Requires only the Python standard library.
"""
import gzip
import hashlib
import json
from pathlib import Path
import tarfile


ROOT = Path(__file__).resolve().parents[1]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    manifest = json.loads((ROOT / 'verification/input_manifest.json').read_text())
    ledger = json.loads((ROOT / 'verification/ledger.json').read_text())
    evidence = ROOT / 'verification/evidence'
    # Logical source names in historical reports can retain their original paths.
    # Relocations preserve the bytes and hashes while removing obsolete fixtures
    # from the active source tree.
    relocations = manifest.get('source_relocations', {})

    def source_input(name):
        return ROOT / relocations.get(name, name)

    for name, expected in manifest['source_files'].items():
        require(digest(source_input(name).read_bytes()) == expected, f'input: {name}')
    require(digest((ROOT.parent / 'phyb').read_bytes()) == manifest['reference_sha256'],
            'reference firmware')
    reports = {}
    for name, entry in manifest['reports'].items():
        packed = (ROOT / entry['path']).read_bytes()
        require(digest(packed) == entry['sha256'], f'compressed report: {name}')
        data = gzip.decompress(packed)
        require(digest(data) == entry['uncompressed_sha256'], f'report: {name}')
        report = json.loads(data)
        reports[name] = report
        if '-before-' not in name and 'elf_sha256' in report:
            require(report['elf_sha256'] == manifest['elf_sha256'],
                    f'current ELF binding: {name}')
    for name, expected in manifest['extra_artifacts'].items():
        require(digest((evidence / name).read_bytes()) == expected, f'artifact: {name}')
    require(digest((evidence / 'phyb.elf').read_bytes()) == manifest['elf_sha256']
            == ledger['artifacts']['current_elf_sha256'], 'ELF/ledger binding')
    query_path = evidence / 'symbolic-smt.tar.gz'
    require(digest(query_path.read_bytes()) == manifest['query_archive_sha256'],
            'query archive')
    queries = reports['symbolic.json']['smt_queries']
    with tarfile.open(query_path, 'r:gz') as archive:
        names = archive.getnames()
        require(len(names) == len(set(names)) == len(queries), 'query inventory')
        for query in queries:
            member = archive.extractfile(Path(query['path']).name)
            require(member is not None and digest(member.read()) == query['sha256'],
                    f"query: {query['path']}")
    diag_queries = []
    if 'diag-symbolic.json' in reports:
        diagnostic = reports['diag-symbolic.json']
        require(diagnostic['passed'] and diagnostic['check_count'] == 27
                and all(r['status'] == 'unsat for modeled difference' for r in diagnostic['results'])
                and all(r['status'] == 'counterexample' for r in diagnostic['self_tests'].values()),
                'diagnostic conditional checks and controls')
        require(digest((evidence / 'diag-before-alias-fix.elf').read_bytes())
                == diagnostic['before_elf_sha256'], 'diagnostic old-ELF control')
        require(digest((ROOT / 'tools/symbolic_semantics.py').read_bytes())
                == diagnostic['Proof_sha256'], 'diagnostic symbolic engine source')
        diag_queries = diagnostic['smt_queries']
        with tarfile.open(evidence / 'diag-symbolic-smt.tar.gz', 'r:gz') as archive:
            names = archive.getnames()
            require(len(names) == len(set(names)) == len(diag_queries), 'diagnostic query inventory')
            for query in diag_queries:
                member = archive.extractfile(Path(query['path']).name)
                require(member is not None and digest(member.read()) == query['sha256'],
                        f"diagnostic query: {query['path']}")
    entries = {entry['path']: entry for entry in manifest['reports'].values()}
    for item in ledger['evidence']:
        entry = entries.get(item['artifact'])
        if entry is not None and 'uncompressed_sha256' in item:
            require(item['uncompressed_sha256'] == entry['uncompressed_sha256'],
                    f"ledger report binding: {item['scope']}")
    for function in ledger['functions']:
        if function.get('source'):
            require((ROOT / function['source']).is_file(),
                    f"function source: {function['name']}")
    if 'build-image-equivalence.json' in reports:
        build = reports['build-image-equivalence.json']
        require(build['runtime_image_equal'] and build['inputs_unchanged']
                and all(check['match'] for check in build['checks']), 'rebuild runtime-image correspondence')
        require(build['elf_sha256'] == manifest['elf_sha256'], 'tested rebuild binding')
        require(digest((evidence / 'c-rebuild.elf').read_bytes()) == build['rebuilt_elf_sha256'],
                'current source rebuild binding')
        for name, expected in build['source_files'].items():
            require(digest(source_input(name).read_bytes()) == expected, f'rebuild input: {name}')
    finite = [item for item in ledger['evidence'] if item['scope'] != 'formatter-diagnostic']
    result = {'archive_consistent': True, 'elf_sha256': manifest['elf_sha256'],
              'source_inputs': len(manifest['source_files']), 'reports': len(reports),
              'extra_artifacts': len(manifest['extra_artifacts']),
              'SMT_queries': len(queries),
              'diagnostic_SMT_queries': len(diag_queries),
              'finite_comparisons': sum(item.get('case_count', 0) for item in finite),
              'finite_divergences': sum(item.get('divergences', 0) for item in finite),
              'semantic_gate_passed': reports['semantic-domains.json']['passed'],
              'scope': 'Integrity and recorded bindings only; no execution or equivalence proof'}
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
