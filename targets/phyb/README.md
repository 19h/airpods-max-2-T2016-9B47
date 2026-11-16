# `phyb` — Apple W3 Bluetooth PHY Firmware, Reconstructed (partial)

Gradual freestanding ARMv7E-M + host-testable C11 reconstruction of the Apple
W3 Bluetooth/Wireless PHY coprocessor firmware (`WxBtPhy/firmware/w3`, build
`185{T:w3}{C:B0}.debug`, on `RTKit_debug-3514.0.15.debug`), derived from
`phyb.i64`, the IDA disassembly (`phyb.asm`) and the Hex-Rays listing
(`phyb.c`). The two exports are not kept in the repository; regenerate them
from `phyb.i64` when needed. The function inventory they provided is in
`verification/ledger.json`.

> **Source location:** unlike the other targets, the `phyb` source tree lives at
> the workspace root (`src/`, `include/`, `link/`, `tests/`, `tools/`) because it
> predates the multi-target layout. This directory holds the `phyb`-specific
> documentation only. Build and verification commands are run from the workspace
> root.

## Reference Binary & IDA Inventory

| Property | Value |
|---|---|
| Raw firmware | `../../../phyb` (`74,240` B, SHA-256 `15f3ce1c30b82e3f87c45974284ccfa68e10928fea0f09a1a46376ac04cd50d4`) |
| IDA database | `../../../phyb.i64` (`232` functions: `219` in `phyb.c` + `13` 2–4 B IRQ/nop stubs) |
| Architecture | ARMv7E-M Thumb-2, Cortex-M4F (`-mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16`, `-Oz`) |
| Product build | `185{T:w3}{C:B0}.debug` (ROM `0x6B60`) |
| RTKit runtime | `RTKit_debug-3514.0.15.debug` (ROM `0x7D00`) |
| `uuid` header | `0x5fa9e1613a11373e968af4be63f54183` at ROM `0xD0` |
| `fwsg` v1 trailer | `__TEXT` `0x0..0x88D2`, `__DATA` file `0x88E0` → VMA `0x4D280000`, table `0x121A0` |
| Toolchain | Apple Clang/LLVM with machine outliner (`30` outlined helpers), `ld.lld` |

## Current Reconstruction Status

- **Inventory:** `219` recovered functions, `197` `@provenance` claims, `30`
  machine-outlined helpers exempt from mandatory claims.
- **Binary-backed content:** all `22` pinned state blocks at their documented
  addresses; `7,558` B readonly bank `0x6B4C..0x88D2` exact; `74,127` B
  file-backed loaded bytes identical; `28/28` ELF sections covered by `PT_LOAD`.
- **Differential evidence:** `124,801` archived finite comparisons with `64`
  documented divergences; the semantic gate **fails** on the ROM-observability
  counterexample (§ below).
- **Retained debt:** selected libc bodies (inline/Thumb assembly), the native FRS
  scan body, naked architectural entry veneers, and the TX-power mode 4/5
  behavior gap.

C recovery is incomplete: retained ARM instruction bodies remain in parts of
the tree and are recorded as work still required for the C-only deliverable.

The code is written the way the original source plausibly looked: one
translation unit per subsystem, hardware described by register/field headers,
shared memory described by structs, module state in named structs, and
behaviour expressed with the vocabulary of the firmware's own strings. Every directly reconstructed
non-outlined function carries a `@provenance RVA 0x… <name>` tag
naming the recovered symbol it corresponds to, so the source can still be read
against the disassembly in `phyb.i64`.

`CONSIDERATIONS.md` holds the analysis notes about the binary itself.
[RODATA_RECOVERY.md](RODATA_RECOVERY.md) explains the recovered readonly tables,
146 named strings, overlapping lookup views and the unclassified zero span.
[NAME_RECOVERY.md](NAME_RECOVERY.md) records the evidence-backed naming pass,
including the TX-test override field and two newly identified C-model fidelity gaps.

