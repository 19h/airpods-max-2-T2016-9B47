# Reconstruction Plan — `phyb`

Approach, constraints and acceptance criteria for the Apple W3 Bluetooth/Wireless
PHY coprocessor (`185{T:w3}{C:B0}.debug` on `RTKit_debug-3514.0.15.debug`).

## Target & status

| Item | Value |
|---|---|
| Raw image | `../../../phyb` (`74,240` B, SHA-256 `15f3ce1c...d50d4`) |
| IDA inventory | `232` functions (`219` in `phyb.c` + `13` stubs); `30` outlined helpers |
| Current ledger | `219` recovered, `197` claims, `0` unclaimed non-outlined, `22` pinned state blocks |
| Build | root `Makefile`: `make arm host check verify` |
| Semantic gate | **fails** on the documented ROM-observability counterexample |
| Deliverable | C reconstruction only; retained instruction bodies are recorded debt |

The `phyb` source tree remains at the workspace root (`src/`, `include/`,
`link/`, `tests/`, `tools/`) because it predates the multi-target layout. The
paths below are workspace-relative.

This document records how the C source relates to the two artefacts it was
derived from (`phyb.asm`, the IDA disassembly, and `phyb.c`, the Hex-Rays
listing) and what must hold for a change to be acceptable. The raw `../phyb`
image and its instruction semantics are authoritative. `CONSIDERATIONS.md` is
analysis, and its tables and prose must be checked against the bytes when it
disagrees with either listing.

## 1. Goal

The current tree follows [SOURCE_LAYOUT.md](SOURCE_LAYOUT.md): W3 application
code is under `src/w3/`, with RTKit threading and fault handling separated.

Source that reads like the firmware's own source would have, while remaining
behaviourally identical to the image:

* one translation unit per subsystem, named after the firmware's own file
  names where they survive in the image (`mac.c`, `exception.c`);
* hardware described once, in register/field headers (`include/btp/regs_*.h`),
  and driven through small read-modify-write helpers;
* memory shared with the host described by structs (`include/btp/shared.h`);
* module state in named structs (`struct rx_state rx`, …) rather than
  address-named globals;
* enums and constants for every protocol value the code dispatches on
  (`enum mac_subcommand`, `enum nvv_tag`, `enum trace_site`, …);
* the firmware's vocabulary preserved: every debug string, assertion text and
  calibration tag name is kept verbatim.

Realism never overrides fidelity. Where the original does something odd (the
always-zero "= %d" column in the NVV log line, a getter that reads a whole
32-bit entry, a status byte assumed negative) the source does the same and
says so in a comment.

## 2. Fidelity rules

| Rule | Where enforced |
|------|----------------|
| Every recovered function is claimed by exactly one `@provenance RVA 0x… <name>` tag with the matching RVA. Machine-outlined helpers (`*_outlined_*`) are inlined and exempt. | `tools/verify_fidelity.py` (`make verify-fidelity`) |
| No decompiler idioms (`HIWORD`, `BYTE1`, `__OFSUB__`, `v1`/`a1` names, `goto LABEL_`) in `src/`. | `tools/verify_fidelity.py` |
| Register accesses keep their widths and the order of consecutive read-modify-write stores. | review against the disassembly (`phyb.i64`) |
| Trace events keep type/subtype/args and the recovered site tokens (`enum trace_site`). | review; `tests/test_main.c` |
| Shared-memory structs are byte-exact and packed. | `tests/layout.c` (`make check`) |
| Module state blocks have the recovered size and field offsets… | `tests/layout.c` |
| …and are linked at the recovered addresses. | `link/phyb.ld`, `tools/verify_layout.py` (`make verify-layout`) |
| Assertions print the original line numbers and file names. | `btp_panic_at()` in `mac.c`, `btp_report_exception()` in `exception.c` |
| The freestanding C library keeps its contracts (`snprintf` truncation and return value, `strl*`, overlap-safe `memcpy`). | `tests/test_main.c` |

## 3. What is reconstructed, what is modelled

* **Code inventory**: 219 recovered functions, 197 source provenance claims,
  and 30 outlined helpers exempt from mandatory claims. Some outlined helpers
  also have explicit claims. Inventory coverage does not prove semantic match.
* **ROM tables**: `static const` arrays with the bytes read from the image
  (FRS band spans, channel lists, gain codes, status names, packet-type maps,
  exception class table and names).
  The complete fixed-address readonly bank is retained as typed pinned objects
  plus typed ROM storage aggregates; its loaded bytes are compared against the image.
