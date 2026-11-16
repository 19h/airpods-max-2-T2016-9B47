#!/usr/bin/env python3
"""Extract exact binary-backed ROM/RAM slices from raw acib firmware for linking and host tests."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--raw", required=True, type=Path)
    ap.add_argument("--out-dir", required=True, type=Path)
    args = ap.parse_args()

    raw = args.raw.read_bytes()
    if len(raw) != 0x1044E0:
        raise RuntimeError(f"Unexpected acib size: 0x{len(raw):x} (expected 0x1044e0)")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    # 0xc0 .. 0x108: 72-byte RTKit UUID header
    (args.out_dir / "rtkit_header.bin").write_bytes(raw[0x0000C0:0x000108])
    # 0x400 .. 0x640: 576-byte Secondary VTOR vector table
    (args.out_dir / "secondary_vectors.bin").write_bytes(raw[0x000400:0x000640])
    # 0xefea8 .. 0xf984c: 0x99a4-byte ROM_RODATA
    (args.out_dir / "rom_rodata.bin").write_bytes(raw[0x0EFEA8:0x0F984C])
    # file 0xf9860 .. 0x10447e -> VMA 0x10000000 .. 0x1000ac1e: 0xac1e-byte RAM_DATA
    (args.out_dir / "ram_data.bin").write_bytes(raw[0x0F9860:0x10447E])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
