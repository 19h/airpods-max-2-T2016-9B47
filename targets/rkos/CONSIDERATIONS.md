# Considerations — `rkos`

## 1. What "reconstructed" means here

Revision 2–3 follow the `verifier-rtk` audits (board #18 and #22). Revision 1 counted 171 functions as `reconstructed_c`, but many of those C bodies were invented or truncated (`rtkit_main`, `rtkit_boot_stage2`, `sub_3B58`, `sub_3EAC`, `crashlog_dump`, `sub_12E38`, …). The rule now is mechanical. A function is `reconstructed_c` only if at least one direct Unicorn case at its raw RVA matches, zero cases diverge, AND its cases defeat `bx lr`, `movs r0, #0; bx lr`, and `movs r0, #1; bx lr` stub mutations unless the raw bytes are literally identical to that stub. `tools/verify_claims.py` enforces both gates on every run. Every other C body is `pending_c` and tagged `@shim`. Shims can still be called from verified code in `rkos.elf`, but the raw-vs-ELF tests always enter verified functions directly.

Note on `memcpy` (`0x12C0`) vs `sub_12C6` (`0x12C6`): raw `0x12C0` executes `subs r3, r0, r1; it eq; bxeq lr` and falls straight into `0x12C6` (`cmp r3, r2; blo backward_copy`), which IDA split into a separate function entry after the conditional `bxeq lr`. Thus `rkos_memcpy` (`0x12C0`) is the 3-arg combined `memmove`/`memcpy` entry, while `rkos_memmove_body` (`0x12C6`) takes `delta = (uintptr_t)dst - (uintptr_t)src` in AAPCS `r3`.

A matched case shows equivalence only for the inputs and memory regions that case covers. It is not a proof. Treat the `evidence` lists as the extent of the claim.

## 2. Boot entry (raw behaviour; the shims do NOT implement this)

- `Reset_Handler` `0x108`: if `*0x4000C080` bit1 is clear, it clears bit16 of `*0x4000C02C` and writes ROM words (`0x1640C..0x16454`, +8) to `0x4000C0A0..0x4000C0B8` and `0x4000C120`, polling `*0x4000C12C == 1` between writes. It then sets bit16 again, stores ROM word `0x16474` to `*0x008000F0`, and branches to `Reset_Main`.
- `Reset_Main` `0x1F8`: reads `*0x20000190`. `0xBAADF1AC` → branch `0x28C`. `0xCAFE4B0B` → `bx *0x20000194` (warm resume). Otherwise it stores `0xBAADF1AC`, sets MSP/PSP from ROM words `0x1429C`/`0x14298`, paints both stacks from a 64-bit pattern at `0x142E0`, sets CONTROL.SPSEL, and branches to `rtkit_start` `0x32CC`.
- Revision 1 claimed a `'*L%h%'` (`0x2568254C`) warm-boot magic. That was wrong: the bytes at `0x11D` are an unaligned instruction-stream coincidence.

## 3. Vector tables and pinned shims

The boot vectors (`0x0`) and the runtime VTOR (`0x13200`, written in `sub_47C`/`sub_3B0C`/`sub_3838`) are verbatim raw bytes. `link/rkos.ld` pins the shim symbols at the raw handler RVAs, so these tables resolve to *something* inside `rkos.elf`. That something is a hand-written shim, not the original handler.

## 4. Unrelocated code pointers (known deviation)

`ROM_RODATA` and `RAM_DATA` are copied from the raw image so that the load-image check is byte-exact. Code pointers inside them still use the raw layout. Example: `.init_array` = `{0x12B29, 0x12BF9, 0x12C25, 0x12C31}`, while the C constructors live elsewhere in `rkos.elf`. `verify_claims.py` recounts these pointers on every run (473 unrelocated, plus 87 VTOR slots that resolve only to shims). The `rtkit_start` shim calls the constructors directly. Fixing this needs either relocating the blobs, which would break byte-exactness, or linking every target at its raw RVA.

## 5. Shared `K512` table (`0x16180`)

SHA-256 `0x11DC8` reads the high words of the first 64 entries of the SHA-512 constant table. Differential cases cover both compress functions and one-shot digest/HMAC.

## 6. Heap block cookies

Block headers XOR `blk_va ^ __stack_chk_guard` with `0x40DA110C` (allocated), `0x1BADF4EE` (scrubbed free), or `0xF4EE705E` (unscrubbed free). `heap_free_impl`'s return value diverges from raw, so it and `heap_free_blk` are shims. `rtkit_malloc`/`rtkit_free`/`calloc`/`memalign` match raw for the covered cases.
