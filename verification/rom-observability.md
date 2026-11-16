# Confirmed fixed-address ROM divergence

The current C/assembly reconstruction does not preserve executable bytes at
their reference addresses. Accepted NVV input makes this externally observable
without executing those bytes through a substituted function pointer.

## Reproduction

```sh
make verify-rom-observability BUILD=/tmp/phyb-semantic-build \
  SEMANTIC_PYTHON=/tmp/phyb-verify-venv/bin/python
```

The target exits nonzero while any tested reference/reconstruction divergence
remains. To reproduce against the retained ELF without rebuilding:

```sh
/tmp/phyb-verify-venv/bin/python tools/probe_rom_observability.py \
  --elf verification/evidence/phyb.elf \
  --output /tmp/rom-observability.json --require-equivalent
```

The Python, Unicorn, pyelftools and Capstone dependencies are the same as the
existing semantic harness. The default probe mode, without
`--require-equivalent`, validates execution, source/output correspondence, RAM
controls and observer consistency; its zero exit status does not assert
reference equivalence.

## Accepted input and observation

The 20-byte NVV blob for the first counterexample is:

```text
0c 00 56 4e 00 00 00 00 0b c8 08 00 94 aa 28 4d 08 01 00 00
```

Its container holds a long-form `0x080B` register-write record. The destination
is the RAM cell `0x4D28AA94`; the stored table pointer is `0x00000108`.
Both executions call their actual NVV parser and verify that the cell acquired
the requested pointer. The TX fixture enables actuation, sets its trigger to
1, supplies GTB=10, disables test/capture requests, and initializes ordinary TX
calibration fixtures identically. TX setup reads the installed pointer and
copies `0x3C0 = 960 = 240 * 4` bytes to `0x4D148800`.

```text
NVV record -> RAM[0x4D28AA94] = 0x108
                         |
                         v
TX setup -> read ROM[0x108 .. 0x4C7] -> aperture[0x4D148800 .. 0x4D148BBF]
```

| Input table address | Differing output bytes | First differing output address |
| --- | ---: | --- |
| `0x00000108` | 906 / 960 | `0x4D148800` |
| `0x0000010C` | 906 / 960 | `0x4D148800` |
| `0x00000200` | 910 / 960 | `0x4D148800` |
| `0x20005000` (RAM control) | 0 / 960 | none |

Each row runs twice: with an executed-instruction observer and with all
Unicorn hooks removed. The hook-free runs use no changing-read plan or ACK
callback. The observed run verifies exactly 240 consecutive 32-bit actuation
writes. Both modes produce identical final observations per image. The full
960-byte output in each run also equals the corresponding pre-execution source
bytes, checked independently of the transaction logger.

For input `0x108`, the first 16 output bytes are:

| Image | Bytes at aperture `0x4D148800` |
| --- | --- |
| Reference | `80 b5 6f 46 06 f0 09 fd 80 b5 6f 46 04 20 ff f7` |
| Reconstructed | `02 48 01 68 41 f0 01 01 01 60 70 47 00 90 15 4d` |

## Requirement implied by the counterexample

For every readable accepted pointer P whose actuation copy completes, matching
the observed output requires matching all 960 source bytes at P. Pinning RAM
objects and selected ROM strings does not meet this requirement. Excluding
executable-ROM pointers from verification would restrict the accepted firmware
input domain. Restricting the reconstructed parser would itself diverge from
the reference parser. No such restriction has been applied.

The native FRS scan now retains its original address and referenced message
bytes. Executable-ROM compatibility across the whole image remains unresolved.
Preserving
fixed-address ROM contents, data accesses and execution requires an architectural
solution; special-casing the three tested pointers is not a universal fix.
The original C/assembly reconstruction remains the implementation under test.

## Assumptions, falsification probes and provenance

| Assumption | Dependent conclusion | Falsification probe |
| --- | --- | --- |
| `../phyb` is the intended reference | All comparisons | Check SHA-256 before execution; recorded in report |
| The scalar Unicorn ARMv7E-M model executes these paths correctly | Dynamic execution and ordering | Hooks removed; compare final states and raw source/output bytes; independent CPU execution remains outstanding |
| GTB=10; no device/interrupt intervention | This finite completed workflow | Changing progress, autonomous writes and interrupt interleavings remain separate domains |
| ROM is data-readable at its loaded addresses | Physical reachability of this input | Physical loader/core memory attributes are unknown; inspect them on the target |

The executable images and local instruction listings are primary evidence:
`phyb.asm` contains the `0x080B` dispatch at `def_5168`, register writes at
`loc_569C`, and the actuation pointer load/copy at `0x3114..0x3156`.
`src/w3/nvv.c:nvv_validate_address` and `nvv_apply_reg_writes` accept the RAM cell;
`src/w3/dpd.c:dpd_run_actuation` follows it. `phyb.c` describes the corresponding
decompiled paths. `verification/input_manifest.json` records reference and
current ELF hashes; `verification/evidence/rom-observability.json.gz` retains
all input blobs and output bytes. The probe is linear in executed instructions,
observed transfers and snapshot size; it inherits the harness's explicit
200000-instruction limit per scalar call.

The impact is high for complete semantic equivalence: data-visible executable
layout is an additional observable domain, independent of ordinary arithmetic
and MMIO ordering. Physical reachability remains conditional on the ROM mapping;
the modeled divergence is confirmed. No assertion of complete equivalence follows
from the earlier passing finite or conditional symbolic checks.
