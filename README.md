# phyb_re — C reconstruction of the Apple T2016 RTKit firmware images

Freestanding C11 reconstructions of the five ARMv7E-M RTKit firmware images in
an Apple accessory firmware bundle for the `T2016` B0 SoC (board `b515d`,
bundle version `90.3431000025000000.6807`). Each reconstruction compiles to an
ARM ELF laid out at the original addresses, builds natively for host tests, and
is checked against the original binary by layout, byte and instruction-level
comparison.

The project started with `phyb`, the W3 Bluetooth PHY coprocessor firmware,
which is why its source sits at the repository root. The other four images were
added later under `targets/`.

This is a partial reconstruction. About 2% of the 32,407 functions across the
five images are recovered as C, and the one nearly complete target (`phyb`) is
known not to be semantically equivalent to the original. See
[Status](#status) and [Known issues](#known-issues).

## Status

As of the last full audit (2026-11-16):

| Target | Role | Image size | Functions | Reconstructed in C | Pending | Tree |
|---|---|---:|---:|---:|---:|---|
| `phyb` | W3 Bluetooth / wireless PHY coprocessor | 74,240 B | 232 | 189 (+30 compiler-outlined helpers) | 0 | repository root |
| `rkos` | Stage-1 RTKit boot and FTAB/IMG4 loader | 134,176 B | 864 | 103 | 761 | `targets/rkos/` |
| `aopf` | Always-on processor: SPU, VoiceTrigger, audio DSP | 597,056 B | 3,915 | 91 | 3,824 | `targets/aopf/` |
| `acib` | Bluetooth controller: HCI, link controller, W3 MAC | 1,066,208 B | 9,917 | 120 (+1 native) | 9,796 | `targets/acib/` |
| `rkof` | Main RTKit OS: audio, AFK/UARP, W3 supervisor | 2,159,008 B | 17,479 | 108 (+5 native veneers) | 17,366 | `targets/rkof/` |

What the two kinds of target contain:

- **`phyb`** has every function accounted for in source, written as one
  translation unit per subsystem with register and shared-memory headers. Some
  bodies are still retained ARM instructions (parts of libc, the fast RSSI scan,
  the architectural entry veneers).
- **`rkos`, `aopf`, `acib`, `rkof`** reconstruct the boot, exception, IRQ, libc
  and inter-processor slices. Read-only and initialized data is carried
  byte-for-byte from the raw image, and every remaining function is listed as
  `pending_c` in that target's ledger.

[FIRMWARE_MAP.md](FIRMWARE_MAP.md) describes how the five images relate: the
shared RTKit image format, the boot chain, the memory map, and the mailbox and
doorbell interfaces between `phyb`, `acib` and `rkof`.

## External inputs

The original firmware is not part of this repository. The tools expect the raw
images `phyb`, `rkos`, `aopf`, `acib` and `rkof` in the parent directory
(`../phyb`, …). They are needed to build the four sub-targets and for every byte
and differential comparison. Their expected SHA-256 values are in
[FIRMWARE_MAP.md](FIRMWARE_MAP.md) §1.

The root `phyb` ELF, its host tests, and `make verify` need no external input.
The IDA databases (`../<name>.i64`) are reference material only; no build or
verification step reads them.

## Requirements

- Clang that can target `armv7em-none-eabi`, plus `ld.lld`. No C library is
  needed; each image carries its own.
- GNU Make, a host C compiler, and Python 3.
- For the differential gates: `unicorn==2.1.4`, `pyelftools==0.33`,
  `capstone==5.0.9`, and `qemu-system-arm` (root `phyb` only).
- For the symbolic gate: a separate Python 3.13 environment built from
  `verification/requirements-symbolic.txt` (angr and Z3).

```sh
python3 -m venv /tmp/phyb-verify-venv
/tmp/phyb-verify-venv/bin/pip install unicorn==2.1.4 pyelftools==0.33 capstone==5.0.9
```

The `aopf` and `acib` `verify` gates also read ELF files, so they need
`pyelftools` too. Pass the interpreter with `SEMANTIC_PYTHON=`.

## Build and check

```sh
# phyb (repository root)
make all            # build/phyb.elf and the host test binary build/phyb_tests
make check          # compile-time layout asserts + host behavioural tests

# the four sub-targets, built into build/<target>/
make targets-all
make targets-check
make targets-verify SEMANTIC_PYTHON=/tmp/phyb-verify-venv/bin/python
```

A single sub-target can be driven on its own, for example
`make -C targets/rkos all check verify`, or `make rkos` from the root to build
it. `BUILD=<dir>` redirects output, and `make clean` removes it.

## Verification gates

| Command | What it checks |
|---|---|
| `make check` | Struct sizes and offsets at compile time, then host tests against a simulated address bus |
| `make verify-layout` | Every pinned state block in the linked map sits at its recovered address |
| `make verify-fidelity` | Every function in `verification/ledger.json` is claimed by exactly one `@provenance` tag in the file the ledger names, and no decompiler idioms survive in `src/` |
| `make verify-semantics` | Loaded bytes against the raw image, then finite differential ARM execution (Unicorn and QEMU) of original against rebuilt code |
| `make verify-symbolic` | Conditional SMT equivalence checks for selected scalar routines |
| `make verify-rom-observability` | The ROM-as-data counterexample described under Known issues |
| `make verify-archive` | Hashes of the pinned source, tools and retained evidence in `verification/evidence/` against `verification/input_manifest.json`. Documentation is not pinned |
| `make targets-verify` | Layout, provenance and ledger audit for each sub-target |
| `make targets-semantics` | Load-image byte match and Unicorn differential execution for each sub-target |
| `make verify-all` | All five targets end to end; writes `build/multi-target-summary.json` |

`make verify` runs `verify-layout` and `verify-fidelity`.

The differential and symbolic gates cover finite cases under stated
assumptions. Neither emulator models the real radio hardware, and none of these
gates proves whole-firmware equivalence. Open functions, paths and assumptions
are tracked in `verification/ledger.json` for `phyb` and in
`targets/<name>/verification/ledger.json` for the rest.

## Conventions

- Every reconstructed function carries a `/* @provenance RVA 0x… <name> */` tag
  naming the function it corresponds to in the IDA database.
- Sub-target ledgers classify each function as `reconstructed_c`,
  `retained_native` (original instruction bytes kept) or `pending_c`. A function
  is `reconstructed_c` only if it matches the original under differential
  execution and its tests reject trivial stub replacements.
- Registers are named by block and function, with fields declared as
  `RFIELD(shift, width)`. Module state is ordinary C data that the linker script
  places at the recovered addresses.

## Documentation

| Document | Contents |
|---|---|
| [FIRMWARE_MAP.md](FIRMWARE_MAP.md) | The five images, shared RTKit format, boot topology, cross-core MMIO and shared SRAM |
| [targets/phyb/README.md](targets/phyb/README.md) | `phyb` status and its verification evidence in detail |
| [targets/phyb/CONSIDERATIONS.md](targets/phyb/CONSIDERATIONS.md) | Analysis notes on the `phyb` binary |
| [targets/phyb/RODATA_RECOVERY.md](targets/phyb/RODATA_RECOVERY.md), [NAME_RECOVERY.md](targets/phyb/NAME_RECOVERY.md) | Read-only table recovery and the naming pass |
| `targets/<name>/README.md` | Per-target identity, memory map, ledger and gates |
| `targets/<name>/PLAN.md`, `CONSIDERATIONS.md`, `SOURCE_LAYOUT.md` | Roadmap, binary analysis and source organisation per target |
| [verification/rom-observability.md](verification/rom-observability.md), [rom-domain.md](verification/rom-domain.md), [frs-scan.md](verification/frs-scan.md) | Write-ups of specific `phyb` verification results |

## Known issues

- **`phyb` is not semantically equivalent to the original.** An accepted NVV
  register-write record can point the DPD actuation pointer at ROM, after which
  TX setup copies executable bytes to hardware. The rebuilt code differs from
  the original at those addresses, so the outputs differ. `make verify-semantics`
  and `make verify-rom-observability` fail on this by design. See
  [verification/rom-observability.md](verification/rom-observability.md).
- **`phyb` C recovery is incomplete.** Retained ARM bodies remain in libc, the
  FRS scan and the entry veneers, and TX-power modes 4 and 5 have a documented
  behaviour gap.
- **Sub-target images are not runnable firmware.** Pointers inside the verbatim
  data blobs (vtables, `.init_array`) still refer to original addresses of
  functions that are not yet reconstructed. Each target's ledger lists these
  under `known_deviations`.