The current linked image has a confirmed semantic divergence. An accepted NVV
register-write record can replace the DPD actuation pointer with a ROM address.
TX setup then copies executable bytes to the hardware aperture. The original
and reconstructed executable bytes differ at the same addresses. Six ROM-input
comparisons diverge; two ordinary-RAM controls match, and all final states agree
between instruction-observed and completely unhooked executions. Therefore the
current build is not equivalent over the accepted input domain. The semantic
verification target now fails on this counterexample. Details and reproduction
are in [verification/rom-observability.md](../../verification/rom-observability.md).

## Build

Run these from the workspace root, where the `phyb` source and `Makefile` live.

```sh
make all        # ARMv7E-M ELF (build/phyb.elf) + native test binary
make arm        # build/phyb.elf (clang -target armv7em-none-eabi, ld.lld)
make host       # build/phyb_tests (system compiler, sparse address bus)
make check      # compile-time layout proof + behavioural tests
make verify     # linked RAM map vs. memmap.h + provenance audit
make verify-semantics  # finite differential ARM execution against ../phyb
```

The target build needs a Clang that can target `armv7em-none-eabi` and
`ld.lld`; no C library is required (the image carries its own, `src/libc/`).

Instruction comparisons additionally require Python `unicorn==2.1.4`,
`pyelftools==0.33`, `capstone==5.0.9`, and `qemu-system-arm`. Use an isolated Python environment:

```sh
python3 -m venv /tmp/phyb-verify-venv
/tmp/phyb-verify-venv/bin/pip install unicorn==2.1.4 pyelftools==0.33 capstone==5.0.9
make check verify verify-semantics BUILD=/tmp/phyb-semantic-build \
  SEMANTIC_PYTHON=/tmp/phyb-verify-venv/bin/python
```

Results are written to the selected build directory as `architecture.json`,
`packets.json`, `boot.json`, `leaves.json`, `mac.json`, `mac-alias.json`, `power.json`,
`tx-calibration.json`, `tx-volatile.json`, `tx-modes.json`, `nvv.json`, `loopback.json`, `loopback-alias.json`, `status-bindings.json`, `trace.json`, `dpd.json`, `dpd-workflow.json`, and
`formatter-qemu.json`. The separate `loopback-qemu.json` covers Normal-memory
loopback execution without memory hooks. `mac-crash-qemu.json` checks crash
descriptor overlap independently; `dpd-qemu.json` checks command/capture-IRQ
results with SCS addresses explicitly redirected to Normal RAM; `instruction-observer.json` checks the MAC
instruction observer against a conditional-load/store oracle. Every comparison verifies the raw
firmware SHA-256.
Each process freezes its ELF and checks that the file remained unchanged.
Matched RAM is compared byte for byte before report snapshots become digests.

`frs.json` adds 1740 entry/exit comparisons over all eight bands, automatic and
forced gain boundaries, NVV-loaded calibration tables, raw single-channel bytes,
null/present result buffers, two SysTick progress steps and changing register
responses. The historical image reread `SEQ_RF_CTRL` before its first mode
write, unlike the reference. That extra read is corrected. These checks cover
entry/exit and supplied polling responses; complete scan/calibration workflows,
arbitrary output aliases and asynchronous devices remain separate obligations.

`frs-scan.json` covers 3456 scan/calibration/workflow cases. Independent QEMU
execution in `frs-process-qemu.json` covers 224 calibration and 1296 bounded
scan cases without hooks. The scan fixtures explicitly use zero-reload delays,
settled AGC and ordinary RAM at nominal device/SCS addresses. Four further
register-sampling/order differences were corrected. The target scan now retains its native Thumb instructions, stack frame and
original 0x0D7C address. All eight selector-zero cases and seven seeded-stack
QEMU cases match; two cases compare terminal failed accesses before exception
delivery. Native bytes, 18 call targets, message data and mutation controls are
checked in `frs-native.json`. Physical selector-zero reachability and exception
delivery remain unknown. Details,
assumptions and reproducible commands are in
[verification/frs-scan.md](../../verification/frs-scan.md).
The final semantic gate runs both selector-zero probes and the ROM probe,
retains all reports and returns nonzero for any remaining divergence.

