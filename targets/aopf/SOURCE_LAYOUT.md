# Source Layout & Firmware Provenance — `targets/aopf/`

The directory layout separates freestanding C/C++ runtime primitives, RTKit OS
plumbing, hardware peripheral drivers, and Always-On Processor (`AppleSPU` /
Laguna) application services.

```text
targets/aopf/
+-- Makefile                    ARMv7E-M + host build, check, verify, verify-semantics
+-- link/
|   +-- aopf.ld                 Exact ROM/SRAM memory map + exact-RVA vector/vtable veneers
+-- include/aopf/
|   +-- memmap.h                ROM/SRAM/stack/global VMAs and 17 peripheral MMIO apertures
|   +-- regs.h                  ARMv7E-M SCS/NVIC/MPU/FPU + Fuse/PMGR/WDT/Timebase/AKF bitfields
|   +-- cpu.h                   Thumb-2 intrinsics + host CPU register stubs
|   +-- bus.h                   32-bit & 128-bit volatile MMIO bus accessors (ARM + host sparse bus)
|   +-- shared.h                Binary-exact headers, vtables, device structs (_Static_assert-checked)
|   +-- libc.h                  Freestanding string/memory/printf/heap/C++ ABI prototypes
|   +-- rtkit.h                 Boot, TLV, exception, IRQ, fastlock, mailbox, & driver prototypes
|   +-- aopf.h                  Top-level AOP timestamp, SOCR mixer, audio log, & PropertyTable API
+-- src/
|   +-- vectors.c               Typed 48-entry boot vector table (.vectors @ 0x00000000)
|   +-- libc/
|   |   +-- string.c            memchr, memcpy, memmove, memset, strlen, strcmp, strncmp, 64-bit div/shift, CRC-32
|   |   +-- printf.c            rtk_vformat, rtk_printf, rtk_snprintf, rtk_vsnprintf, rtk_i64_to_dec, rtk_call_once
|   |   +-- cxxabi.c            Tail-splitting free-list heap (rtk_heap_alloc, malloc, calloc) + __cxa_guard_* / new
|   +-- rtkit/
|   |   +-- image_blobs.c       Typed uuid v5 header (.rtkit.header) + verbatim ROM_RODATA & RAM_DATA blobs
|   |   +-- boot.c              Reset/Fault/SVC entry shims (@shim), STKG stack guard init, TLV lookup, rtk_start, MPU
|   |   +-- exception.c         Fault frame formatting & verbatim firmware panic/assert wrappers (@shim)
|   |   +-- irq.c               BASEPRI/PRIMASK critical sections, NVIC driver (0x20819808), device MMIO claim
|   |   +-- threading.c         Spinlock, recursive fastlock (LDREX/STREX), stack watermark, VM region alloc
|   |   +-- ipc.c               AKF 128-bit mailbox FIFO pop, tryReadOne, IRQ registration, & Mbox panic check
|   |   +-- vt_trampolines.c    46 exact-RVA Thumb veneers bridging ROM_RODATA C driver vtables into C11
|   +-- drivers/
|   |   +-- systick.c           ARM SysTick one-shot/periodic timer driver (0x2080CBA8)
|   |   +-- timers.c            1 MHz 48-bit Timebase (0x42016000) & multi-channel Timer Bank (0x42020000)
|   |   +-- platform.c          Fuse/OTP (0x4201C000), PMGR power state (0x42440000), & AON Watchdog (0x42200000)
|   +-- aop/
|       +-- app.c               aop_get_timestamp_us, aop_socr_lfsr_mix, aud_log_*, PropertyTable_*, Battery_getBuildString
+-- tests/
|   +-- host_bus.c              Sparse MMIO + SRAM mirror backing store and longjmp panic trap for host tests
|   +-- layout.c                Compile-time _Static_asserts on all binary-backed struct sizes and field offsets
|   +-- test_main.c             Native host behavioral test runner (verifies ROM/RAM blobs, libc, heap, drivers, AOP)
+-- tools/
|   +-- verify_fidelity.py      Bans Hex-Rays idioms and enforces @provenance / @shim / ledger 1-to-1 sync
|   +-- verify_layout.py        Verifies ELF section bounds, VMA/LMA alignment, and zero section overlap
|   +-- verify_load_image.py    Proves 100.00% byte-for-byte match of all 4 binary-backed ELF sections vs ../../../aopf
|   +-- arm_semantics.py        Unicorn ARMv7E-M differential harness (214 cases, 91 functions, MMIO R+W, udf ROM trap) + 3-stub gate
|   +-- verify_claims.py        Enforces Unicorn + stub-mutation evidence on all reconstructed_c & audits known_deviations
+-- verification/
    +-- inventory.json          Complete 3,915-function, 24-segment, vector, TLV, IPC, & string inventory from aopf.i64
    +-- ledger.json             Full 3,915-function classification ledger, per-function Unicorn evidence, & known_deviations
```

## Surviving Firmware Source Path Evidence (`inventory.json["source_paths"]`)

 Ten source filenames survive verbatim in `ROM_RODATA` assertion/log strings and anchor subsystem boundaries:

| Surviving Firmware Path | Reconstructed / Planned Location | Subsystem Role |
|---|---|---|
| `laguna_service_t2016.c` (line `314`) | `src/rtkit/exception.c` (`laguna_assert`), `src/drivers/platform.c` | T2016 Laguna PMU/SPMI/platform service assertions |
| `AppleSPU.cpp` | Phase 4 (`src/aop/`) | Core Embedded C++ AppleSPU service base class |
| `AppleSPUHIDDriver.cpp` | Phase 4 (`src/aop/`) | `ep20-hid` HID sensor event dispatcher |
| `AudioController.cpp` | Phase 5 (`src/aop/`) | `aop-audio` / `ull-audio` controller |
| `VoiceTrigger.cpp` | Phase 5 (`src/aop/`) | `aop-voicetrigger` streaming keyword detector |
| `SPMISensor.cpp` | Phase 3 (`src/drivers/`) | SPMI PMIC/sensor transport (`0x42038000`) |
| `AppleAOPSPI.cpp` | Phase 3 (`src/drivers/`) | AOP SPI master (`0x42008000`, `0x42015000`) |
| `AppleAOPI2C.cpp` | Phase 3 (`src/drivers/`) | AOP I2CM0 master (`0x42010000`) |
| `PropertyTable.cpp` | `src/aop/app.c` (`PropertyTable_lookup`, `PropertyTable_findOrAlloc`) | Key-value property hash table & pool allocator |
| `AudioProductsBattery` (`BNI:...`) | `src/aop/app.c` (`Battery_getBuildString`) | Battery firmware component build-string selector |
