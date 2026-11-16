# `rkof` — Architectural & Instruction-Level Considerations

This document records hardware, ABI, and binary-level subtleties discovered while reconstructing and differentially verifying `rkof` against `../../../rkof`.

---

## 1. Boot Remap (`rtkit_entry` @ `0x000108`) & `DSRL = 0` Reload Semantics

- Vector slot 0 (`0x00000000`) is `0x00000000` (`Reset_Handler` initializes `MSP` from `*0x001C2EE0 = 0x200D46B0` and `PSP` from `*0x001C2ED8 = 0x200411D8`), and vector slot 1 (`0x00000004`) points to **`0x000001F9`** (`Reset_Handler`), matching `src/rtkit/vectors.c`. Meanwhile, **`0x00000108`** (`rtkit_entry`), situated immediately after the 72-byte RTKit `uuid` v5 header (`0x000000C0..0x00000107`), is the pre-remap entry point entered by the stage loader (`rkos`) before tail-branching to `Reset_Handler` (`0x1F8`).
- When `(MMIO32(0x4000C080) & 2) == 0`, `rtkit_entry` clears bit `16` of `0x4000C02C`, programs three data-remap registers (`RMD1..3` -> `0x4000C0B0..B8`) and three code-remap registers (`RMC1..3` -> `0x4000C0A0..A8`) from the embedded FourCC TLVs at `0x001E1ACC..0x001E1B08` (polling `0x4000C12C == 1` after each write), writes `RMFI` (`0x4000C120`), re-enables bit `16` of `0x4000C02C`, writes `RTSI` to **`0x008000F0`** (`0x00800000 + 0xF0`), and branches to `Reset_Handler` (`0x1F8`).
- In `rtkit_start` (`0x16D60`), `RAM_BSS` (`0x2000F138..0x200D46B8`) is zeroed in **three non-overlapping spans** so the live `PSP` (`0x200409D8..0x200411D8`, `2 KB`) and `MSP` (`0x200D36B0..0x200D46B0`, `4 KB`) stacks already poisoned by `Reset_Handler` are preserved!
- The `DSRL` TLV (`0x001E1B5C`) has payload byte `0x00`: `rkos` pre-loads `RAM_DATA` (`0x20000000..0x2000F138`) directly from file offset `0x200000` before releasing `rkof`, so `rtkit_start` skips copying from `ROM_RODATA_END` (`0x001F389A`, which is zero-padded in the raw file).

---

## 2. Exact Vector-Table Byte Parity via Pinned Veneer VMAs

Rather than emitting `.vectors` as an opaque `.incbin`, `src/rtkit/vectors.c` defines a typed 48-entry C array (`g_rkof_vectors[48]`). By pinning `rtkit_entry` (`0x108`), `Reset_Handler` (`0x1F8`), `Fault_Handler` (`0x290`), `NMI_Handler` (`0x2E0`), `SVCall_Handler` (`0x374`), and `Default_Handler` (`0x16F70`) at their exact original VMAs in `link/rkof.ld` and `src/rtkit/entry_veneers.S`, `ld.lld` resolves all 48 entries to **100% byte-for-byte SHA-256 identity** with `../../../rkof[0x00..0xBF]`.

All five `retained_native` naked veneers (`458` total bytes) are also verified byte-for-byte against `../../../rkof` by `tools/verify_load_image.py` (`442/442` non-relocated instruction and literal-pool bytes exact, including `Reset_Handler`'s cold-boot guard `0xBAADF1AC` at `0x20000190`, warm-resume magic `0xCAFE4B0B` -> `0x20000194`, `MSP`/`PSP` 8-byte poison loops, and `Default_Handler`'s pre-indexed `ldr r1, [r0, #0x28]!` dispatch; only `16` bytes across 4 linker `bl`/`b.w` or inlined MachineOutliner `sub_1B53FE` sites differ).

---

## 3. Subtle Instruction-Level Quirks Verified by Unicorn (`tools/arm_semantics.py`)

1. **`memcmp` (`0x00CA8C`) Return-Value Asymmetry**:
   - On unaligned leading bytes or trailing `< 4` bytes, `memcmp` returns `(int)lhs_byte - (int)rhs_byte`.
   - Once `lhs` is 4-byte aligned and `len >= 4`, `memcmp` loads little-endian 32-bit words (`ldr` / byte-pack when `rhs` is unaligned), byte-swaps both via `rev`, and returns **`-1` or `+1`** (`(bel < ber) ? -1 : 1`).
2. **`sub_17F5C` (`0x017F5C`) `uxth` Mask Quirk**:
   - Mask `2` is checked after `uxth r1, r0` (`(uint16_t)coproc_mask == 2u`, so `0x00010002` matches!), whereas masks `4, 0x80, 0x10, 0x20, 0x40, 8` compare the full 32-bit `coproc_mask`.