The loaded readonly bank at `0x6B4C..0x88D2` now matches all 7558 reference
bytes. Load-image verification additionally caught and corrected 71 bytes of
section data omitted from PT_LOAD segments. The ROM domain audit covers every
nonzero byte address through 37 accepted pointer windows; byte zero already
matches. A separate callback check retains the debug stub's fixed `0x41C`
entry and verifies NVV-installed callbacks. These checks do not establish
whole-image executable-byte identity. Details and assumptions are in
[verification/rom-domain.md](../../verification/rom-domain.md).

Unicorn 2.1.4 generic RAM memory hooks can corrupt Thumb IT-condition behavior.
MAC probes use a Capstone-based instruction observer because even peripheral
memory hooks can leave stale IT state. The observer records executed accesses,
rejects unsupported peripheral instructions, and is checked against unhooked
execution and a known conditional-load/store oracle. Other suites scope memory
hooks to peripheral/SCS ranges and include an
unaligned copy/return observer regression check. This resolves the previously
observed formatter and reload-copy discrepancies. `formatter-diagnostic` remains
an additional Unicorn formatter comparison; QEMU independently runs the scalar
libc implementations with Normal memory. The matrix includes string alignment,
high-bit bytes, truncation and overlapping `strlcpy`/`strlcat` calls. Target
string routines retain the reference instruction widths and order; host adapters
retain ordinary-memory results. Exact instruction comparisons decode and verify
external call relocations. The loopback alias matrix also checks byte/word access order, raw active-byte
values, accumulator wraparound, request/readback overlap, and supplied changing
control-byte responses. Its QEMU counterpart checks final memory independently;
it does not reproduce changing read responses. Neither emulator models actual
PHY hardware.

The initialized RAM bytes and original addresses are retained, including shared
NVV data and pointer cells. Reload source starts at runtime address `0x88D2`,
without the raw file's `0xE`-byte segment padding. The boot matrix covers DSRL=0
and DSRL=1. Its `0x9900`-byte reload extends `0x43` bytes past the `0x98BD`-byte
initialized payload. Those trailing physical source bytes remain unknown; the
model supplies zeros, and the ELF does not specify them. Physical loader
validation remains outstanding.

Conditional symbolic checks use a separate environment:

```sh
python3.13 -m venv /tmp/phyb-symbolic-venv
/tmp/phyb-symbolic-venv/bin/pip install -r verification/requirements-symbolic.txt
make verify-symbolic BUILD=/tmp/phyb-semantic-build \
  SYMBOLIC_PYTHON=/tmp/phyb-symbolic-venv/bin/python
```

`symbolic.json` records complete-path coverage and difference queries for the
selected scalar routines, including arbitrary mutable RAM, all initial NZCV
patterns, declared argument domains and independent MMIO responses per read.
Predicated accesses retain their guards. SMT-LIB queries are exported to
`symbolic-smt/` and replayed through the separate Z3 Python API. Deliberate
changed-return, missing-path and inverted-event-guard controls test detection;
unbounded symbolic pointers fail closed. These are conditional results under
the recorded lifting/SMT and entry-state assumptions. Fixed initialized pointer
cells are a per-check precondition, not a firmware-wide invariant: NVV register
records can modify them. The TX power and loopback consumers now follow their
original mutable cells, with corresponding differential fixtures.

Diagnostic descriptor/header overlaps have an additional 2100 unhooked
comparisons and 2100 independent QEMU Normal-memory comparisons. Both engines
reproduce 1409 divergences in the retained old object and none after preserving
checksum/length preloads, header stores and the late source-pointer load.
`diag-mac.json` additionally covers 216 accepted-NVV/MAC workflows in paired
observer modes: 144 old divergences, zero current divergences. The executed
eight-access sampling order is checked in `diag-sampling.json`.

`diag-symbolic.json` adds 27 conditional complete-domain checks for zero-byte
copies at fixed overlap geometries. Every 32-bit offset and symbolic descriptor
bytes are covered under the declared model, with explicit stack arguments.
The old bug and a changed copied-count mutation are detected; 212 queries are
retained in `diag-symbolic-smt.tar.gz` and independently replayable through
`tools/replay_smt.py`. These results do not prove arbitrary pointer/stack aliases,
nonzero-copy domains or physical execution.

