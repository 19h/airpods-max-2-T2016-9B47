# `acib` Source Tree Layout

All reconstructed source, headers, linker scripts, host tests, verification scripts, and function ledgers for `acib` live self-contained under `targets/acib/`:

```text
targets/acib/
├── Makefile                        # ARMv7E-M (soft-float) + Host C11 build & verification targets
├── README.md                       # Quickstart, memory map, MAC<->PHY contract & test summary
├── PLAN.md                         # Phased C11 reconstruction roadmap (120 / 9,917 complete)
├── CONSIDERATIONS.md               # ABI, dual-VTOR, fwsg layout, retained ASM & Unicorn invariants
├── SOURCE_LAYOUT.md                # Directory & RVA-range guide (this file)
├── link/
│   └── acib.ld                     # Exact ROM (0x0..0xF984C) & SRAM (0x10000000..0x1002D2C4) layout
├── include/
│   └── acib/
│       ├── acib.h                  # Compiler attributes (section/naked/used) & fixed-width helpers
│       ├── memmap.h                # ROM/SRAM/MPU/MMIO/shared-RAM base addresses & sizes
│       ├── bus.h                   # Direct volatile ARM MMIO + 64-bit host bus shims + unaligned LE helpers
│       ├── cpu.h                   # ARMv7E-M special-register (PRIMASK/BASEPRI/IPSR/MSP/PSP/DMB/ISB) API
│       ├── regs.h                  # Cortex-M4 SCS/SCB/NVIC/MPU/SysTick/DWT + W3 PHY doorbell + SoC timers
│       ├── vectors.h               # Primary 48-entry & secondary 144-entry Cortex-M vector table types
│       ├── shared.h                # MAC<->PHY 0x4D280000 req/resp structs, Dekker lock, FRS, coex, crash copy
│       ├── rtkit.h                 # RTKit 'uuid' v5 header (0x48 B), stack desc, boot/MPU/exception/IRQ API
│       ├── libc.h                  # Freestanding memory/string & 64-bit AEABI shift prototypes
│       ├── phy_ipc.h               # 35 MAC<->PHY IPC, Dekker sync, FRS, coex & SEQ_RF_CTRL prototypes
│       └── aci.h                   # TraceManager buffer/packet API, OSA mailbox relay, HCI/BT entry points
├── src/
│   ├── vectors.c                   # Primary 48-slot Cortex-M vector table @ 0x00000000..0x000000BF
│   ├── pending_links.c             # Explicit pending_c link-boundary symbols for unported callees
│   ├── rtkit/
│   │   ├── image_blobs.S           # Exact .incbin slices for .rtkit.header, .vectors.secondary, .rom.rodata, .ram.data
│   │   ├── image.c                 # 35 pinned SRAM globals verified against linker map by verify_layout.py
│   │   ├── boot.c                  # 7-region MPU, CPU Hz calibration, DWT, board props, RTKSTACK watermarks
│   │   ├── exception.c             # Byte-exact NMI_Handler (.text.nmi), fault frame extractor, LR unwinder, panics
│   │   └── irq.c                   # PRIMASK/BASEPRI, NVIC methods, pinned SysTick_Handler, priority scheduler
│   ├── libc/
│   │   └── string.c                # memchr/memcmp/memcpy_wrapper/memmove/memset/strlen/strncmp + AEABI shifts
│   ├── phy/
│   │   └── phy_ipc.c               # MAC<->PHY 0x4D280000 & 0x4D154000 IPC, Dekker lock, FRS, coex, crash IRQ
│   └── aci/
│       ├── trace.c                 # TraceManager buffer control blocks & timestamped packet framing
│       ├── osa.c                   # OSA mailbox relay table lookup, buffer owner tag & crash event snapshot
│       └── bt_ctrl.c               # main_entry, banner, BT system thread init, HCI test opcodes, BD_ADDR, TS ring
├── tests/
│   ├── layout.c                    # Compile-time _Static_assert checks (ARM32 + Host64) on structs & VMAs
│   ├── host_bus.h                  # 64-bit host virtual ROM/SRAM/shared-RAM/MMIO & panic/PHY spy API
│   ├── host_bus.c                  # Host bus implementation loading exact raw slices & simulating W3 PHY slave
│   └── test_main.c                 # 128 behavioral checks across all reconstructed subsystem suites
├── tools/
│   ├── extract_blobs.py            # Slices raw acib into build-dir binary blobs (.rtkit.header/.rom.rodata/.ram.data)
│   ├── gen_verification.py         # Generates inventory.json & ledger.json from raw acib + IDA exports
│   ├── verify_fidelity.py          # Enforces 0 Hex-Rays anti-idioms, @provenance RVAs & 9,917-fn ledger integrity
│   ├── verify_layout.py            # Audits 12 ELF sections, 35 pinned symbol VMAs & unpinned_bss == 0
│   ├── verify_load_image.py        # Verifies 6 binary-backed sections (84,362 B) + 12/12 SHF_ALLOC PT_LOAD matches
│   └── arm_semantics.py            # 200-case Unicorn Cortex-M4 differential + stub-callee + 357-trial mutation harness
└── verification/
    ├── inventory.json              # Complete segment, fwsg, RTKit TLV, MMIO, string & 9,917-function catalog
    └── ledger.json                 # Per-function ledger (120 reconstructed_c, 1 retained_native, 9,796 pending_c)
```