3. **`sub_FDDF8` (`0x0FDDF8`) Consecutive Same-Register RMW Coalescing**:
   - When replaying `(reg_addr, clear_mask, set_bits)` triples into W3 MMIO, `sub_FDDF8` defers calling `sub_FDE48` (`write_warm_restore` into `0x4D291008[slpr_reg_num++]`) until `reg_addr` changes or the loop finishes, coalescing back-to-back bitfield edits to the same modem register into a single warm-restore slot.
4. **`sub_FDCB4` (`0x0FDCB4`) Overflow Early-Out Power Domain Asymmetry**:
   - Both `sub_FDC38` (`'BPHY'`) and `sub_FDCB4` (`'MgrC'`) wake power domain `0x6E` to state `2` (`sub_FDD48(2)`) if `saved_pwr != 2` before reading `0x4D289000..0x4D289298`. However, if `max_bytes < detail1_len + 8`, `sub_FDCB4` calls `sub_1887C4()` (setting coredump overflow byte `+0x5D = 1`) and returns `0` **without** restoring `saved_pwr`.
5. **`sub_FE59E` (`0x0FE59E`) Ring Layout & `0xBC` Endianness Switch**:
   - `struct rkof_byte_ring` places `size` at `+0x04`, `wr` at `+0x08`, `rd` at `+0x0C`, and `buf` at **`+0x10`** (`0x14` bytes total on 32-bit ARM).
   - When `buf[(rd + 1) % size] == 0xBC`, bytes `+2` and `+3` are assembled **big-endian**; for all other tags, bytes `+2` and `+3` are assembled **little-endian**.
6. **Hard-Float VFPv4 ABI (`-mfloat-abi=hard -mfpu=fpv4-sp-d16`)**:
   - `rkof` passes `float` in `s0` and `double` in `d0` even in compiler-rt helpers (`sub_D610`, `sub_D680`, `sub_D6C2`, `sub_D704`, `sub_D736`, `sub_D7BC`), while 64-bit integers stay in `r0:r1`. Both compilation flags and Unicorn VFP register checks (`UC_ARM_REG_S0`, `UC_ARM_REG_D0`, `CPACR = 0x00F00000`) match this exact calling convention.
7. **Spicy Logger `va_list`-by-Value Vtable Dispatch (`spicy_log_emit` @ `0x0C7AB4`, `spicy_log_emit_ex` @ `0x0C7AF4`)**:
   - Under 32-bit ARM AAPCS, `va_list` (`void *`) is a single 4-byte pointer passed **by value** in a register (`r2` in `spicy_log_emit(self, log_id, ap)` via `vtable[+0x08]`, and `r3` in `spicy_log_emit_ex(self, level, log_id, ap)` via `vtable[+0x10]`), whereas `str r2, [sp]` / `str r3, [sp, #4]` in the raw prologues are compiler `va_start` local spills rather than stack arguments.

---

## 4. Partial-Link Raw Code Pointers & Boundary Stubs (`known_deviations`)

Because Phase 1 reconstructs `108` functions + `5` naked veneers out of `17,479` IDB functions while preserving byte-exact `.rodata.image` (`0x001BFA00..0x001F389A`) and `.data.image` (`0x20000000..0x2000F138`), `verification/ledger.json` (`known_deviations`) and `tools/verify_claims.py` mechanically audit the exact partial-link boundaries on every build:

- **`.rodata.image` (`212,634` bytes)**: Contains `13,980` aligned 32-bit words matching Thumb `ROM_TEXT` range (`0xC0..0x1BFA00` with bit 0 set), of which `8,451` match top-level IDB function entry RVAs:
  - `87` `retained_native` pointers in the secondary vector copy (`0x001BFA04..0x001BFB5F`) — **all 87 resolve to identical live addresses** because all 5 `retained_native` veneers are pinned at their exact raw RVAs in `rkof.elf`.
  - `8,186` `pending_c` function entry pointers (including `37` top-level IDA entries inside the `53`-word `CTOR_EARLY` constructor table at `0x001F37B8..0x001F388C`) and `178` `reconstructed_c` raw RVA vtable/descriptor slots (preserved byte-for-byte in `.rodata.image`).
- **`.data.image` (`61,752` bytes)**: Contains `1,024` aligned Thumb `ROM_TEXT`-range words (`423` `pending_c` callback entry pointers and `4` `reconstructed_c` raw RVA slots), preserved byte-for-byte against `../../../rkof[0x200000..0x20F138]`.
- **`src/rtkit/pending_boundary.S` (`41` weak link stubs)**: Resolves external `pending_c` callees referenced by reconstructed C11 modules into `.text.pending.*` (never claimed as `reconstructed_c` or `retained_native`, and intercepted at both raw RVAs and ELF stub addresses during Unicorn differential execution).