The accepted ELF, compressed reports, historical counterexamples and exported
SMT queries are retained in `verification/evidence/`; hashes are listed in
`verification/input_manifest.json`. Replay the archived queries without loading
angr or firmware:

```sh
/tmp/phyb-symbolic-venv/bin/python tools/replay_smt.py \
  --report verification/evidence/symbolic.json.gz \
  --queries verification/evidence/symbolic-smt.tar.gz
```

Full semantic equivalence is refuted for the current linked image by the ROM
counterexample above. The earlier passing comparisons cover finite cases;
unresolved functions, paths and assumptions are tracked in
`verification/ledger.json`.

## Layout

[SOURCE_LAYOUT.md](SOURCE_LAYOUT.md) distinguishes the source paths exposed by
the firmware from inferred subsystem boundaries and records the RTKit/W3 split.

| Path | Contents |
|------|----------|
| `src/rtkit/boot.c` | reset, stack painting, cold/warm boot, stack guard |
| `src/rtkit/image.c` | RTKit wire records and initialized shared RAM bytes |
| `src/rtkit/irq.c` | NVIC, BASEPRI levels and CPU idle/halt |
| `src/rtkit/exception.c` | fault entry and architectural frame capture |
| `src/rtkit/threading.c` | SVC/PendSV entry and scheduler hooks |
| `src/w3/exception.c` | W3 crash records, panic reporting and MAC notification |
| `src/w3/main.c` | `btp_main()`, early clock bring-up, PHY/modem reset sequence |
| `src/w3/clock.c` | clock controller, SysTick, 48-bit sequencer timestamps |
| `src/w3/mac.c` | MAC↔PHY mailbox: commands, sub-commands, overrides, responses |
| `src/w3/nvv.c` | NVV calibration container parser, module identity, modem init |
| `src/w3/syscfg.c` | SysCfg TLV block upload, checksum and application |
| `src/w3/rx.c` | per-packet RX setup/done, RSSI/CFO statistics, max-gain tables, coex back-off |
| `src/w3/tx.c` | per-packet TX setup/done, CW tone and PRBS test modes |
| `src/w3/txpwr.c` | TX power: calibration interpolation, limits, PA stage selection |
| `src/w3/dpd.c` | digital pre-distortion capture/actuation experiments |
| `src/w3/frs.c` | fast RSSI scan |
| `src/w3/loopback.c` | TX→RX loopback measurement via the two correlators |
| `src/w3/trace.c` | RTKit trace ring, error events, debug console stub |
| `src/w3/status.c` | radio revision, warm boot, telemetry, version, diagnostic buffers |
| `src/libc/` | `memcpy`/`memmove`/`memset`/`str*`, `snprintf`, EABI helpers |
| `src/w3/vectors.c` | 48-entry vector table and IRQ entry points |
| `include/btp/regs_*.h` | register blocks: radio, TX digital, DPD, RX digital, sequencer, clock |
| `include/btp/shared.h` | mailbox, status page, trace ring, crash record |
| `include/btp/memmap.h` | fixed addresses: shared regions, state blocks, stacks |
| `include/btp/<module>.h` | per-module state structs, enums and prototypes |
| `link/phyb.ld` | ROM/RAM map; pins every state block to its recovered address |
| `tests/` | layout proof (`layout.c`), host bus model, regression tests |
| `tools/` | layout/provenance inventory and ARM differential harnesses |

## Conventions

* Registers are named by block and function (`SEQ_RF_CTRL`, `RX_PKT_STATUS`)
  with fields as `RFIELD(shift, width)` constants; `REG_WRITE_FIELD`,
  `REG_SET_BITS` and friends generate the same read-modify-write sequences the
  hardware expects. Where a field's purpose could not be inferred it is named
  after its position and says so.
* Memory shared with the host is addressed absolutely (`MAC_MAILBOX_BASE`,
  `PHY_STATUS_BASE`, …) through packed structs; module state is ordinary C
  data pinned by the linker script so the recovered RAM map is preserved.
* Debug printing (`dbg_printf`) is compiled out, but format strings and
  arguments are kept verbatim; they document the firmware's vocabulary.
