# Source Tree Layout — `targets/rkos/`

Counts are `reconstructed_c` / `@shim` (pending_c) per file.

```text
targets/rkos/
├── Makefile                        # all, arm, host, check, verify, verify-semantics, clean
├── README.md  PLAN.md  CONSIDERATIONS.md  SOURCE_LAYOUT.md
├── link/rkos.ld                    # ROM/RAM segments, pinned shim RVAs, 10 .state.* blocks
├── include/rkos/
│   ├── memmap.h                    # segment bounds, RAM landmarks, MMIO bases
│   ├── regs.h                      # SCS/NVIC/SCB/MPU/FPU registers
│   ├── shared.h                    # fwsg, uuid, DER, BMID, CLHE, heap, sched, .state.* structs
│   └── rkos.h                      # prototypes
├── src/
│   ├── vectors.c                   # typed 48-entry boot vector table
│   ├── image.c + image_*.inc       # byte-exact uuid header, ROM_RODATA, RAM_DATA
│   ├── state.c                     # .state.* globals covering all of RAM_BSS
│   ├── shims/arch_entry.c          # 0 / 9   hand-written naked entry shims (NOT raw code)
│   ├── libc/string.c               # 19 / 2
│   ├── libc/printf_stdlib.c        #  6 / 7
│   ├── kernel/heap.c               # 23 / 15
│   ├── kernel/crypto_der.c         # 12 / 5
│   ├── kernel/sched_sync.c         # 19 / 8
│   ├── rtkit/irq.c                 # 10 / 8
│   ├── rtkit/boot.c                #  5 / 13
│   └── platform/ftab_crashlog.c    #  9 / 10
├── tests/                          # layout.c (static asserts), host_bus.[ch], test_main.c (78 host checks)
├── tools/
│   ├── verify_fidelity.py          # anti-idiom, @provenance == reconstructed_c, @shim ⊆ pending_c
│   ├── verify_layout.py            # ELF/map section + pinned entry + state block check
│   ├── verify_load_image.py        # byte-exact blobs vs raw
│   ├── arm_semantics.py            # 528 Unicorn raw-vs-ELF cases (8 suites)
│   └── verify_claims.py            # direct evidence + bx_lr/ret0/ret1 stub-mutation gate + pointer recount
└── verification/
    ├── inventory.json
    └── ledger.json                 # 103 reconstructed_c / 0 retained_native / 761 pending_c (+known_deviations)
```
