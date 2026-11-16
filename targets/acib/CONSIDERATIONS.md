# `acib` Architectural & Reconstruction Considerations

This document records the hardware, ABI, memory-layout, ledger-accounting, and emulation invariants discovered during the analysis of `acib` (`b515d` / `AirPods3,6` Bluetooth MAC / ACI controller firmware) and enforced by the verification harness.

---

## 1. Processor, Soft-Float ABI & Compiler Fingerprint

- **ARMv7E-M Soft-Float (`-mfloat-abi=soft`)**:
  - Unlike `phyb` (which enables `CPACR` bits `[23:20]` and executes hardware VFP instructions), `acib` contains **zero** hardware VFP/NEON instructions across all `982,504` bytes of `ROM_TEXT` (`0x0000C0..0x0EFEA8`) and never enables `SCB->CPACR` (`0xE000ED88`); software IEEE-754 double helpers reside in ROM `0x0E50B8..0x0E564B`.
  - Compiling with `-target armv7em-none-eabi -mcpu=cortex-m4 -mfloat-abi=soft -mthumb` matches the firmware's integer-only register ABI (`r0–r3`, `r7` frame pointer, `lr`) and exception frames (`r4–r11, lr`).
- **Compiler Characteristics**:
  - Built with Apple Clang / LLVM (`-Oz`, Thumb-2) on `RTKit_release-3514.0.15.release` (`SDK version 9A347`, product `3.91.1.0`).
  - Uses Apple's `R7` frame-pointer chain with bottom-of-stack sentinel `R7 = 0x77777700` seeded by `Reset_Handler` (`0x000108`).

---

## 2. Dual Vector Tables (`0x0` vs `0x400`) & Interrupt Routing

- **Primary Vector Table (`0x00000000..0x000000BF`, 48 Entries)**:
  - Slot `0` (`Initial MSP`) = `0x1001BD80` (top of the 2 KB MSP stack `0x1001B580..0x1001BD80`; the 1.5 KB init PSP stack sits immediately below at `0x1001AF80..0x1001B580`, described by ROM descriptors at `0x0F52D4` and `0x0F52E0` and painted with `"RTKSTACK"` by `rtk_init_stacks_watermark` @ `0x070BB4`).
  - Slot `1` (`Reset`) = `0x00000109`, Slot `2` (`NMI`) = `0x000E4A95`, Slots `3..6` (`HardFault..UsageFault`) = `0x000E4A51`, Slots `11` & `14` (`SVCall`, `PendSV`) = `0x000E4B15`, Slot `15` (`SysTick`) = `0x000E58F1`.
  - Vector words `[17]` (`0x00000044 = 0x20150101`) and `[18]` (`0x00000048 = 0x0003005B`) store embedded protocol/banner metadata (`3.91` = major `3`, minor `0x5B = 91`), read by `acib_log_build_banner_and_start` (`0x070BF0`) and echoed in the MAC->PHY op `0` handshake. `src/vectors.c` reproduces all 48 words byte-for-byte (`verify_load_image.py`).
- **Secondary Vector Table (`0x00000400..0x0000063F`, 144 Entries = 16 Core + 128 External IRQs)**:
  - During RTKit startup, `sub_707F0` (`0x0709DC`) writes `SCB->VTOR = 0x00000400` (`0xE000ED08`).
  - Entries `[15..143]` all point to `0x000E58F1` (`SysTick_Handler` pinned at VMA `0x000E58F0` in `link/acib.ld`), routing SysTick and all 128 external interrupts into `rtk_nvic_dispatch_irq` (`0x0E5B0C`), which reads `IPSR` and dispatches `g_rtk_irq_handlers[vec - 14]` (`0x1000EEB4`) with `g_rtk_irq_contexts[vec - 14]` (`0x1000F0BC`).

---

## 3. `fwsg` v1 Loader Layout & Warm-Sleep Resume (`0xAA11CE55` / `0xBAADF1AC`)

- **`fwsg` v1 Segment Trailer (`0x104480..0x1044E0`)**:
  - Raw `acib` ends with a 96-byte `fwsg` v1 segment descriptor table (`0x104480..0x1044C0`) and 32-byte footer (`0x1044C0..0x1044E0`) describing two host-loaded segments:
    - `__TEXT`: file `0x000000..0x0F984C` -> VMA `0x00000000..0x000F984C` (`memsz = 0xF984C`).
    - `__DATA`: file `0x0F9860..0x10447E` -> VMA `0x10000000..0x1000AC1E` (`filesz = 0xAC1E`, `memsz = 0x2D2C4`, covering `RAM_DATA` + `RAM_BSS` up to `RTSZ = 0x1002D2C4`).
