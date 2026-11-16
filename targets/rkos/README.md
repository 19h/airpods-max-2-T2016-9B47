# `rkos` — Freestanding ARMv7E-M + Host C11 Reconstruction (partial)

Stage-1 RTKit OS / FTAB loader for Apple T2016 B0 (`b515d` / `AirPods3,6`).
Version strings in the image: `RTKit_release-3514.0.15.release`, `RTKitOSPlatform-3431.0.25~004286`.

## Reference Binary & IDA Inventory

| Property | Value |
|---|---|
| Raw firmware | `../../../rkos` (`134,176` B, SHA-256 `adf98c119724a5c6bec397f70fd08d65b3108c261cf0e6ad4b81ede766f18ecf`) |
| IDA database | `rkos.i64` (`864` functions, `899` chunks, `241` strings, `162` MMIO xrefs) |
| Architecture | ARMv7E-M Thumb-2, `-mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16` (11 VFP insns at `0x2B6..0x526`; CPACR written in `sub_47C` `0x500/0x50C` and `rtkit_start` `0x3368/0x3376`) |
| `uuid` header | file `0xC0..0x108`: tag `uuid`, version 5, UUID bytes at `0xD0` = `32166990-6147-3171-a396-018473417533`; other words recorded by value only |
| Metadata TLVs | `0x1714C..0x17580` (28 FourCC TLVs) |
| `fwsg` v1 | segment table `0x20BC0`, footer `0x20C00..0x20C20`; `__TEXT` file `0x0` len `0x17590` → VMA `0x0`; `__DATA` file `0x20000` filesz `0xBA4` memsz `0x1F068` → VMA `0x20000000` |

## Memory Map

| Segment | VMA range | File offset | Notes |
|---|---|---|---|
| `ROM_VECTORS` | `0x00000000..0x000000C0` | `0x0` | 48 entries; word0 = `0x00000000` (no initial MSP in image), Reset = `0x109` |
| `ROM_TEXT` | `0x000000C0..0x00013090` | `0xC0` | `uuid` header `0xC0..0x108`, code `0x108..` |
| `ROM_RODATA` | `0x00013090..0x00017590` | `0x13090` | runtime VTOR `0x13200` (88 entries), `K512` `0x16180`, TLVs, `.init_array` `0x17580` |
| (padding) | — | `0x17590..0x20000` | all zero |
| `RAM_DATA` | `0x20000000..0x20000BA4` | `0x20000` | remap table `0x200004C8`, crashlog state `0x20000730`, `0x200`-B trampoline payload `0x20000988` |
| `RAM_BSS` | `0x20000BA4..0x2001F068` | NOLOAD | 10 pinned `.state.*` blocks: META `0x20001078`, heap arena `0x20001140` (`0x1C000`), PSP `0x2001D140..0x2001D940`, PLAT `0x2001D940`, SCHED `0x2001DCD0`, FAULT `0x2001DE00`, MSP `0x2001E060..0x2001F060` |
| MMIO (IDB segments) | `0x00800000`, `0x40000000`, `0x40007000`, `0x40008000`, `0x4000C000`, `0x40200000..0x403C0000`, `0x42201000`, `0x42400000`, `0x42440000`, SCS `0xE000E000` | — | xref counts in `verification/inventory.json`; peripheral identities are **not** confirmed |

## Ledger (`verification/ledger.json`)

- `864/864` functions accounted.
- **`reconstructed_c`: 103 (11.92%)**. A function gets this class only if `arm-semantics.json` has at least one direct Unicorn case for its raw RVA that matches, zero cases that diverge, AND its evidence defeats `bx lr`, `movs r0, #0; bx lr`, and `movs r0, #1; bx lr` stub mutations unless raw bytes are literally that stub (`tools/verify_claims.py`). Each entry lists its `evidence` cases.
- **`retained_native`: 0**. No original instruction bytes are carried.
- **`pending_c`: 761**. Of these, **77 have a C *shim*** tagged `/* @shim RVA ... */` that only keeps `rkos.elf` linking: 68 C bodies that are unproven or diverge, plus the 9 hand-written naked entry stubs in `src/shims/arch_entry.c`.
- **Known deviations** (listed in `ledger.json:known_deviations`): 473 raw code pointers in the verbatim RODATA/DATA blobs are not relocated (vtables, `.init_array`, 1 RAM slot). The runtime VTOR resolves only to pinned shims. The trampoline is raw code carried as data.

## Build & Verification

```sh
make -C targets/rkos all check verify verify-semantics BUILD=/tmp/rkos-build SEMANTIC_PYTHON=/tmp/rkos-venv/bin/python
```

- `check`: compile-time layout asserts + 78 host checks.
- `verify`: `verify_fidelity.py` (anti-idiom; `@provenance` set == `reconstructed_c`; `@shim` only on `pending_c`; no provenance in `src/shims/`) + `verify_layout.py`.
- `verify-semantics`: `verify_load_image.py` (20,908 B of vectors + uuid + rodata + data byte-exact), `arm_semantics.py` (528 cases; 521 match, and the 7 that diverge all hit `pending_c` shims and are recorded as evidence for the demotion), `verify_claims.py` (claim gate + `bx_lr`/`ret0`/`ret1` stub-mutation gate + code-pointer recount).
