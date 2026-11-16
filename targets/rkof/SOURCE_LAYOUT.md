# `rkof` — Source Tree & Verification Layout

All files owned by the `rkof` reconstruction reside strictly under `targets/rkof/`:

```text
targets/rkof/
├── Makefile                        # Freestanding ARMv7E-M + host C11 build & verification targets
├── README.md                       # Firmware identity, memory map, W3 cross-core findings & summary
├── PLAN.md                         # Reconstruction milestones & phased pending_c roadmap
├── CONSIDERATIONS.md               # Hardware remap, linker VMA pinning & instruction-level quirks
├── SOURCE_LAYOUT.md                # Directory & module map (this file)
├── link/
│   └── rkof.ld                     # Exact ROM/SRAM linker script + 24 pinned .state.* BSS blocks
├── include/rkof/
│   ├── rkof.h                      # Umbrella header
│   ├── bus.h                       # Freestanding volatile MMIO vs host bus translation macros
│   ├── cpu.h                       # ARMv7E-M intrinsics (BASEPRI, PRIMASK, CLZ, ROR16, DSB/ISB/WFI)
│   ├── memmap.h                    # Segment bounds, pinned RODATA/DATA globals & STATE_*_BASE macros
│   ├── regs.h                      # T2016 remap, pinmux, PMGR, RF PMGR & W3 PHY/modem MMIO registers
│   ├── shared.h                    # uuid v5, TLV, fwsg v1 & 0x4D280000..0x4D294000 shared SRAM structs
│   ├── libc.h                      # Freestanding string/printf & AEABI/VFPv4 helper prototypes
│   ├── rtkit.h                     # Boot, fault/panic, NVIC, scheduler, SysTick & service APIs
│   ├── coproc.h                    # Multi-coprocessor bitmask & btp carveout grant supervisor APIs
│   ├── phy.h                       # W3 PHY reset, warm-restore, OTP scanmedia, coredump & shell APIs
│   └── wnvv.h                      # Wireless NVRAM/OTP sorted tag list, CRC8 & byte-ring APIs
├── src/
│   ├── rtkit/
│   │   ├── vectors.c               # Typed 48-entry ARMv7-M vector table (100% byte match)
│   │   ├── binary_blobs.S          # .rtkit.header, .rodata.image, .data.image slices from ../../../rkof
│   │   ├── entry_veneers.S         # 5 retained_native naked ARMv7E-M exception/reset/SVC veneers
│   │   ├── pending_boundary.S      # Weak link-boundary stubs for external pending_c callees + BSS pins
│   │   ├── boot.c                  # rtkit_entry remap loader, __stack_chk_*, rtkit_start (6 funcs)
│   │   ├── exception.c             # SCB fault/MPU capture, IPSR mapper, assert/panic hooks (10 funcs)
│   │   ├── irq.c                   # BASEPRI/PRIMASK critical sections, NVIC dispatch & prio (11 funcs)
│   │   ├── sched.c                 # Thread context switch & CLZ ready-bitmap scanner (2 funcs)
│   │   ├── systick.c               # 24 MHz SysTick periodic/oneshot/stop & udelay poller (7 funcs)
│   │   └── services.c              # Heap wrappers, carveout registry, coredump overflow, mode (10 funcs)
│   ├── libc/
│   │   ├── string.c                # memchr, memcmp, memcpy, memset, str*, vsnprintf, snprintf (13 funcs)
│   │   └── aeabi.c                 # 64-bit shifts/divs & IEEE-754 VFPv4 float/double helpers (13 funcs)
│   ├── coproc/
│   │   └── supervisor.c            # Coproc mask lookup, btp grant refcounting & GTB snapshot (8 funcs)
│   └── phy/
│       ├── modem_inits.c           # W3 PHY/modem reset, RF PMGR init & 0x4D291008 warm-restore (8 funcs)
│       ├── scanmedia.c             # SRAM & hardware OTP 'NV' container scanner (5 funcs)
│       ├── wnvv.c                  # WNVV variable-width tag parser, 0x0Cxx append, CRC8 & ring (7 funcs)
│       └── bt_phy.c                # bt_phy_otp_init ('NV' @ 0x4D28D000), BPHY/MgrC coredump, shell (9 funcs)
├── tests/
│   ├── layout.c                    # Compile-time _Static_assert struct/offset proof (ARM32 + host)
│   ├── host_compat.h               # Host simulation state & longjmp panic trap declarations
│   ├── host_bus.c                  # Host address-space translator (ROM, SRAM, MMIO, W3 SRAM, SCS) & mocks
│   └── test_main.c                 # 123-check native behavioral test suite against raw ../../../rkof
├── tools/
│   ├── verify_fidelity.py          # Anti-Hex-Rays idiom scanner + @provenance & 17,479-fn ledger audit
│   ├── verify_layout.py            # LLD map verifier for fixed sections, STATE_*_BASE & veneer VMAs
│   ├── verify_load_image.py        # pyelftools PT_LOAD SHF_ALLOC, SHA-256 slice & 5/5 veneer byte proof
│   ├── arm_semantics.py            # Unicorn Cortex-M4F 175-case (108/108 symbol) differential harness
│   └── verify_claims.py            # 108/108 claim coverage, 3-stub mutation gate & known_deviations recount
└── verification/
    ├── inventory.json              # Complete IDB & binary inventory (17,479 functions, TLVs, strings)
    └── ledger.json                 # Complete 17,479-function ledger (108 C / 5 asm / 17,366 pending + deviations)
```
