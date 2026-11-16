# Name Recovery — `phyb`

Supporting record of the evidence-backed naming pass for the `phyb` target.
See [README.md](README.md) and [PLAN.md](PLAN.md) for current status.

This pass uses the reconstructed C, the associated `phyb.asm` listing, and raw
9A348 PHYB bytes (SHA-256
`15f3ce1c30b82e3f87c45974284ccfa68e10928fea0f09a1a46376ac04cd50d4`).
Names describe observed operations; they are not claimed to be original symbols.
The original disassembly and `@provenance` names remain unchanged.

## Function names

| Previous C name | Canonical C name | ROM RVA | Evidence and purpose |
|---|---|---|---|
| `rtk_sched_take_switch_request` | `rtk_clear_context_targets` | `0x011A` | Stores zero through R0 and R1, returns R0; does not retrieve a request |
| `nvv_module_id_equals` | `nvv_module_prefix4_matches` | `0x58D4` | C predicate adapts `strncmp(...,4)`; full-string equality is not required |
| `xor_checksum` | `xor_checksum_words` | `0x4E88` | XOR reduction over 32-bit loads; count is in words, not bytes |
| `txpwr_get_channel_limit` | `txpwr_get_channel_limit_word` | `0x38DC` | Returns an entire packed four-byte table entry, not a scalar power limit |

The module-prefix helper has an important ABI distinction. The raw function
returns the signed `strncmp` result, where zero means match. The C model already
converted that into a Boolean result before this pass. Its descriptive native
alias is therefore `nvv_compare_module_prefix4`, while the C predicate is
`nvv_module_prefix4_matches`. Renaming does not repair or establish equivalence
between those return conventions. Prior native aliases remain for compatibility.

Previous C function spellings remain available as source-compatibility macros.
Callers, declarations, test harness lookups, native entry aliases, and native
source comments use the appropriate current names. No wrapper instructions or
new image bytes are introduced.

## Diagnostic response variables

At ROM `0x4DBC`, the response writes distinguish buffer selection, total source
length, requested length, and actual copied length. The following private names
now preserve that distinction:

| Previous | New name | Meaning |
|---|---|---|
| `index` | `buffer_index` | Diagnostic descriptor table selector |
| `offset` | `source_offset` | Byte displacement within the source buffer |
| `length` | `requested_length` | Caller-requested byte count |
| `capacity` | `response_capacity` | Available response-buffer size |
| `buffer_length` | `total_buffer_length` | Descriptor's total source byte length |
| `count` | `chunk_length` | Eight-bit length actually emitted/copied |
| `buf` | `descriptor` | Diagnostic buffer descriptor |
| `hdr` | `response_header` | Packed response header |

The checksum count is now `word_count`. The four-byte channel-limit getter's
local result is `packed_limit`. The context-clearing helper's arguments are
`save_target_out` and `restore_target_out`.

The existing packed widths, descriptor sampling order, alias behavior, return
sizes, and length-selection expressions are retained. This is not a bounds-check
change or a fix to the protocol's unusual length rules.

## Recovered TX-test field and mode meanings

The seventh argument of `txpwr_set_test_mode` is stored at RAM `0x4D289960`,
which is `struct txpwr_mode` offset `0x18` from base `0x4D289948`.
It is not an unused padding word:

| Native mode | Reader | Argument use | Descriptive mode name |
|---|---|---|---|
| 4 | `0x3E76` | Load into R0, then call calibrated-target path at `0x3E7A` | `TXPWR_MODE_TARGET_OVERRIDE` |
| 5 | `0x3F64` | Load into R1; R0 is the selected stage; call direct-index path | `TXPWR_MODE_INDEX_OVERRIDE` |

The state field, wire field, and C argument are named `override_value`.
Anonymous unions retain the old field spellings at the same offsets; enum
aliases retain the old mode constants. The wire argument is signed eight-bit,
whereas the state slot retains its extended 32-bit representation.

**Pre-existing fidelity gap:** the current C consumer halts for modes 4 and 5.
The native code instead applies the target or index override as shown above.
This pass identifies the field and updates names/comments but deliberately
retains the existing C execution behavior. It does not claim those modes are
semantically equivalent to the firmware.

## Stack-floor check boundary

At `0x6240`, the firmware executes `LDR R0,=0x4D289EC8; CMP SP,R0`, followed by
`BLT` to that branch itself. The comparison is not re-executed inside the loop.
The source model currently polls `cpu_get_msp()` in a C `while` loop. Selecting
MSP rather than active SP, recomputing the condition, and signed versus unsigned
comparison are unresolved model differences. The unused ABI arguments have not
been assigned speculative meanings.

## Assumptions and validation

- **A1:** the supplied SHA-256-bound 9A348 image is the intended reference.
  Probe: compare the raw identity and the listed RVA instruction sequences.
- **A2:** the selected dataflow is interpreted in the observed Thumb mode.
  Probe: instruction decoding, call-argument registers, and object offsets;
  alternative execution modes are not asserted.
- **A3:** names do not imply knowledge of undocumented hardware units.
  The TX field is named by its use as a target/index, not by a guessed physical
  unit. Runtime reachability of its modes is not established here.

ARM and host builds, 73 host checks, 22 pinned-state checks, and the provenance
inventory pass. The entire C target's 74,127 file-backed loaded bytes are
identical before and after the renaming pass. The native candidate remains
byte-identical to the original 74,240-byte file and passes its identity and
mutation-control checks. The diagnostic sampling-order and 2,100 alias cases
also pass. These results verify naming/layout preservation, not whole-firmware
semantic equivalence.

Temporary before/after builds and records were produced under
`/tmp/phyb-names-20261029/` (not retained); the durable result is the source
tree itself. Historical evidence and source-hash manifests have
not been rewritten to pretend they were generated from the renamed sources.

Bounded follow-up findings: the mode-4/5 C behavior mismatch has potentially
material impact if those modes are used; the stack-floor mismatch is conditional
on stack selection and failing entry state. Both are documented in code and
remain unfixed by this naming-only pass. Other reserved fields with insufficient
reader/writer evidence retain neutral names.
