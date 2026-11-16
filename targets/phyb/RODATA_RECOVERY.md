# Readonly-Bank Recovery — `phyb`

Supporting record of the `rom_rodata_*` range recovery for the `phyb` target.
See [README.md](README.md) and [PLAN.md](PLAN.md) for current status.

## What the `rom_rodata_*` ranges contain

The five former arrays were **not five original firmware structures**. They
were byte ranges inserted between already pinned objects to retain the original
readonly image. Their boundaries sometimes fall inside a related group of
tables. They have now been replaced by named, typed **storage aggregates** in
`src/rtkit/rom_data.c`. This does not claim that the original source declared
those aggregates as C structs.

Reference: 9A348 PHYB, SHA-256
`15f3ce1c30b82e3f87c45974284ccfa68e10928fea0f09a1a46376ac04cd50d4`.
Addresses below are hexadecimal; intervals are half-open; sizes are bytes.

## Range classification

| Former array / interval | Size | Recovered contents |
|---|---:|---|
| `rom_rodata_6b4c`, `[0x6B4C,0x6B60)` | 20 | Five 32-bit stack-bound/size constants |
| `rom_rodata_6b76`, `[0x6B76,0x6BA4)` | 46 | Two alignment bytes, nine exception-name addresses, eight band spans |
| `rom_rodata_6bdd`, `[0x6BDD,0x7D00)` | 4,387 | FRS/RX/TX lookups, an unclassified zero span, exception classification, stack paint pattern, formatter digits |
| `rom_rodata_7d1c`, `[0x7D1C,0x7E48)` | 300 | 16 strings: common formatting, exception names, MAC source path and IRQ messages |
| `rom_rodata_7e73`, `[0x7E73,0x88D2)` | 2,655 | 130 strings: FRS/DPD/TX diagnostics, SysCfg/NVV names, source paths, version text and runtime assertions |

Total covered by these five storage objects: `20 + 46 + 4387 + 300 + 2655 =
7408` bytes. Other pinned objects cover the product string, channel lists,
RTKit version string and native FRS messages. The complete readonly bank at
`[0x6B4C,0x88D2)` is 7,558 bytes.

## Stack constants and exception-name pointers

At `0x6B4C`, the five words are:

| Address | Value | Role |
|---|---|---|
| `0x6B4C` | `0x4D289EC8` | PSP top |
| `0x6B50` | `0x400` | PSP size |
| `0x6B54` | `0x4D289EC8` | MSP base |
| `0x6B58` | `0x4D28A2C8` | MSP top |
| `0x6B5C` | `0x400` | MSP size |

The reset stack initializer at `0x5B50` consumes these constants. The preceding
word at `0x6B48` is the PSP base, `0x4D289AC8`, and lies outside the former raw
fragment. This is a tail of the stack-constant group, not a complete new header.

The table at `0x6B78` contains nine encoded 32-bit addresses pointing to
`RESET`, `UNDEF`, `SVC`, `PREFETCH_ABORT`, `DATA_ABORT`, `IRQ`, `FIQ`, `NMI`, and
`HARDFAULT`. The exception bridge at `0x220` indexes it by exception class.
Some entries were displayed as short printable strings in the old assembly;
following their targets and the indexed loads identifies them as addresses.
The typed storage uses `uint32_t` addresses, preserving the firmware encoding
independently of the host's pointer width.

The bytes at `0x6B9C` are the per-band channel spans used by result processing
at `0x11B4` and reading dumps at `0x1430`: `79,101,126,76,126,125,125,125`.

## Lookup and constant inventory

| Interval | Representation / role | Evidence |
|---|---|---|
| `[0x6BDD,0x6BE5)` | Eight default channel counts | Multipass scan at `0x1120`; C `frs_default_channel_counts` |
| `[0x6BE5,0x6BE8)` | Zero bytes before aligned pointer table | Physical alignment; semantic role inferred |
| `[0x6BE8,0x6BF8)` | Four addresses: `inv`, `clp`, `pll`, `agc` | RSSI dump at `0x1430` |
| `[0x6BF8,0x6C38)` | Eight bands x two packed max-gain words | FRS entry/configuration at `0x15E0`; exact fallback values in `frs.c` |
| `[0x6C38,0x6C45)` | Thirteen forced-gain indices | Gain-index selection in FRS entry |
| `[0x6C45,0x6C46)` | Zero byte before halfword table | Physical alignment; semantic role inferred |
| `[0x6C46,0x6C60)` | Thirteen 16-bit gain-control codes | Corresponding halfword selection in FRS entry |
| `[0x6C60,0x6C68)` | Four 16-bit sample/status encodings | Result processing: `0x7FFF,0x7FFE,0x7FFD,0x7FFC` |
| `[0x6C68,0x6C7D)` | Physical storage shared by RX bandwidth views | RX setup indexed loads use literal pointers at `0x1F28/0x1F2C` |
| `[0x6C7D,0x6C85)` | Eight per-band max-gain offsets | RX gain update uses `dword_6C74+9`, literal at `0x2698` |
| `[0x6C85,0x6C8D)` | Eight alternate-format TX power types | TX command setup at `0x4064`, literal at `0x40D4` |
| `[0x6C8D,0x6C9D)` | Sixteen packet-type TX power types | TX command setup at `0x4064`, literal at `0x40D8` |
| `[0x6C9D,0x7CA0)` | **4,099 zero bytes; purpose unknown** | Raw bytes verified; no specific static consumer identified |
| `[0x7CA0,0x7CD8)` | Fourteen 32-bit exception-class entries | Fault capture at `0x6314`: bounds check, classification mask and `LDR [...,index,LSL#2]` |
| `[0x7CD8,0x7CE0)` | 64-bit `RTKSTACK` stack-paint pattern | Reset stack painting at `0x5B50` |
| `[0x7CE0,0x7CF0)` | Lowercase hexadecimal digits | Formatter core at `0x6444`, literal at `0x693C` |
| `[0x7CF0,0x7D00)` | Uppercase hexadecimal digits | Formatter core at `0x6444`, literal at `0x6940` |

