#!/usr/bin/env python3
"""
verify_fidelity.py -- check the source against the recovered function inventory.

Every function in the inventory (the "functions" list of verification/ledger.json:
name, RVA and source file) must be claimed by exactly one
`@provenance RVA 0x... <name>` comment in src/, with the RVA matching and the tag
living in the file the ledger names.  Machine-outlined helpers are compiler
artefacts and are exempt (they are inlined at their call sites).  The script also
rejects the decompiler idioms that the rewrite is meant to eliminate.
"""
from __future__ import annotations

import glob
import json
import re
import sys

LEDGER = "verification/ledger.json"
BANNED = ("__get_CPSR", "__PAIR64__", "HIDWORD(", "LODWORD(", "HIWORD(", "LOWORD(",
          "BYTE1(", "BYTE2(", "HIBYTE(", "LOBYTE(", "__OFSUB__", "__CFADD__",
          "goto LABEL_", " v1;", " a1,", " a1)")


def main() -> int:
    failures = []
    recovered = {}
    expected_source = {}
    with open(LEDGER, encoding="utf-8") as handle:
        for function in json.load(handle)["functions"]:
            key = function["name"].lstrip("_")
            if key in recovered:
                failures.append("%s listed twice in %s" % (function["name"], LEDGER))
            recovered[key] = int(function["rva"], 16)
            if function.get("source"):
                expected_source[key] = function["source"]

    claimed = {}
    sources = sorted(glob.glob("src/**/*.c", recursive=True))
    for path in sources:
        text = open(path, encoding="utf-8").read()
        for rva, name in re.findall(r"@provenance RVA (0x[0-9A-Fa-f]+) (\S+)", text):
            key = name.lstrip("_")
            if key in claimed:
                failures.append("%s claimed twice (%s, %s)" % (name, claimed[key][1], path))
            claimed[key] = (int(rva, 16), path)
        for token in BANNED:
            if token in text:
                failures.append("%s: decompiler idiom %r survives" % (path, token.strip()))

    outlined = {n for n in recovered if "outlined" in n}
    missing = sorted(n for n in recovered if n not in claimed and n not in outlined)
    if missing:
        failures.append("unclaimed functions: " + ", ".join(missing))
    for name, (rva, path) in sorted(claimed.items()):
        if name not in recovered:
            failures.append("%s: @provenance names unknown function %s" % (path, name))
            continue
        if recovered[name] != rva:
            failures.append("%s: %s has RVA 0x%X, ledger says 0x%X"
                            % (path, name, rva, recovered[name]))
        if name in expected_source and expected_source[name] != path:
            failures.append("%s: %s is claimed here, ledger says %s"
                            % (path, name, expected_source[name]))

    if failures:
        for f in failures:
            print("FAIL:", f)
        return 1
    print("provenance inventory passed (not a semantic proof): %d recovered functions, %d outlined helpers exempt, %d claimed"
          % (len(recovered), len(outlined), len(claimed)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
