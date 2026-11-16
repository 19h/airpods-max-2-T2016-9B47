# FRS scan and calibration verification

Four scan access-order differences were corrected in `src/w3/frs.c`:

| Reference instructions | Required behavior |
| --- | --- |
| `0x0DD0..0x0DDE` | Clear the channel latch, then reread the register before setting it |
| `0x0DF4`, `0x0DF6` | Sample PLL failure bit 18 from the first load and bit 17 from the second |
| `0x0ED2`, `0x0ED6` | Read the selected gain-table word before reading the gain-force register |
| `0x0FAC..0x0FCA` | Read center-bin flags, then reread the center value when that bin is enabled |

The former ELF diverged in 2928 of 3456 scan/calibration/workflow comparisons.
The corrected executable matches that finite matrix. The old ELF and full
comparison report remain in `verification/evidence/frs-before-scan-fix.*`.

## Matrices and limits

| Matrix | Cases | Scope |
| --- | ---: | --- |
| Scan combinations | 2880 | Eight bands; three offsets and half-spans; center/single/forced flags; normal, PLL failure, AGC timeout, correlator timeout and clipping |
| Changing responses | 144 | All bands; raw single flags 0/2/255; AGC selectors 1/2/3; timer steps 1/1000 |
| Calibration | 48 | All bands; one/two/three passes; absent/present signed trim; sample/gain boundary patterns |
| Entry/scan/process/exit | 384 | All bands and pass counts; custom/default channel lists; center/single/forced combinations |
| Independent QEMU calibration | 224 | Valid and invalid band/pass inputs; trims, spans and signed sample extremes |
| Independent QEMU scans | 1296 | Two radio-bank families; offsets/spans; flags, selectors 1/2/3, PLL failure and clipping |

The Unicorn matrix records complete ordered peripheral accesses, returns,
shared RAM and scratch memory. Equal memory is compared byte for byte before
digest compaction. Each scalar call has a 200000-instruction cap. QEMU uses no
observer hooks and compares returns, FRS state, scratch, radio/RX/sequencer RAM
and clock state; snapshot capacity is compile-time checked. The original ROM is
unmodified in both tools.

QEMU scan fixtures set SysTick reload to zero, so delays clamp to zero. They
supply settled AGC and an idle correlator; SCS virtual addresses map to Normal
RAM. These fixtures establish scalar final-state agreement for the stated
inputs, not target timing or autonomous hardware behavior.

## Native selector-zero correction

The former C helper reset its gain-table defaults on every channel. The native
selector-zero branch instead reads stack slots at SP+8/SP+12 on the first
channel and reuses values stored there by selectors 1..3 on later channels.
The prior ELF failed eight Unicorn cases and six of seven seeded QEMU cases.
These historical reports and executable images remain in
`verification/evidence/frs-selector-zero-before-native*` and
`frs-before-native-stack.elf`.

The target now retains the original Thumb scan instructions and its 112-byte
frame in `src/w3/frs_scan_arm.inc`. It is linked at the original address 0x0D7C;
the jump table, literal pool and three referenced message strings retain their
bytes and addresses. Three small outlined helpers retain their register
contracts; the elapsed-time helper includes the original shared arithmetic
core. Host C remains an ordinary-memory adapter with explicit initial table
values and state retained across channels. It does not model an ARM entry stack.

```text
entry stack   -> first selector 0 reads SP+8/SP+12
selector 1/2/3 -> updates those slots -> later selector 0 reuses them
failed load   -> PC 0x0ED2, address 0xFFFFFF4C for the zero-stack fixture
```

`tools/verify_frs_native.py` verifies the 744-byte scan body, all 18 external
BL targets, 60 helper bytes and 43 message bytes. Apart from those decoded call
relocations and the helper's checked branch to its shared arithmetic core,
bytes match the reference exactly. Deliberate changes to the first stack-slot
load, a call target and the selector-zero jump-table entry are all rejected.
The scan's fixed address also preserves its absolute failed-instruction PC.

The eight selector-zero cases now match: six changing-selector sequences return
with matching observations; two zero-stack fixtures produce the same terminal
unmapped read. For the fault cases the checker compares the actual failed
address/width/kind, absolute PC, all general registers and xPSR, the active
112-byte frame, and pre-fault returns/accesses/RAM. Both executions must raise
an actual Unicorn memory error; unexpected faults in ordinary cases still fail.
This checks execution through the failed access. M-profile exception delivery,
SCB fault-status updates, vector handling and physical fault behavior are not
executed or proved by this scalar fixture.

The independent QEMU diagnostic explicitly seeds the original slot locations
before each call. The slots are at entry-SP minus 104 and 100 bytes. Six cases
use bases 64/80 with table addresses 0x4D150500/0x4D150600/0x4D150700; a seventh
uses base 0/table 0x4D150500. All seven now match without observer hooks. The
comparison fixture binds its message data to the unchanged reference ROM: a
non-loadable ELF segment supplies symbol addresses, and byte comparisons check
those 43 bytes. Loadable segments are checked against unintended reference-ROM
overlap. No reference instructions, literals or strings are patched.

```sh
/tmp/phyb-verify-venv/bin/python tools/verify_frs_native.py \
  --elf /tmp/phyb-semantic-build/phyb.elf \
  --output /tmp/phyb-semantic-build/frs-native.json --mutation-controls
/tmp/phyb-verify-venv/bin/python tools/probe_frs_scan.py \
  --elf /tmp/phyb-semantic-build/phyb.elf \
  --output /tmp/phyb-semantic-build/frs-selector-zero.json --selector-zero
/tmp/phyb-verify-venv/bin/python tools/qemu_frs_process.py \
  --build /tmp/phyb-semantic-build --selector-zero
```

Hardware reachability of selector zero remains unknown; no hardware exclusion
is assumed. The semantic domain gate still runs these cases and the independent
ROM-as-data probe. A passing native-layout or selector-zero check does not
establish whole-image ROM compatibility.

## Assumption register and remaining requirements

| Assumption | Dependent result | Stress test or falsification probe |
| --- | --- | --- |
| Frozen reference/rebuilt images are the intended inputs | Every comparison | SHA-256 checks and retained ELF/object hashes |
| The supported instruction observer models executed transfers | Unicorn traces | Changing register values; known conditional-transfer oracle; independent hook-free QEMU final states |
| Supplied polling values describe the tested environment | Completed scan/workflow cases | Timeout and changing-response profiles; real hardware progression remains unverified |
| QEMU Normal RAM and zero-reload delays are the declared scalar model | QEMU positive and negative cases | Invalid-input boundaries, seeded-stack control and exact executed-case counts; target memory attributes/timing remain unknown |
| Explicit stack and memory-fault fixtures describe the compared state | Selector-zero cases | Zero-stack absolute-PC/register/frame comparisons, seeded hook-free QEMU, changing selectors and mutation controls; all other initial stack values and physical exception delivery remain open |

Arbitrary output/state aliases, all preexisting-stack values, unexpected AGC
states, asynchronous aperture changes, interrupt interleavings and full target
fault handling remain open. ROM-as-data compatibility remains independently
refuted in `verification/rom-observability.md`. These requirements are retained
in the full objective.

The primary sources are `../phyb`, `phyb.asm`, and the retained executable
artifacts. `phyb.c` supplies decompiled control-flow context. Hashes and the
current assumption register are in `verification/input_manifest.json` and
`verification/ledger.json`. Per-case work is O(I + M + T) time and O(M + T)
space for instructions, compared memory bytes and captured transfers; no finite
matrix establishes universal equivalence.
