# ROM domain, load coverage and fixed code addresses

The actuation-pointer counterexample applies across the readable ROM image,
not only to its three initial probe addresses. For every non-null pointer P
whose copy completes, equal actuation output requires equality of the 960
source bytes at P. The full objective retains this input domain.

## Complete byte coverage for the selected ROM interval

The ROM interval before the initialized-RAM reload mapping is `[0, 0x88D2)`,
35026 bytes. A 960-byte copy cannot use pointer zero because the firmware
checks for NULL. The audit chooses 37 non-null windows covering every byte in
`[1, 0x88D2)`. Each window runs with instruction observation and with all hooks
removed: 74 comparisons. The copied output must equal the corresponding
pre-execution ROM bytes in each image, and both modes must agree on final
observations. Static differing-byte counts are checked against runtime outputs.
Byte zero is the sole uncovered byte and already matches directly.

```text
0                  1                 0x6B4C                0x88D2
| equal, unprobed   | code/vectors ... | readonly data bank  |
                   +------ 37 overlapping copy windows ----+
```

The audit's zero exit status without `--require-equivalent` means its coverage
and comparisons are internally consistent. With that option, any byte/output
divergence causes a nonzero exit. Physical ROM mapping and device behavior
remain assumptions; coverage is a property of the declared scalar model.

## ELF load coverage correction

The earlier linker script placed the RTKit build string and FRS messages before
lower-address channel sections. Their section contents were correct, but no
PT_LOAD segment covered them. The execution harness loaded zeros at 0x7D00
(28 bytes) and 0x7E48 (43 bytes). The old section-only native check did not
establish runtime coverage. The preserved old ELF and failing checks are
`before-load-order.elf`, `load-coverage-before.json.gz` and
`frs-native-before-load-check.json.gz` in `verification/evidence/`.

ROM output sections now follow address order. `verify_load_image.py` compares
every file-backed allocated section against the actual PT_LOAD byte map; it
also rejects conflicting load bytes and truncated segments. Native FRS checks
now require load coverage as well as section/instruction equality. NOBITS
initialization, cold/warm policy and physical loader behavior remain separate.

## Readonly bank restoration

The full 7558-byte interval `[0x6B4C, 0x88D2)` now matches the reference in the
loaded memory image. Existing typed pinned objects provide 150 bytes. Five
target-only data fragments provide the remaining 7408 bytes:

| Range, end exclusive | Bytes |
| --- | ---: |
| `0x6B4C..0x6B60` | 20 |
| `0x6B76..0x6BA4` | 46 |
| `0x6BDD..0x7D00` | 4387 |
| `0x7D1C..0x7E48` | 300 |
| `0x7E73..0x88D2` | 2655 |

`src/rtkit/rom_data.c` and its five includes preserve raw readonly bytes without
inventing semantic types. They do not replace executable text. The linker
rejects code extending into the original readonly bank and checks its final
address. The load checker separately compares the complete bank with the
SHA-256-verified reference. The ROM audit verifies that no bytes in its interval
are supplied solely by an unspecified ELF gap.

## Callback compatibility

An accepted NVV 0x080B record can replace the wakeup callback at 0x4D289900 with
0x41D, the original debug stub's Thumb address. The former rebuilt layout
executed an invalid instruction at 0x41C; the reference returned normally.
Hook-free execution corroborated the failure. The former ELF and 24-case report
are retained as `before-callback-pin.elf` and `callback-before-pin.json.gz`.

The existing C debug stub is now linked at its original 0x41C address. Its four
loaded bytes match the reference exactly; no parser filtering or pointer-value
translation was introduced. The callback matrix has 24 cases over NULL, the
fixed stub and a synthetic register-recording callback, four peripheral/register
seeds and both observer modes. The controlled callback records R0-R12, entry
SP/LR and xPSR. It occupies otherwise unused modeled ROM and does not overwrite
original firmware bytes. IRQ5 is called as a scalar routine; physical IRQ
delivery is not established by these cases.

## Reproduction and assumptions

```sh
/tmp/phyb-verify-venv/bin/python tools/verify_load_image.py \
  --elf /tmp/phyb-semantic-build/phyb.elf \
  --reference ../phyb --output /tmp/phyb-semantic-build/load-coverage.json
/tmp/phyb-verify-venv/bin/python tools/audit_rom_domain.py \
  --elf /tmp/phyb-semantic-build/phyb.elf \
  --output /tmp/phyb-semantic-build/rom-domain.json --require-equivalent
/tmp/phyb-verify-venv/bin/python tools/probe_clock_callback.py \
  --elf /tmp/phyb-semantic-build/phyb.elf --build /tmp/phyb-semantic-build \
  --output /tmp/phyb-semantic-build/clock-callback.json
```

| Assumption | Dependent result | Stress test or falsification probe |
| --- | --- | --- |
| The reference image and ELF inputs are intended and frozen | All checks | Full-file SHA-256 and unchanged-input checks |
| PT_LOAD contents define the compared initial byte map | Load coverage | Preserved two-section/71-byte failure; section/data and load bytes checked separately |
| ROM is readable and GTB=10 completes actuation without autonomous writes | ROM coverage and copy results | Every covered byte observed in both modes; physical mapping/progression remains unknown |
| The compared callback is entered through a scalar IRQ5 call | Callback cases | Real NVV installation, NULL and register-capture controls, fully unhooked invalid-instruction counterexample; actual exception delivery remains unverified |

Load-map construction is O(L) time/space for L loaded bytes. Section comparison
adds O(S) time for S file-backed allocated bytes. The ROM audit is O(R + C*(I+M+T))
time for R ROM bytes, C scalar comparisons, executed instructions I, compared
memory M and transfers T. Per-call instruction caps remain active.

Original firmware bytes, phyb.asm, phyb.c and retained ELF/report artifacts are
primary evidence. `verification/input_manifest.json` binds hashes; the complete
assumption register and remaining requirements are in `verification/ledger.json`.
Run `make verify-archive` to check all retained input/report/artifact hashes,
current ELF bindings, ledger report bindings and the declared SMT query archive.
Its success establishes archive consistency; the semantic gate result is
reported separately and remains false while recorded counterexamples persist.
The impact is high: fixed-address data, callback entry addresses and loadable
memory contents are separate fidelity requirements. Executable/vector byte
identity, other code-pointer domains, arbitrary aliases, device concurrency and
physical fault behavior remain open. No complete semantic-match claim follows.
