#!/usr/bin/env python3
"""Claim-honesty & stub-mutation gate for `targets/rkos/`.

1. Every `reconstructed_c` ledger entry must have at least one *direct* matched
   Unicorn differential case against the raw image (by raw RVA) in
   `arm-semantics.json`, and no diverging case. Its `evidence` list must name
   all and only its matched cases.
2. Stub-mutation sensitivity gate: for every `reconstructed_c` function, replacing
   its ELF machine code with `bx lr` (`70 47`), `movs r0, #0; bx lr` (`00 20 70 47`),
   or `movs r0, #1; bx lr` (`01 20 70 47`) MUST cause at least one of its direct
   differential cases to fail (unless the raw firmware slice `[rva : rva + size]`
   is literally byte-identical to that stub instruction sequence, e.g. a 2-byte
   `nullsub` `bx lr` or 4-byte `movs r0,#0; bx lr`). This prevents single-outcome
   or no-op evidence from ever regressing.
3. `retained_native` is only allowed for byte-identical original instructions;
   this tree has none, so the count must be 0.
4. Code-pointer audit: the binary-backed ROM_RODATA / RAM_DATA blobs carry raw
   firmware code addresses (init_array, runtime VTOR, vtables, ...). Those are
   NOT relocated to rkos.elf symbols. The audit recounts them and requires
   `ledger.json:known_deviations` to declare the same count, so the deviation
   cannot silently drift.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import struct
import sys

ROM_RODATA = (0x13090, 0x17590)
RAM_DATA_VMA, RAM_DATA_FILE, RAM_DATA_LEN = 0x20000000, 0x20000, 0xBA4

STUBS: dict[str, bytes] = {
    "bx_lr": b"\x70\x47",
    "ret0": b"\x00\x20\x70\x47",
    "ret1": b"\x01\x20\x70\x47",
}


def code_pointer_audit(raw: bytes, inv: dict, led: dict, elf_funcs: dict[int, str]) -> dict:
    starts = {int(f["rva"], 16) for f in inv["functions"]}
    # A raw pointer is "resolved" only if rkos.elf places the C symbol for that same
    # IDA function at the same address (e.g. pinned vector entries), not merely
    # some unrelated symbol that happens to land there.
    owners = {int(f["rva"], 16): {f.get("c_symbol"), f.get("c_shim")} - {None} for f in led["functions"]}
    hits: list[dict] = []

    def scan(blob: bytes, base: int, region: str) -> None:
        for off in range(0, len(blob) - 3, 4):
            val = struct.unpack_from("<I", blob, off)[0]
            if val & 1 and (val & ~1) in starts:
                tgt = val & ~1
                hits.append({
                    "region": region,
                    "slot_va": hex(base + off),
                    "raw_target": hex(tgt),
                    "elf_has_same_function_at_target": elf_funcs.get(tgt) in owners.get(tgt, set()),
                })

    scan(raw[ROM_RODATA[0]:ROM_RODATA[1]], ROM_RODATA[0], "ROM_RODATA")
    scan(raw[RAM_DATA_FILE:RAM_DATA_FILE + RAM_DATA_LEN], RAM_DATA_VMA, "RAM_DATA")
    unresolved = [h for h in hits if not h["elf_has_same_function_at_target"]]
    return {"thumb_code_pointers": len(hits), "unrelocated": len(unresolved), "entries": hits}


def elf_function_addrs(elf_path: Path) -> dict[int, str]:
    from elftools.elf.elffile import ELFFile  # type: ignore

    out: dict[int, str] = {}
    with elf_path.open("rb") as fh:
        elf = ELFFile(fh)
        for sym in elf.get_section_by_name(".symtab").iter_symbols():
            if sym["st_info"]["type"] == "STT_FUNC" and sym["st_value"]:
                out[sym["st_value"] & ~1] = sym.name
    return out


def stub_mutation_audit(root: Path, elf_path: Path, ref_path: Path, raw: bytes, rec: list[dict]) -> tuple[dict, list[str]]:
    spec = importlib.util.spec_from_file_location("arm_sem", root / "tools/arm_semantics.py")
    assert spec is not None and spec.loader is not None
    arm_sem = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(arm_sem)

    rec_by_rva = {int(f["rva"], 16): f for f in rec}
    detected: dict[int, set[str]] = {rva: set() for rva in rec_by_rva}
    literal_identities: list[dict[str, str]] = []

    for rva, f in sorted(rec_by_rva.items()):
        raw_slice = raw[rva:rva + int(f["size"])]
        for sname, sbytes in STUBS.items():
            if raw_slice == sbytes:
                detected[rva].add(sname)
                literal_identities.append({
                    "rva": f["rva"],
                    "ida_name": f["ida_name"],
                    "c_symbol": f.get("c_symbol", ""),
                    "stub": sname,
                })

    active_patch: dict[str, int | bytes | None] = {"addr": None, "stub": None}
    orig_make_vm = arm_sem.CortexM4Pair._make_vm
    orig_run = arm_sem.CortexM4Pair.run_diff_case

    def patched_make_vm(self, is_elf: bool):
        uc = orig_make_vm(self, is_elf)
        if is_elf and active_patch["addr"] is not None:
            uc.mem_write(int(active_patch["addr"]), bytes(active_patch["stub"]))  # type: ignore[arg-type]
        return uc

    def patched_run(
        self,
        suite: str,
        name: str,
        raw_rva: int,
        elf_sym: str,
        setup_fn,
        compare_regions=None,
        check_r0: bool = True,
        check_r1: bool = False,
        mutate_candidate_r0: int = 0,
    ):
        if raw_rva not in rec_by_rva or len(detected[raw_rva]) == len(STUBS):
            # Already proved all 3 stubs fail on earlier cases for this RVA; skip redundant VM runs.
            return {
                "suite": suite,
                "case": name,
                "raw_rva": hex(raw_rva),
                "elf_symbol": elf_sym,
                "elf_addr": hex(self.sym_addr(elf_sym)),
                "raw_r0": "0x0",
                "elf_r0": "0x0",
                "matched": True,
                "mismatches": [],
            }
        base = orig_run(
            self, suite, name, raw_rva, elf_sym, setup_fn,
            compare_regions, check_r0, check_r1, mutate_candidate_r0,
        )
        if base["matched"]:
            sym_va = self.sym_addr(elf_sym)
            for sname, sbytes in STUBS.items():
                if sname in detected[raw_rva]:
                    continue
                active_patch["addr"] = sym_va
                active_patch["stub"] = sbytes
                try:
                    mut_res = orig_run(
                        self, suite, name, raw_rva, elf_sym, setup_fn,
                        compare_regions, check_r0, check_r1, 0,
                    )
                    if not mut_res["matched"]:
                        detected[raw_rva].add(sname)
                except Exception:
                    detected[raw_rva].add(sname)
                finally:
                    active_patch["addr"] = None
                    active_patch["stub"] = None
        return base

    arm_sem.CortexM4Pair._make_vm = patched_make_vm
    arm_sem.CortexM4Pair.run_diff_case = patched_run

    pair = arm_sem.CortexM4Pair(elf_path, ref_path)
    arm_sem.build_test_suites(pair)

    failures: list[str] = []
    undefeated: list[dict[str, object]] = []
    for rva, f in sorted(rec_by_rva.items()):
        missing = sorted(set(STUBS.keys()) - detected[rva])
        if missing:
            undefeated.append({
                "rva": f["rva"],
                "ida_name": f["ida_name"],
                "c_symbol": f.get("c_symbol"),
                "undefeated_stubs": missing,
            })
            failures.append(
                f"reconstructed_c {f['rva']} {f.get('c_symbol')} survived constant/noop stub(s): {missing}"
            )

    summary = {
        "stubs_tested": list(STUBS.keys()),
        "reconstructed_c_checked": len(rec_by_rva),
        "literal_raw_stub_identities": literal_identities,
        "undefeated": undefeated,
    }
    return summary, failures


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=Path("."))
    ap.add_argument("--semantics", type=Path, required=True)
    ap.add_argument("--elf", type=Path, required=True)
    ap.add_argument("--reference", type=Path, required=True)
    ap.add_argument("--output", type=Path, default=None)
    args = ap.parse_args()

    root = args.root.resolve()
    raw = args.reference.read_bytes()
    led = json.loads((root / "verification/ledger.json").read_text(encoding="utf-8"))
    inv = json.loads((root / "verification/inventory.json").read_text(encoding="utf-8"))
    sem = json.loads(args.semantics.read_text(encoding="utf-8"))
    errors: list[str] = []

    matched: dict[int, set[str]] = {}
    diverged: dict[int, set[str]] = {}
    for c in sem["cases"]:
        rva = int(c["raw_rva"], 16)
        (matched if c["matched"] else diverged).setdefault(rva, set()).add(c["case"])

    rec = [f for f in led["functions"] if f["classification"] == "reconstructed_c"]
    for f in rec:
        rva = int(f["rva"], 16)
        if rva not in matched:
            errors.append(f"reconstructed_c {f['rva']} {f['ida_name']} has no direct matched differential case")
        if rva in diverged:
            errors.append(f"reconstructed_c {f['rva']} {f['ida_name']} has diverging cases {sorted(diverged[rva])}")
        ev = set(f.get("evidence", []))
        if not ev or ev != matched.get(rva, set()):
            errors.append(f"reconstructed_c {f['rva']} evidence list does not match passing cases")

    n_ret = sum(1 for f in led["functions"] if f["classification"] == "retained_native")
    if n_ret:
        errors.append(f"{n_ret} retained_native entries but no byte-identical native code is carried")

    stub_summary, stub_errors = stub_mutation_audit(root, args.elf.resolve(), args.reference.resolve(), raw, rec)
    errors.extend(stub_errors)

    audit = code_pointer_audit(raw, inv, led, elf_function_addrs(args.elf))
    declared = {d["id"]: d for d in led.get("known_deviations", [])}
    cp = declared.get("unrelocated_code_pointers")
    if cp is None or cp.get("count") != audit["unrelocated"]:
        errors.append(
            f"known_deviations.unrelocated_code_pointers must declare count={audit['unrelocated']} "
            f"(found {None if cp is None else cp.get('count')})"
        )

    report = {
        "target": "rkos",
        "reconstructed_c": len(rec),
        "reconstructed_c_with_direct_evidence": sum(1 for f in rec if int(f["rva"], 16) in matched),
        "retained_native": n_ret,
        "stub_mutation_audit": stub_summary,
        "code_pointer_audit": audit,
        "errors": errors,
        "passed": not errors,
    }
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if errors:
        for e in errors[:40]:
            print(f"[verify_claims] FAIL: {e}", file=sys.stderr)
        return 1
    print(
        f"[verify_claims] PASS: {len(rec)}/{len(rec)} reconstructed_c claims backed by direct differential "
        f"cases & stub-mutation gate (bx_lr/ret0/ret1: 0 undefeated); retained_native=0; "
        f"{audit['unrelocated']}/{audit['thumb_code_pointers']} raw code pointers declared as known deviation"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
