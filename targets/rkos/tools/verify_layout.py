#!/usr/bin/env python3
"""ELF32 & linker map layout verifier for `targets/rkos/`."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys

EXPECTED_SECTIONS = {
    ".vectors": (0x00000000, 0x000000C0),
    ".rodata":  (0x00013090, 0x00004500),
    ".data":    (0x20000000, 0x00000BA4),
    ".bss":     (0x20000BA4, 0x0001E4C4),
}

EXPECTED_PINNED_ENTRIES = {
    "Reset_Handler":     0x00000108,
    "Reset_Main":        0x000001F8,
    "HardFault_Handler": 0x00000290,
    "NMI_Handler":       0x000002E0,
    "SVCall_Handler":    0x00000374,
    "SysTick_Handler":   0x000034D8,
}

EXPECTED_STATE_SYMBOLS = {
    "g_state_warm":  (0x20000BA4, 0x0001C),
    "g_state_qspi":  (0x20000BC0, 0x00180),
    "g_state_irq":   (0x20000D40, 0x00338),
    "g_state_meta":  (0x20001078, 0x000C8),
    "g_state_arena": (0x20001140, 0x1C000),
    "g_state_psp":   (0x2001D140, 0x00800),
    "g_state_plat":  (0x2001D940, 0x00390),
    "g_state_sched": (0x2001DCD0, 0x00130),
    "g_state_fault": (0x2001DE00, 0x00260),
    "g_state_msp":   (0x2001E060, 0x01008),
}


def parse_elf32(data: bytes) -> tuple[dict[str, dict[str, int]], dict[str, tuple[int, int]]]:
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise ValueError("not a 32-bit little-endian ELF image")
    e_shoff, _, _, _, _, e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<IIHHHHHH", data, 0x20)

    raw_shdrs = []
    for i in range(e_shnum):
        sh = struct.unpack_from("<10I", data, e_shoff + i * e_shentsize)
        raw_shdrs.append(sh)

    shstr_off = raw_shdrs[e_shstrndx][4]
    shstr_sz = raw_shdrs[e_shstrndx][5]
    shstr = data[shstr_off : shstr_off + shstr_sz]

    def get_cstr(blob: bytes, idx: int) -> str:
        end = blob.find(b"\x00", idx)
        return blob[idx:end].decode("utf-8", "replace") if end != -1 else ""

    sections: dict[str, dict[str, int]] = {}
    symtab_sh = None
    for sh in raw_shdrs:
        name = get_cstr(shstr, sh[0])
        sections[name] = {"type": sh[1], "flags": sh[2], "addr": sh[3], "offset": sh[4], "size": sh[5]}
        if sh[1] == 2:  # SHT_SYMTAB
            symtab_sh = sh

    symbols: dict[str, tuple[int, int]] = {}
    if symtab_sh is not None:
        sym_off, sym_sz, str_idx, _, entsz = symtab_sh[4], symtab_sh[5], symtab_sh[6], symtab_sh[7], symtab_sh[9]
        str_sh = raw_shdrs[str_idx]
        strtab = data[str_sh[4] : str_sh[4] + str_sh[5]]
        entsz = entsz or 16
        for pos in range(sym_off, sym_off + sym_sz, entsz):
            st_name, st_value, st_size, _, _, _ = struct.unpack_from("<IIIBBH", data, pos)
            if st_name != 0:
                sname = get_cstr(strtab, st_name)
                symbols[sname] = (st_value, st_size)

    return sections, symbols


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True, help="Built rkos.elf path")
    parser.add_argument("--map", type=Path, required=True, help="Linker map path")
    parser.add_argument("--output", type=Path, default=None, help="Optional JSON report path")
    args = parser.parse_args()

    errors: list[str] = []
    if not args.map.is_file() or args.map.stat().st_size == 0:
        errors.append(f"missing or empty linker map: {args.map}")

    sections, symbols = parse_elf32(args.elf.read_bytes())

    for sname, (exp_addr, exp_sz) in EXPECTED_SECTIONS.items():
        if sname not in sections:
            errors.append(f"missing section {sname}")
            continue
        sec = sections[sname]
        if sec["addr"] != exp_addr or sec["size"] != exp_sz:
            errors.append(
                f"section {sname} mismatch: got addr={hex(sec['addr'])} size={hex(sec['size'])}, "
                f"expected addr={hex(exp_addr)} size={hex(exp_sz)}"
            )

    if ".text" not in sections:
        errors.append("missing .text section")
    else:
        tsec = sections[".text"]
        if tsec["addr"] != 0x000000C0 or tsec["addr"] + tsec["size"] > 0x00013090:
            errors.append(
                f".text bounds violation: addr={hex(tsec['addr'])} end={hex(tsec['addr'] + tsec['size'])}"
            )

    for sym, exp_rva in EXPECTED_PINNED_ENTRIES.items():
        if sym not in symbols:
            errors.append(f"missing pinned vector entry symbol {sym}")
            continue
        actual_rva = symbols[sym][0] & ~1
        if actual_rva != exp_rva:
            errors.append(f"pinned entry {sym} at {hex(actual_rva)}, expected {hex(exp_rva)}")

    for sym, (exp_addr, exp_sz) in EXPECTED_STATE_SYMBOLS.items():
        if sym not in symbols:
            errors.append(f"missing state block symbol {sym}")
            continue
        s_addr, s_sz = symbols[sym]
        if s_addr != exp_addr or s_sz != exp_sz:
            errors.append(
                f"state block {sym} mismatch: got {hex(s_addr)}+{hex(s_sz)}, expected {hex(exp_addr)}+{hex(exp_sz)}"
            )

    bss_start = symbols.get("__bss_start", (0, 0))[0]
    bss_end = symbols.get("__bss_end", (1, 0))[0]
    if bss_start != 0x2001F068 or bss_end != 0x2001F068:
        errors.append(f"wildcard .bss non-empty: __bss_start={hex(bss_start)} __bss_end={hex(bss_end)}")

    passed = len(errors) == 0
    report = {
        "target": "rkos",
        "elf": str(args.elf),
        "map": str(args.map),
        "text_size_bytes": sections.get(".text", {}).get("size"),
        "pinned_entries_checked": len(EXPECTED_PINNED_ENTRIES),
        "state_blocks_checked": len(EXPECTED_STATE_SYMBOLS),
        "errors": errors,
        "passed": passed,
    }

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if not passed:
        for err in errors:
            print(f"[verify_layout] FAIL: {err}", file=sys.stderr)
        return 1

    print(
        f"[verify_layout] PASS: .vectors/.text({hex(sections['.text']['size'])})/.rodata/.data/.bss "
        f"+ {len(EXPECTED_PINNED_ENTRIES)} pinned entries + {len(EXPECTED_STATE_SYMBOLS)} state blocks verified"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