---

## Module-to-ROM RVA Mapping

| Source File | `reconstructed_c` | `retained_native` | Firmware RVA Span(s) | Original Firmware Subsystem |
| :--- | :---: | :---: | :--- | :--- |
| `src/vectors.c` | *(48-entry table)* | — | `0x000000..0x0000BF` | Primary Cortex-M vector table (byte-exact) |
| `src/rtkit/boot.c` | `20` | — | `0x0706D0..0x0707C0`, `0x070BB4`, `0x07DDF0..0x07E1D8`, `0x0E60A8..0x0E6102` | 7-region MPU, DWT, CPU clock measure, board props, `"RTKSTACK"` watermarks |
| `src/rtkit/exception.c` | `8` | `1` (`0x0E4A94`) | `0x06BF30`, `0x071230`, `0x0725FC..0x072658`, `0x0E4A94..0x0E4B14`, `0x0E4C48`, `0x0E6040`, `0x0E6474` | Byte-exact `NMI_Handler` (`.text.nmi`), fault frame extractor, Thumb-2 LR unwinder, panics |
| `src/rtkit/irq.c` | `21` | — | `0x0E5844..0x0E5C50`, `0x0E6104`, `0x0EF0DC` | PRIMASK/BASEPRI, NVIC controller methods, pinned `SysTick_Handler` (`0x0E58F0`), scheduler |
| `src/libc/string.c` | `9` | — | `0x0E4C58..0x0E50AC`, `0x0E53DC` | Freestanding `mem*`/`str*` & 64-bit AEABI shift runtime |
| `src/phy/phy_ipc.c` | `35` | — | `0x07A0DC..0x07A12C`, `0x07C820..0x07DCB4`, `0x084E74..0x084F14`, `0x097920` | MAC<->W3 PHY shared mailbox (`0x4D280000`), Dekker lock, FRS, coex, `0x4D154000` doorbells |
| `src/aci/trace.c` | `12` | — | `0x0E44A0..0x0E4524`, `0x0E47D0..0x0E4875` | `TraceManager` buffer control blocks & timestamped trace packet framing |
| `src/aci/osa.c` | `5` | — | `0x072570..0x0729BC` | OSA mailbox relay table (`0x0F5434`), task pending/count & crash event snapshot |
| `src/aci/bt_ctrl.c` | `10` | — | `0x002EA8`, `0x020A58`, `0x070BF0`, `0x071A50..0x071AA4`, `0x078D44..0x078DC0`, `0x0AEF9E` | `main_entry`, banner, `bt_system_thread_init`, HCI test opcodes, `BD_ADDR` swap, BT TS ring |
| **Total** | **`120`** | **`1`** | `0x000000..0x0EFEA8` | `120 reconstructed_c` + `1 retained_native` + `9,796 pending_c` = **`9,917` functions** |