- **Cold Boot vs Warm-Sleep Resume (`Reset_Handler` @ `0x000108`)**:
  - On entry, `Reset_Handler` checks `*(uint32_t *)0x10000074 == 0xAA11CE55`: on warm wake from deep sleep, `rtk_sleep_restore_context` (`0x0E4BB0`) stamps `0xBAADF1AC` into `0x10000074` and restores NVIC/SCB/CPU context from `0x1000006C`.
  - On cold boot, `Reset_Handler` zeroes `.bss` (`0x1000AC40..0x1002D2C4`), seeds `__stack_chk_guard` (`0x10002534`) from TLV `STKG` (`0x0AFF`), invokes the 61 static constructors at `0x0F9758..0x0F984C`, sets `R7 = 0x77777700`, and calls `main_entry` (`0x071A50`).

---

## 4. Strict `retained_native` Discipline & Documented `known_deviations`

- **Byte-Exact `retained_native` (`1` Function)**:
  - Only `NMI_Handler` (`0x0E4A94`, `.text.nmi` `128` bytes = `114`B handler body + `14`B literal pool at `0x0E4B06..0x0E4B14`) is marked `retained_native` in `verification/ledger.json`, because its `bl SysTick_Handler` (`0x0E58F0`), `g_rtk_nmi_saved_sp` (`0x1002D2B8`), and stack bounds (`0x1001B580`, `0x000F52E0`) resolve to identical addresses, making all 128 bytes 100% bit-for-bit identical to raw `acib` (enforced on every `verify-semantics` run by `tools/verify_load_image.py`).
- **Known Deviations (`verification/ledger.json["known_deviations"]`)**:
  - The 4 naked ARMv7-M bootstrap/exception veneers (`Reset_Handler` `0x108`, `Fault_Handler` `0xE4A50`, `SVCall_Handler` `0xE4B14`, `rtk_sleep_restore_context` `0xE4BB0`) branch to relocated `.text` targets and are therefore classified honestly as `pending_c` (never `retained_native` or `reconstructed_c`).
  - Binary-preserved `.rom.rodata` (`1,401` odd code-range words, including the 61 constructors at `0xF9758..0xF984C`), `.vectors.secondary` (`143` odd words, where `[15..143]` match pinned `SysTick_Handler` `0x0E58F1`), and `.ram.data` (`158` odd code-range words) retain raw Thumb RVAs until full-image symbolization.

---

## 5. Portability (32-Bit ARM vs 64-Bit Host) & Unicorn Differential Nuances

- **64-Bit Host Address Translation (`ACIB_HOST_TEST`)**:
  - Fixed-VMA firmware regions (`0x00000000..0x000F984C`, `0x10000000..0x10030000`, `0x4D280000..0x4D281000`) route through `acib_addr_ptr(addr)` in `include/acib/bus.h`, backed by flat host memory buffers pre-loaded with the exact `rom_rodata.bin` and `ram_data.bin` slices in `tests/host_bus.c`. Peripheral MMIO (`0x42440000`, `0x4D154000`, `0x520DC000..0x522DBFFF`, `0xE0001000..0xE000FFFF`) routes through `acib_mmio_read32` / `acib_mmio_write32` spy callbacks.
- **Unaligned Mailbox Accesses**:
  - Two fields in the `0x4D280000` MAC<->PHY shared RAM are **intentionally unaligned** in Apple's wire protocol:
    - `0x4D2801F9` (`req + 0x1F9`): 32-bit coexistence grant word (`phy_ipc_coex_ring_doorbell` @ `0x07DB44`).
    - `0x4D2805C1` (`resp + 0x1C1`): five 32-bit PHY crash words (`phy_ipc_irq_handler` @ `0x07CD5C`).
- **Unicorn 2.1.4 M-Profile Differential Execution & Stub-Mutation Audit (`tools/arm_semantics.py`)**:
  - `UC_CPU_ARM_CORTEX_M4` is selected via `ctl_set_cpu_model` so `MRS`/`MSR` (`PRIMASK`, `BASEPRI`, `IPSR`) execute with Cortex-M4 semantics, and `UC_HOOK_MEM_*` callbacks are attached **only** to peripheral MMIO ranges (`MMIO_RANGES`) so Thumb `IT` block condition flags are never clobbered on ROM/SRAM/mailbox accesses.
  - Every `verify-semantics` run enforces `100%` `reconstructed_c` coverage (`120 / 120` functions, `200` cases, `0` divergences), ordered `pending_c` callee/parameter/varargs trace equivalence across all 57 hooked link stubs, and a 3-stub (`bx lr`, `movs r0,#0; bx lr`, `movs r0,#1; bx lr`) mutation audit (`ctl_remove_cache` flushed per trial, `0 / 357` survivors) so vacuous test cases cannot pass.
