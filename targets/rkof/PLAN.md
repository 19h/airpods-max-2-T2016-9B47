# `rkof` — Gradual C11 Reconstruction Plan & Milestone Status

## Objective

Reconstruct `rkof` (Main RTKit Firmware, `2,159,008` bytes, `17,479` IDB functions) into an honest, recompileable freestanding ARMv7E-M (`cortex-m4`, `fpv4-sp-d16`) + host-verifiable C11 codebase following the exact engineering standards of `phyb_re`:

- Zero fabricated provenance (`reconstructed_c` strictly equals `@provenance`-annotated C11 functions; naked architectural veneers are tracked as `retained_native`; unrecovered application modules remain `pending_c`).
- Zero Hex-Rays decompiler artifacts (`HIWORD`, `LODWORD`, `BYTE1`, `__PAIR64__`, `goto LABEL_`, `v1..v99`, `a1..a99`).
- Byte-for-byte preservation of `ROM_VECTORS`, `.rtkit.header`, `ROM_RODATA`, and `RAM_DATA` against `../../../rkof`.
- Instruction-level semantic equivalence verified via Unicorn Cortex-M4F differential execution.

---

## Completed Milestones (Phase 1 — Foundation, RTKit Kernel & W3 PHY Supervisor)

1. **Complete Binary & IDB Inventory (`verification/inventory.json` & `verification/ledger.json`)** — **DONE**
   - Extracted all `17,479` functions, `48` vector entries, `34` segments, `uuid` v5 header, `36` FourCC TLVs, `fwsg` v1 trailer, `275` source paths, and `148` panic strings.
2. **Exact Linker & Load-Image Reconstruction (`link/rkof.ld`, `src/rtkit/vectors.c`, `src/rtkit/binary_blobs.S`)** — **DONE**
   - Pinned `ROM_VECTORS` (`0x00..0xBF`), `.rtkit.header` (`0xC0..0x107`), `rtkit_entry` (`0x108`), naked exception veneers (`0x1F8, 0x290, 0x2E0, 0x374, 0x16F70`), `.rodata.image` (`0x1BFA00..0x1F389A`), `.data.image` (`0x20000000..0x2000F138`), and 24 `RAM_BSS` state sections ending at exact boundary `0x200D46B8`.
3. **RTKit Early Boot, Fault/Exception, NVIC/SysTick, Scheduler & Core Services (`45` C functions)** — **DONE**
   - `boot.c`: `rtkit_entry` (`RMC1..3`/`RMD1..3`/`RMFI`/`RTSI` remap programming), `__stack_chk_init`, `__stack_chk_fail`, `rtkit_start` (stack-hole-preserving BSS zeroing, FPU CPACR init, MPU region probe, constructor walk), `sub_175C8`, `sub_175D4`.
   - `exception.c`: SCB fault/MPU capture, IPSR-to-exception-code mapping, coredump magic, panic/assert reporting (`10` functions).
   - `irq.c`, `sched.c`, `systick.c`, `services.c`: BASEPRI/PRIMASK critical sections, NVIC table dispatch & priority banding, CLZ ready-thread bitmap & context switch, 24 MHz SysTick periodic/one-shot/udelay, heap wrappers, carveout registry, and platform mode lookup (`29` functions).
4. **Freestanding Libc & Compiler-RT / VFPv4 Helpers (`26` C functions)** — **DONE**
   - `string.c`: `memchr`, `memcmp` (preserving aligned big-endian sign vs unaligned byte-diff behavior), `memcpy` + thunks, `memset`, `strchr`, `strcmp`, `strlen`, `strncmp`, `rtkit_mem_is_filled`, `vsnprintf`, `snprintf`.
   - `aeabi.c`: 64-bit shifts, unsigned/signed 64-bit division (`udivmoddi4`, `udivdi3`, `divdi3`), subnormal IEEE-754 normalizers, and hard-float VFPv4 conversions (`extendsfdf2`, `fixdfsi`, `fixsfsi64`, `fixunsdfsi`, `fixunssfdi`, `floatdisf`).
5. **Coprocessor Supervision & Complete W3 PHY Calibration / Reset / Crash Stack (`37` C functions)** — **DONE**
   - `coproc/supervisor.c` (`8` functions), `phy/modem_inits.c` (`8` functions), `phy/scanmedia.c` (`5` functions), `phy/wnvv.c` (`7` functions), `phy/bt_phy.c` (`9` functions).
6. **Host Behavioral Suite + 100% Unicorn Cortex-M4F & Stub-Mutation Harness** — **DONE**
   - `123 / 123` host checks + `175 / 175` Unicorn ARMv7E-M differential cases covering `108 / 108` `reconstructed_c` symbols (`0` divergences) + `tools/verify_claims.py` stub-mutation & partial-link deviation audit.

---

## Roadmap for Subsequent Phases (`pending_c`: `17,366` Functions)

- **Phase 2 — RTKit IPC, Mailbox Endpoints & Power Controller (`0x18000..0x28000`, `0x186000..0x1A5000`)**:
  - Reconstruct `sub_22F14` / `sub_23B78` endpoint message routing, `rtkit_heap_alloc`/`free` (`0x15F98..0x16800`), coredump serializer (`sub_1880F8`), and PMGR power-domain vtable (`0x20001BD0`).
- **Phase 3 — Peripheral Drivers (`SPI`, `I2C`, `UART`, `OTP` `sub_18E930`, `AON` / `PMU`)**:
  - Reconstruct hardware OTP word reader (`sub_18E930`) and SoC bus controllers under `0x42000000..0x42480000`.
- **Phase 4 — Audio / Bluetooth Application Services & DSP0/DSP1/AOP IPC Clients**:
  - Gradually migrate application subsystems out of `pending_c`, adding per-subsystem Unicorn differential suites at each boundary.
