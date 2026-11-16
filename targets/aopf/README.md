# aopf — Apple Always-On Processor (SPU) Firmware, Reconstructed (Phase 1)

Gradual freestanding ARMv7E-M + host-testable C11 reconstruction of the Apple
Always-On Processor firmware (`aopf`, build
`b515daop:MAIN:DEVELOPMENT:AppleSPUFirmwareBuilder-990.0.13~5644` on
`RTKit_release-3514.0.15.release`, SoC `T2016` Rev B0 / `b515d` / `AirPods3,6` /
Laguna), derived from `aopf.i64` and the raw `ftab` payload `../../../aopf`
(`597,056` bytes, SHA-256 `8c9818d7ea1dbc6b99b634b5089300501e446ec2c4a79408818b00660741dfff`).

## Current Reconstruction Status

C recovery is intentionally gradual and mechanically gated:

- **Total IDB functions accounted (`verification/inventory.json` & `verification/ledger.json`)**: **3,915 / 3,915 (100%)**
- **`reconstructed_c`**: **91** self-contained functions across `src/libc/`, `src/rtkit/`, `src/drivers/`, and `src/aop/`. Every `reconstructed_c` function is compiled from clean C11 (`0` banned Hex-Rays idioms), passes direct ARMv7E-M Unicorn differential execution against raw `../../../aopf` at its original RVA with ordered MMIO read+write trace comparison and `udf #255` (`0xDEFF`) unported-ROM traps (**214/214 cases, 0 divergences**), and kills all three constant-return stub mutations (`bx lr`, `movs r0,#0; bx lr`, `movs r0,#1; bx lr`).
- **`retained_native`**: **0** (hand-written arch-entry veneers and partial shims are never claimed as `reconstructed_c` or `retained_native`).
- **`pending_c`**: **3,824** functions (including **70** `@shim` arch-entry, `noreturn` panic/assert, slow-path lock/heap/printf wrappers, and `*_ioctl` C shims kept in `pending_c` until their full RTKit scheduler/format slow paths are reconstructed, plus **3,754** remaining Embedded C++ SPU/VoiceTrigger/DSP and RTKit functions).
- **Binary-backed sections & `PT_LOAD` verification (`verify_load_image.py`)**: **4/4 binary-backed sections (`187,596` bytes)** — `.vectors` (`0xC0`), `.rtkit.header` (`0x48`), `.rodata.rom_bank` (`0x13B14`), and `.data.ram_bank` (`0x1A0B0`) — match `../../../aopf` **100.00% byte-for-byte**, and all allocated file-backed (`SHF_ALLOC`, non-`SHT_NOBITS`) sections are covered by and byte-equal to `PT_LOAD` segments (`0` unpinned floating `.bss` bytes).
- **Known pointer-relocation debt (`ledger.json["known_deviations"]`)**: Verbatim `.rodata.rom_bank` and `.data.ram_bank` preserve all raw firmware tables; **235** function-pointer slots (including **93/93** runtime VTOR slots and **142** RTKit C driver vtable slots across 46 exact-RVA veneers in `src/rtkit/vt_trampolines.c`) resolve into the reconstructed ELF, while **2,630** slots (`2,550` in C++/C vtables, `13` in `.init_array`, and `67` in `RAM_DATA`) still point to raw `ROM_TEXT` RVAs of `pending_c` functions and are mechanically audited by `tools/verify_claims.py`.

## Build & Verification

```sh
make -C targets/aopf all BUILD=/tmp/aopf-build               # ARMv7E-M ELF + host test runner
make -C targets/aopf check BUILD=/tmp/aopf-build             # compile-time _Static_asserts + host tests
make -C targets/aopf verify BUILD=/tmp/aopf-build            # anti-idiom, @provenance/@shim, & ELF map audit
make -C targets/aopf verify-semantics BUILD=/tmp/aopf-build  # byte-exact load check + Unicorn diff + stub-mutation + claim gate
make -C targets/aopf clean BUILD=/tmp/aopf-build
```

From the workspace root, the cross-target audit runner executes the full pipeline:

```sh
/tmp/aopf-venv/bin/python tools/verify_all_targets.py --targets aopf --build /tmp/aopf-audit
```

## Memory Map Summary

| Region | VMA Range | Size | Raw File Offset | Backing |
|---|---|---:|---:|---|
| `ROM_VECTORS` | `0x00000000..0x000000C0` | `0x000C0` (48 entries) | `0x000000` | Typed C11 (`src/vectors.c`), 100% byte-identical |
| `ROM_HEADER` (`uuid` v5) | `0x000000C0..0x00000108` | `0x00048` | `0x0000C0` | Typed C11 (`src/rtkit/image_blobs.c`), 100% byte-identical |
| `ROM_TEXT` | `0x00000108..0x00064000` | `0x63EF8` | `0x000108` | Reconstructed C11 `.text` + exact-RVA entry/vtable veneers |
| `ROM_RODATA` | `0x00064000..0x00077B14` | `0x13B14` | `0x064000` | Verbatim `.rodata.rom_bank` (VTOR, C++/C vtables, TLVs, `.init_array`) |
| `RAM_DATA` | `0x20800000..0x2081A0B0` | `0x1A0B0` | `0x077B20` | Verbatim `.data.ram_bank` (LMA `0x00077B20`, 100% byte-identical) |
| `RAM_BSS` | `0x2081A0B0..0x2089BED8` | `0x81E28` | `NOLOAD` | Zero-init BSS + PSP (`0x20899490..0x2089A890`) + MSP (`0x2089B610..0x2089BA10`) |
| `fwsg` v1 Trailer | — (not mapped) | `0x00060` | `0x091BE0` | 2 reload segment descriptors (`__TEXT`, `__DATA`) + `fwsg` v1 header |

See [PLAN.md](PLAN.md), [CONSIDERATIONS.md](CONSIDERATIONS.md), and [SOURCE_LAYOUT.md](SOURCE_LAYOUT.md) for subsystem roadmaps, Embedded C++ vs RTKit C findings, MMIO apertures, and IPC peer/endpoint maps.
