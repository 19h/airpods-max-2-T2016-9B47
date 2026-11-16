# `acib` Gradual C11 Reconstruction Plan

This document tracks the phased, verification-gated C11 reconstruction of `acib` (`9,917` total functions across `982,504` bytes of `ROM_TEXT` `0x0000C0..0x0EFEA8`).

## Guiding Engineering Rules

1. **Honest Function Accounting (`verification/ledger.json`)**:
   - Every one of the `9,917` functions extracted from `acib.i64` is tracked in `verification/inventory.json` and `verification/ledger.json` under exactly one status:
     - `reconstructed_c` (`120`): Clean, idiomatic C11 implementation compiled for ARMv7E-M (`acib.elf`) with `@provenance RVA 0x... <ida_name>`, 100% exercised by Unicorn differential execution against raw `acib` (`0` divergences) and passing a 3-stub (`bx lr`, `ret 0`, `ret 1`) mutation audit (`0` survivors).
     - `retained_native` (`1`): Unmodified native instruction slice (`NMI_Handler` @ `0x0E4A94`, `.text.nmi` `128` bytes = `114`B body + `14`B literal pool) verified 100% byte-identical against raw `acib` by `tools/verify_load_image.py`.
     - `pending_c` (`9,796`): Remaining firmware functions not yet reconstructed into C11, explicitly including both relocated naked entry veneers (`Reset_Handler` `0x108`, `Fault_Handler` `0xE4A50`, `SVCall_Handler` `0xE4B14`, `rtk_sleep_restore_context` `0xE4BB0`) and link-boundary stubs in `src/pending_links.c`, documented under `known_deviations` in `verification/ledger.json`.
2. **Zero Decompiler Anti-Idioms**:
   - Enforced on every build by `tools/verify_fidelity.py` (and workspace-wide `tools/verify_all_targets.py`): zero `LODWORD`/`HIDWORD`/`BYTE1`/`__PAIR64__`/`__OFSUB__`, zero `goto LABEL_*`, and zero Hex-Rays placeholder variable/parameter names (`v1..`, `a1..`).
3. **Dual-Target & Differential Verification Gate**:
   - Every reconstructed module compiles cleanly with `-std=c11 -Wall -Wextra -Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Werror` on both `clang -target armv7em-none-eabi -mcpu=cortex-m4 -mfloat-abi=soft` and host `cc`, passes 128 host behavioral tests (`acib_tests`), preserves exact `ROM_VECTORS` / `.rtkit.header` / `.vectors.secondary` / `.text.nmi` / `ROM_RODATA` / `RAM_DATA` load bytes (`84,362` B), and passes Unicorn ARMv7E-M differential execution + stub-mutation auditing.

---

## Phase Status

### Phase 1 — Foundation, RTKit Core, MAC<->PHY IPC Bridge & ACI Entry Points (**COMPLETE**)

