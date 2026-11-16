# Reconstruction Plan — `targets/rkos/`

## Done (revision 2, after verifier-rtk #18)

1. Inventory: 864 functions, 15 segments, 241 strings, 162 MMIO xrefs, fwsg v1, uuid header (fixed a 4-byte mis-parse), TLVs, both vector tables, `.init_array`.
2. Byte-exact vectors / uuid / rodata / data (20,908 B).
3. **103 `reconstructed_c`**, each backed by direct Unicorn cases against raw:
   libc string/divmod (19), printf/strtoul/crc (6), heap allocator front-end and validators (23), DER + SHA-256/512 + digest/HMAC pieces (12), scheduler TAILQ, mutex fast paths, VM region registry (19), NVIC/IRQ helpers (10), BMID/remap/FTAB stream (9), 4 static ctors + stack paint (5).
4. Claim gate `tools/verify_claims.py`, plus a code-pointer recount.

## Next (ordered by value)

1. Replace the boot shims with real reconstructions of `Reset_Handler`/`Reset_Main` (§2 of CONSIDERATIONS.md). These need a naked asm prologue plus C, with Unicorn cases against `0x108`/`0x1F8` comparing the MMIO write sequence.
2. Turn the 68 C-shim bodies into real reconstructions, starting with the ones where raw and C differ least: `heap_free_impl` (return value), `rkos_cchmac_init` (ctx layout at +0x28), `rkos_nvic_is_enabled` (raw falls through into `sub_35BC`), and the heap freelist ops.
3. Reconstruct the large drivers from the decompilation, not by inventing them: `rtkit_main` `0x279C` (1096 B), `rtkit_boot_stage2` `0x4110`, `crashlog_dump` `0x67C0`, the unwinders `0x3B58`/`0x3EAC`, and `mutex_lock_slow`/`mutex_unlock_slow`.
4. Relocation strategy for blob code pointers (§4).
