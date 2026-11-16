# Source Layout & Firmware Evidence — `phyb`

The `phyb` source tree separates W3 application code from RTKit support and the
freestanding C library. It is kept at the workspace root (not under
`targets/phyb/`) because it predates the multi-target layout; all paths below
are workspace-relative. The firmware is the authority for behavior; this layout
is a reconstruction, not a claim to recover the original build tree.

```text
src/
+-- w3/
|   +-- mac.c               mailbox commands and responses
|   +-- exception.c         crash records and MAC panic notification
|   +-- main.c, vectors.c   PHY startup and vector bindings
|   +-- rx.c, tx.c          packet receive/transmit paths
|   +-- txpwr.c, dpd.c      power and predistortion
|   +-- frs.c               fast RSSI scan
|   +-- frs_scan_arm.inc    retained native scan body
|   +-- loopback.c          TX->RX loopback measurement
|   +-- nvv.c, syscfg.c     configuration inputs
|   +-- clock.c            PHY clock and timing
|   +-- status.c, trace.c   status export and diagnostics
+-- rtkit/
|   +-- boot.c             reset, stacks and core setup
|   +-- irq.c              interrupt masking, NVIC, CPU idle/halt
|   +-- threading.c        SVC/PendSV entry and scheduler hooks
|   +-- exception.c        fault entry and architectural frame capture
|   +-- exception_frame.h  private shared frame constants
|   +-- image.c            image metadata and initialized RAM
|   +-- rom_data.c         typed fixed-address ROM tables and strings
|   +-- *.def              readable ROM string definitions
|   +-- *.inc              initialized RAM image bytes
+-- libc/
    +-- string.c, printf.c, aeabi.c
```

## Evidence strength

| Firmware evidence | Source location | Interpretation |
|---|---|---|
| `Sources/WxBtPhy/firmware/w3/mac.c` | `src/w3/mac.c` | Exact surviving application path suffix |
| `Sources/WxBtPhy/firmware/w3/exception.c` | `src/w3/exception.c` | Exact surviving application path suffix |
| `threading.c`, context-frame assertion strings | `src/rtkit/threading.c` | Exact basename; RTKit directory is inferred |
| RTKit exception-class names and architectural register/frame handling | `src/rtkit/exception.c` | Inferred kernel-side boundary |
| Existing PHY subsystem vocabulary and functions | Other `src/w3/*.c` files | Application grouping inferred; exact original filenames are not established |
| Existing image metadata and retained data fragments | `src/rtkit/image.c`, `rom_data.c` | Reconstruction-specific packaging |

W3 exception reporting receives a decoded frame from RTKit, creates the crash
record, notifies the MAC, and halts. RTKit exception code handles machine entry,
frame construction and fault-register capture. The SVC/PendSV veneer belongs
with the scheduler consumer in `threading.c`, rather than with the fault path.
The two veneers share only private frame-layout constants.

Public declarations remain under `include/btp/` to preserve the existing
include interface. Function names, function bodies, provenance RVAs, state
layouts, image fragments and build-comparison comments are retained.

## Build and verification integration

The Makefile discovers `src/w3/`, `src/rtkit/`, and `src/libc/`. Object files now
follow the source tree, for example `BUILD/arm/w3/mac.o`. Direct-object QEMU
harnesses use those paths. The current per-function verification ledger points
to the moved/split sources. The main semantic archive is refreshed against this
layout by the semantic verification run; `make verify-archive` checks its source,
report and ELF bindings. The structural comparison report below retains its
own before/after snapshot paths and hashes.

Build and check in an isolated directory with:

```sh
make -j4 all check verify BUILD=/tmp/phyb-layout-check
```

`make verify-fidelity` verifies ownership tags, not semantic equivalence.
Source relocation changes translation-unit and linker ordering, so equal
function-body tokens alone do not establish binary or runtime equivalence.
The existing ROM-as-data counterexample documented in the target README remains
outside this structural refactoring.

## Validation of this refactor (29 October 2026)

This section is a dated historical snapshot of the source-layout refactor, not
the current status report. Current counts and gates are in
[README.md](README.md) and [PLAN.md](PLAN.md).

Both the pre-refactor and refactored sources were built in separate temporary
build directories. All 289 extracted function bodies retain identical token
sequences. The public headers and retained data bytes were not changed.

| Check | Result |
|---|---|
| ARM and host builds | Pass with warnings treated as errors |
| Host regression checks | 73 pass before and after |
| Pinned state layout | All 22 blocks verified |
| Function provenance | 219 recovered, 197 claims, 30 outlined exemptions |
| Architecture comparisons | 29 pass before and after |
| Boot comparisons | 34 pass before and after |
| Leaf comparisons | 2,550 pass before and after |
| MAC comparisons | 642 pass before and after |
| QEMU crash-notification comparisons | 2,160 pass after |
| QEMU DPD comparisons | 496 pass after |
| QEMU loopback comparisons | 1,280 pass after |
| QEMU FRS comparisons | 1,520 pass after |
| Loaded image coverage and reference readonly bytes | Pass after |
| Native FRS scan, 18 call bindings and mutation controls | Pass after |
| Retained image/data section bytes | Identical before and after |

These are bounded regression results, not exhaustive semantic equivalence.
The existing documented ROM-observability limitation has not been repaired.
[Machine-readable validation](../../verification/source-layout-refactor.json) records
the changed paths, build hashes and test results. Full temporary build artifacts
are under `/tmp/phyb-structure-20261029/`.