* Assertions use the line numbers of the original build so crash messages
  stay byte-identical (`btp_panic_at(0x51C, …)` in `mac.c`).

## C reconstruction scope

The required deliverable is the C reconstruction. The separate complete
reassembly candidate has been removed from the source tree and build targets;
its exact-image result is not used as proof of C correctness. Existing retained
instruction bodies remain work to recover in C, with architecture/ABI behavior
recorded explicitly. The current semantic gate still fails on the documented
ROM-observability counterexamples.

## Multi-target ftab reconstruction suite

Alongside `phyb` at the repository root, this workspace integrates freestanding
C11 reconstructions, host test suites, and Unicorn/Capstone differential
harnesses for the four companion ARMv7E-M RTKit firmware images in the parent
`ftab_dump/` bundle (`T2016` B0 / `b515d` platform, shared `RTKit-3514.0.15`
lineage). Cross-firmware boot topology, `uuid` v5 / `fwsg` v1 wire records,
CPU/FPU flags, and shared MMIO/SRAM IPC apertures (`0x42440000`, `0x42034000`,
`0x42038000`, `0x4D154000`, `0x4D280000`) are documented in
[FIRMWARE_MAP.md](../../FIRMWARE_MAP.md).

| Target | Raw Image | Subsystem | Tree | IDB Functions | Reconstructed C | Differential Cases |
|--------|-----------|-----------|------|--------------:|----------------:|-------------------:|
| `phyb` | `../phyb` (`74,240 B`) | W3 Bluetooth / Wireless PHY (`185{T:w3}{C:B0}.debug`) | Root (`src/`, `include/btp/`, `link/phyb.ld`) | 232 (219 in `phyb.c`) | 189 (+30 helpers) | 124,801 (archived) |
| `rkos` | `../rkos` (`134,176 B`) | Stage-1 RTKit OS boot & FTAB/IMG4 loader (`RTKitOSPlatform-3431.0.25~004286`) | `targets/rkos/` | 864 | 103 | 528 (Unicorn) |
| `aopf` | `../aopf` (`597,056 B`) | Always-On Processor SPU / VoiceTrigger / Audio DSP (`b515daop`) | `targets/aopf/` | 3,915 | 91 | 214 (Unicorn) |
| `acib` | `../acib` (`1,066,208 B`) | Bluetooth Controller HCI / Link Controller / W3 MAC (`3.91.1.0`) | `targets/acib/` | 9,917 | 120 (+1 native) | 200 (Unicorn) |
| `rkof` | `../rkof` (`2,159,008 B`) | Main RTKit OS + Audio + AFK/UARP + W3 supervisor | `targets/rkof/` | 17,479 | 108 (+5 veneers) | 175 (Unicorn) |
| **Total** | **5 `ftab` images** | **Apple Silicon `T2016` B0 (`b515d` / `AirPods3,6`, `RTKit-3514.0.15`)** | **Root + `targets/*`** | **32,407** | **611** | **125,918** |

### Multi-target Make & audit commands

All root `phyb` targets above behave identically; the historical single-target
root `Makefile` remains hash-bound in `verification/fixtures/single-target-root/`
so `make verify-archive` stays reproducible. Sub-target outputs are isolated under
`$(BUILD)/<target>`:

```sh
make rkos               # build targets/rkos into $(BUILD)/rkos
make aopf               # build targets/aopf into $(BUILD)/aopf
make acib               # build targets/acib into $(BUILD)/acib
make rkof               # build targets/rkof into $(BUILD)/rkof

make targets-all        # build ARM ELFs + host runners for rkos, aopf, acib, rkof
make targets-check      # run compile-time layout + host tests across all 4 sub-targets
make targets-verify     # run verify_layout.py + verify_fidelity.py across all 4 sub-targets
make targets-semantics  # run verify_load_image.py + ARM differential suites across all 4 sub-targets

# Unified 5-firmware audit (phyb + rkos + aopf + acib + rkof) -> $(BUILD)/multi-target-summary.json
make verify-all
python3 tools/verify_all_targets.py --build build --output build/multi-target-summary.json
```