* **`.data`**: initialised globals with the bytes recovered from the dump (PA
  stage tables, RF sequence container, radio-rev-2 fallbacks, RTKit metadata
  records, pointer cells). These objects and the initialized shared RAM prefix
  retain their original addresses and bytes. FRS channel-list targets retain
  their original ROM addresses. Reload source mapping is specified in the linker;
  physical loader behavior and mutable pointer-cell consumers remain to audit.
* **`.bss`**: twenty-two state blocks pinned by the linker script to the
  addresses in `include/btp/memmap.h`; the stacks and the gap between them are
  reserved.
* **Exception entry**: target reset/fault/SVC entry uses stackless ARM veneers.
  Fault decoding remains C around `struct rtk_exception_frame`. Host entry
  helpers are test adapters and do not prove architectural register capture.
* **Host build**: the same sources compile natively with `-DBTP_HOST`.
  Peripherals and shared memory resolve through a sparse bus model
  (`tests/host_bus.c`); module state is ordinary data. This is a test harness,
  not a model of the device.

## 4. Known deviations

1. Selected target `libc`/arithmetic bodies retain original instructions or
   decoded call relocations; their host adapters implement ordinary-memory
   behavior. These are C-recovery debt. The two 64-bit shift helpers now share
   defined C implementations on host and target, with complete-domain numerical
   checks under the declared ARM/ABI model. Formatter and remaining library
   domains have conditional coverage.
2. Initialized RAM, pointer cells, header, guard and metadata retain their
   original addresses/bytes. Executable ROM does not: accepted NVV pointers
   make that difference visible through DPD actuation. The current semantic
   gate fails on this confirmed counterexample (see `verification/rom-observability.md`).
   The full readonly bank and debug stub's fixed entry are now preserved, but
   these do not restore all executable/vector bytes. File-backed sections must
   also be covered by PT_LOAD; correct section contents alone are insufficient.
3. DPD logic that the compiler inlined into `tx_setup_isr` lives in
   `dpd.c` (`dpd_on_tx_setup()`); the call order is unchanged.
4. Pointer cells to the shared regions (`mac_mbox`, `trace_ring`,
   `crash_record`, `phy_status`) are initialised data on the target and are
   re-pointed into the bus model on the host.
5. Initial AGC selector 0 consumes preexisting stack words; later selector 0
   reuses table state. The target scan now retains the native instructions,
   frame and original 0x0D7C address; eight scalar and seven seeded QEMU cases
   match. The host C adapter supplies explicit initial values and retains table
   state across channels. It does not model the native entry stack. Physical
   exception delivery and remaining diagnostic/exception stack aliases still
   require complete audit.

## 5. Verification

```
make all BUILD=/tmp/phyb-semantic-build      # target ELF + host tests, -Werror
make check BUILD=/tmp/phyb-semantic-build    # layout + behavioral checks
make verify BUILD=/tmp/phyb-semantic-build   # linked RAM map + inventory
make verify-semantics BUILD=/tmp/phyb-semantic-build  # dependencies: README
```

`verify-fidelity` checks provenance comments, not behavioral equivalence.
`verify-semantics` compares reset/exception entry, initialized RAM and RTKit wire
bytes, selected exact library instruction bytes, DSRL=0/1 modeled boot, TX/RX setup matrices,
leaf APIs, NVV, power interpolation, loopback, FRS, DPD and libc cases against the raw firmware. It is not an exhaustive
proof. `verification/ledger.json` records the full objective and unresolved
functions/paths; that objective remains active until every requirement has
sufficient evidence.

`verify-symbolic` adds conditional complete-domain scalar checks with path
coverage, MMIO event guards, SMT-LIB export/replay and deliberate failure
controls. It does not establish whole-firmware equivalence or the invariance of
initialized pointer cells; NVV records can modify those cells.

### C reconstruction requirement

The user explicitly selected C reconstruction as the deliverable. A complete
native reassembly is not an alternative implementation or evidence of C
correctness. The separate reassembly candidate has been removed from the source
and build targets; its files were preserved outside this project as reference.
Existing retained-instruction function bodies remain C-reconstruction work.
They must not be counted as completed C implementations merely because their
machine bytes match. Processor entry/ABI effects and compiler intrinsics require
explicit treatment in the acceptance ledger.

## 6. Working on the source

1. Hardware: add registers/fields to the relevant `regs_*.h`; name fields by
   function when the firmware's use makes it clear, by position otherwise, and
   say which.
2. State: add fields to the module's state struct and extend `tests/layout.c`;
   the struct size must still equal the gap to the next block.
3. New or split functions keep their `@provenance` tags; `make verify-fidelity`
   must stay green.
4. Protocol values go into the module header's enums, never inline.