- **Ledger Progress**: `120 reconstructed_c`, `1 retained_native`, `9,796 pending_c` (`9,917` total).
- **Completed Subsystems**:
  1. **Inventory & Build Infrastructure**:
     - Full extraction of `9,917` functions, `469` surviving strings, `fwsg` v1 segment trailer (`0x104480..0x1044E0`), RTKit `'uuid'` v5 header (`0xC0..0x108`) & 23 TLV metadata records (`0xF9640..0xF9756`), 61 constructors (`0xF9758..0xF984C`), and MMIO xrefs into `verification/inventory.json` & `verification/ledger.json`.
     - Linker script `link/acib.ld` + `src/vectors.c` + `src/rtkit/image_blobs.S` reproducing `.vectors` (`0x0..0xC0`), `.rtkit.header` (`0xC0..0x108`), `.vectors.secondary` (`0x400..0x640`), `.text.nmi` (`0xE4A94..0xE4B14`), `.rom.rodata` (`0xEFEA8..0xF984C`), `.ram.data` (`0x10000000..0x1000AC1E`), and pinned `.ram.bss` (`0x1000AC1E..0x1002D2C4`) byte-for-byte.
  2. **Boot, 7-Region MPU, DWT, CPU Clock Calibration, Board Props & Stack Watermark (`src/rtkit/boot.c` — 20 functions)**:
     - `acib_mpu_default_attr` (`0x0706D0`), `acib_mpu_enable` (`0x0706D8`), `acib_mpu_disable` (`0x0706E8`), `acib_mpu_config_region` (`0x0706F8`), `rtk_early_hw_init` (`0x07DEB8`), `acib_dwt_cyccnt_read` (`0x070734`), `acib_get_calibrated_cpu_hz` (`0x070788`), `acib_measure_cpu_hz` (`0x0707A4`), `acib_clock_timing_init` (`0x07DE50`), `acib_load_rtkit_board_props` (`0x07DDF0`), board/FPGA helpers (`0x07E18C..0x07E1C8`), and `"RTKSTACK"` watermark init/fill/check (`0x070BB4`, `0x0E60A8..0x0E60DC`).
  3. **Exception Frame Extraction, Unwind & Panic Reporting (`src/rtkit/exception.c` — 8 `reconstructed_c` + 1 `retained_native`)**:
     - `__stack_chk_fail` (`0x0E4C48`), `rtk_abort_panic` (`0x06BF30`), `acib_panic_null` (`0x071230`), `acib_panic_code_line` (`0x0725FC`), `acib_panic_code_group` (`0x072614`), `acib_ctx_store_fault_arg` (`0x07264C`), `rtk_fault_extract_frame` (`0x0E6040`), `rtk_unwind_adjust_lr` (`0x0E6474`), and byte-exact `NMI_Handler` (`0x0E4A94`).
  4. **IRQ Masking, NVIC Controller Methods, SysTick & Priority-Bitmap Scheduler (`src/rtkit/irq.c` — 21 functions)**:
     - `rtk_irq_mask_save`/`restore` (`0x0E5844`/`0x0E585C`), `rtk_primask_disable_save`/`restore` (`0x0E5862`/`0x0E5874`), `rtk_irq_enable_all` (`0x0E587E`), `rtk_irq_are_enabled` (`0x0E588C`), `rtk_irq_set_prio_group` (`0x0E58AC`), `rtk_cpu_in_thread_mode` (`0x0E58DC`), `SysTick_Handler` (`0x0E58F0`), `rtk_sched_switch` (`0x0E5908`), `rtk_spinlock_try_enter` (`0x0E6104`), and NVIC methods (`0x0E59A8..0x0E5C4C`).
  5. **Freestanding Libc & 64-Bit AEABI Shift Helpers (`src/libc/string.c` — 9 functions)**:
     - `acib_memchr` (`0x0E4C58`), `acib_memcmp` (`0x0E4C7C`), `memcpy_wrapper` (`0x0E4DA8`), `memmove` (`0x0E4DB0`), `memset` (`0x0E4EC0`), `acib_strlen` (`0x0E4F40`), `acib_strncmp` (`0x0E4FC8`), `acib_aeabi_llsl` (`0x0E5094`), `acib_aeabi_llsr` (`0x0E53DC`).
  6. **MAC<->PHY (`0x4D280000` / `0x4D154000`) IPC, Dekker Lock, FRS & Coex (`src/phy/phy_ipc.c` — 35 functions)**:
     - Full `acib`<->`phyb` shared memory & doorbell subsystem (`0x07A0DC..0x07A104`, `0x07C820..0x07DCA8`, `0x084E74`, `0x084EB8`, `0x097920`), including command/ack `{3, 0x5B, 1, 0}` handshake, B0 (`0x4D280400`) vs legacy (`0x4D2801E6`) response window detection, Dekker spinlock + 288B link-table sync, Fast RSSI Scan (FRS) 12-bit -> 24-bit packing & 15 ms abort path, coexistence doorbell, PHY crash W1C unpack (`reason = 0x5D`), and `SEQ_RF_CTRL` (`0x4D154038`) coordination.
  7. **ACI Trace Manager, OSA Mailbox Relay & BT Controller Entry Points (`src/aci/*.c` — 27 functions)**:
     - `tm_buffer_util_*` (`0x0E44A0..0x0E451C`), `tm_pkt_write_*` (`0x0E47D0..0x0E4828`), `osa_*` (`0x072570..0x0729AC`), `bt_system_thread_init` (`0x002EA8`), `hci_test_mode_get_opcode_from_type` (`0x020A58`), `acib_log_build_banner_and_start` (`0x070BF0`), `main_entry` (`0x071A50`), mode/slot accessors (`0x071A74..0x071A94`), BT timestamp ring (`0x078D44`, `0x078D84`), and `bt_bdaddr_swap` (`0x0AEF9E`).

---

## Roadmap for Subsequent Phases (`9,796` `pending_c` Functions)

### Phase 2 — RTKit Kernel Services, Endpoints & Power Management (~280 functions)
- Reconstruct remaining RTKit thread blocking/wakeup primitives, timers, semaphores, IOP/AP mailbox endpoints (`ep0` crashlog/power/syslog, ACI host transport endpoint), and deep-sleep entry/exit (`0x10000074` `0xAA11CE55`/`0xBAADF1AC` save/restore pipeline around `rtk_sleep_restore_context` @ `0x0E4BB0`).

### Phase 3 — OSA Runtime, Trace Formatting & NVRAM/Platform Drivers (~650 functions)
- Reconstruct full OSA message queues, buffer pools, timers, `TraceManager` ring transport, and `0x520DC000` / `0x42440000` hardware timer, event-trigger DMA (`0x521D8000`), and radio MAC register drivers.

### Phase 4 — HCI Command/Event Layer & Link Manager (`LM` / `LMP`) (~2,800 functions)
- Reconstruct HCI command parsers, Apple vendor-specific ACI opcodes, connection/key databases, Link Manager state machines, and SCO/eSCO audio control.

### Phase 5 — Classic Baseband Link Controller (`LC`) & BLE Link Layer (`LL`) (~6,066 functions)
- Reconstruct Classic BT Baseband, BLE 5.x Link Layer (advertising, scanning, connections, ISO/CIS), coexistence arbiter feeding `phy_ipc_coex_ring_doorbell`, and RF calibration orchestrators feeding `phy_ipc_*`.