The hexadecimal alphabets are 16-element tables **without NUL terminators**.
They are represented as character arrays, not C strings.

### Overlapping RX views

The RX alternate-rate table starts at `0x6C68`; the packet-type table starts
seven bytes later at `0x6C6F`. The existing C model's eight- and sixteen-byte
views match the raw bytes, including shared trailing zeros:

```text
0x6C68          0x6C6F                    0x6C7D
   alternate-rate view (8 bytes)
                  packet-type view (16 bytes)
                                            band-offset table (8 bytes)
```

The packet-type view extends two bytes into the first zeros of the band-offset
table. This could result from compiler constant merging or shorter original
arrays whose neighboring bytes are also read. The original declarations are
unknown. The recovered storage therefore uses a byte span rather than claiming
independent, nonoverlapping arrays of those logical lengths.

### Unclassified zero span

`[0x6C9D,0x7CA0)` is retained as `unclassified_zero_fill[4099]`. It can be
partitioned arithmetically into three bytes to a four-byte boundary plus 4,096
bytes, but that is not evidence for a page table, buffer, linker alignment
policy, or reserved firmware object. None of those names is assigned.

No dedicated consumer has been identified from the available static references.
Generic ROM reads through supplied pointers can still observe the bytes, so
absence of a named reference is not evidence that the span is unobservable or
can be omitted.

## Readable strings

The two string banks are defined in:

- `src/rtkit/rom_exception_strings.def`: 16 strings, 300 bytes.
- `src/rtkit/rom_diagnostic_strings.def`: 130 strings, 2,655 bytes.

Each entry gives a descriptive field name, absolute ROM address, and the exact
escaped literal. X-macro expansion creates the fixed-size character field,
initializer, and compile-time offset check from that single entry. This retains
all original text, including source build roots, spelling errors and newlines.
It does not redirect the reconstructed algorithms to new strings or alter
source-path reporting behavior.

Names identify content and observed use; they do not claim original debug
symbols. Legacy raw `.inc` files are archived under
`verification/fixtures/rom-rodata/`; they are not active source files. The
verification manifest maps their historical source paths to that location,
preserving all original fixture hashes and archived report contents. Existing linker section bindings are retained.

## Assumptions, probes and validation

| ID | Assumption / uncertainty | Probe or falsifier |
|---|---|---|
| A1 | The SHA-256-bound 9A348 image is the reference | Hash and complete loaded readonly-bank comparison |
| A2 | Indexed references identify table element widths and consumers | Inspect the cited literal loads, index scaling and raw values; alternate-mode execution could expand interpretation |
| A3 | Zero bytes between typed groups are alignment storage | Their bytes/positions are certain; original compiler/linker declarations remain unknown |
| A4 | RX consumer views do not establish original array extents | Compare all view bytes; original object metadata or source would resolve the overlapping declarations |
| A5 | Descriptive aggregates model physical layout | Every field/string offset and total size is asserted; they are explicitly not claimed as original structs |

ARM/host builds, 73 host checks, state layout and provenance pass. The complete
7,558-byte readonly bank matches the raw firmware. A controlled relink with the
same objects except old versus typed `rom_data.o` produces identical file-backed
loaded bytes and runtime mapping metadata. This isolates a concurrent change
to the libc shift helpers observed during the initial full builds. It does not
attribute that unrelated edit to this work or undo it.

See [the object inventory](../../verification/rom-object-inventory.json) and
[controlled layout verification](../../verification/rom-object-layout-check.json).
No hardware trace, exhaustive pointer analysis or original debug-type metadata
was available. Such evidence is not required for the byte-layout conclusions,
but would be required to assign a stronger purpose to the zero span.

Bounded additional findings: the apparent strings at the start of the exception
pointer table are actually addresses (medium analysis impact); RX constant
views overlap (medium impact for future type recovery); unreferenced zeros can
still be observable ROM data (medium impact for preservation). The original
struct declarations and the intended zero-span purpose remain unknown.
