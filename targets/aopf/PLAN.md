# Reconstruction Plan — `aopf` (Always-On Processor Firmware)

## Phase 1 (Completed) — Foundation, RTKit Core, Platform Drivers & Mechanical Verification

1. **Complete Binary & IDB Inventory (`verification/inventory.json`, `verification/ledger.json`)**
   - Cataloged all `3,915` functions in `aopf.i64`, `24` segments (ROM, SRAM, 17 peripheral MMIO apertures, ARM DWT & SCS), `uuid` v5 image header (`61724c3e-0144-3e90-8882-7a4a637bd44a`), `fwsg` v1 segment reload trailer (`0x91BE0..0x91C40`), boot vectors (`48` entries at `0x0`), runtime VTOR table (`94` entries at `0x64000`), `10` surviving source paths, and IPC peer/endpoint descriptors.
   - Classified the mixed **Embedded C++ (`-fno-rtti -fno-exceptions`, 160 Itanium vtables, 13 `.init_array` global constructors)** + **RTKit C (`25` C driver vtables)** architecture.

2. **Freestanding ARMv7E-M + Native Host C11 Build (`Makefile`, `link/aopf.ld`)**
   - Built warning-clean (`-Wall -Wextra -Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Werror`) with `clang -target armv7em-none-eabi -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16` and host `cc`.
   - Preserved `100.00%` byte-for-byte identity across `.vectors`, `.rtkit.header`, `.rodata.rom_bank`, and `.data.ram_bank` (`187,596` bytes).
   - Bridged all `93/93` runtime VTOR vector entries and `142` RTKit C driver vtable pointer slots (`46` exact-RVA veneers in `src/rtkit/vt_trampolines.c`).

3. **Differential Execution & Anti-Vacuity Gate (`tools/arm_semantics.py`, `tools/verify_claims.py`)**
   - Reconstructed and verified **91 self-contained functions** across **214 Unicorn ARMv7E-M Thumb-2 test cases** (with `UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE` MMIO trace verification and `udf #255` ROM traps) with **0 divergences**:
     - `src/libc/`: `aopf_memchr`, `aopf_memcmp`, `aopf_strlen`, `aopf_strcmp`, `aopf_strncmp`, `rtk_memcpy` (4-arg `delta_r3` chunked `ldm/stm` semantics), `rtk_memcpy_wrap`, `rtk_memmove_entry`, `rtk_memset`, `rtk_stpncpy`, `rtk_strlcat`, `rtk_strlcpy`, `rtk_strnlen`, ARM `Rm[7:0]` 64-bit shifts (`rtk_u64_shl`, `rtk_u64_shr`), `rtk_u64_div_u32`, `rtk_i64_to_dec`, `rtk_vsnprintf_putc`, `rtk_abs_i32/i64`, CRC-8 & CRC-32 (`0x72EC8` table), and segregated free-list heap (`rtk_heap_alloc` tail-split, `rtk_heap_init_free_hdr`, `rtk_heap_insert_block`, `rtk_heap_unlink_block` with verbatim `"!Heapblk:%p"`).
     - `src/rtkit/`: `STKG` stack canary init (`0x6CBC`), critical-section `BASEPRI`/`PRIMASK` helpers, NVIC controller (`0x20819808`), non-blocking `rtk_spinlock_acquire` (`0x4EBC0`), leaf `rtk_fastlock_trylock` (`0x50424`), stack `"RTKSTACK"` watermark scanner, and AKF 128-bit mailbox FIFO pop/RX-empty IRQ arming/IRQ setup (`Mbox_armRxEmptyIrq` @ `0x5248E`, `Mbox_popOne` @ `0x524B0`, `Mbox_tryReadOne` @ `0x523B4`, `Mbox_setIrqHandler` @ `0x52244`).
     - `src/drivers/`: SysTick timer (`0x2080CBA8`), 1 MHz 48-bit Timebase (`0x42016000`), Timer Bank (`0x42020000`), PMGR power-state lookup (`0x42440000`), and Always-On Watchdog (`0x42200000`).
     - `src/aop/`: microsecond timestamp (`aop_get_timestamp_us`), `SOCR` TLV-gated 64-bit Gray-inverse mixer (`aop_socr_lfsr_mix`), Audio binary TLV ring log (`aud_log_*`), AppleSPU `PropertyTable` hash lookup/allocation (`PropertyTable_lookup`, `PropertyTable_findOrAlloc`), and `Battery_getBuildString`.
   - Enforced 3-stub mutation killing (`bx lr`, `ret 0`, `ret 1`) on all 91 `reconstructed_c` functions and classified all 70 unverified/slow-path-dependent C shims as `pending_c` (`@shim`).

## Subsequent Phases (Roadmap for Remaining 3,824 `pending_c` Functions)

- **Phase 2 — RTKit Scheduler, Slow-Path Fastlocks, Formatter & Full Driver `ioctl` Paths (~280 functions)**
  - Reconstruct RTKit mutex slow paths (`0x63D20` / `0x63BDC`), lazy system heap init (`0x4EF64`), full `rtk_vformat` jump-table branches, and `*_ioctl` state machines to promote the 70 `@shim` functions to `reconstructed_c`.
  - Reconstruct RTKit task switch (`SVCall`/`PendSV`), event queues, timers, and endpoint multiplexers (`ep0` management, crashlog/syslog/ioreport endpoints).
- **Phase 3 — Serial Bus & Sensor Transport Drivers (~420 functions)**
  - Reconstruct SCM/FIFO SPI (`0x42004000`, `0x42008000`, `0x42015000`), SCM/I2CM0 (`0x4200C000`, `0x42010000`), SPMI arbiter (`0x42038000`), and GPIO (`0x42019000`) drivers.
- **Phase 4 — AppleSPU / HID / Laguna Service Framework (~1,450 functions)**
  - Reconstruct the Embedded C++ `AppleSPU` service hierarchy (`160` Itanium vtables in `ROM_RODATA` and `13` `.init_array` constructors at `0x00077AE0..0x00077B14`), retiring the `2,550` unrelocated vtable slots in `ledger.json["known_deviations"]`.
- **Phase 5 — VoiceTrigger, Audio DSP & FPU Inference Kernels (~1,676 functions)**
  - Reconstruct the Cortex-M4F hard-float (`FPv4-SP-D16`) audio framing, biquad/FFT filterbanks, neural/ML feature normalization (`ml_normalize_and_infer` @ `0x0108`), and `aop-voicetrigger` / `aop-audio` / `ull-audio` pipelines.
