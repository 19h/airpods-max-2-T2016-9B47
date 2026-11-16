#!/usr/bin/env python3
"""Replay exported scalar-check queries without loading angr or firmware."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import tarfile
import z3


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--queries', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    data = args.report.read_bytes()
    report = json.loads(gzip.decompress(data) if args.report.suffix == '.gz' else data)
    results = []
    with tarfile.open(args.queries, 'r:gz') as archive:
        for query in report['smt_queries']:
            member = archive.extractfile(Path(query['path']).name)
            if member is None:
                raise ValueError('missing SMT query')
            text = member.read()
            if hashlib.sha256(text).hexdigest() != query['sha256']:
                raise ValueError('SMT query checksum mismatch')
            solver = z3.Solver()
            solver.set(timeout=10000)
            solver.from_string(text.decode())
            actual = str(solver.check())
            results.append({'query': Path(query['path']).name, 'expected': query['result'],
                            'actual': actual, 'match': actual == query['result']})
    output = {'z3_version': z3.get_version_string(), 'query_count': len(results),
              'mismatches': sum(not r['match'] for r in results), 'queries': results}
    if args.output:
        args.output.write_text(json.dumps(output, indent=2) + '\n')
    print(f"{output['query_count']} SMT queries replayed, {output['mismatches']} mismatches")
    return output['mismatches'] != 0


if __name__ == '__main__':
    raise SystemExit(main())
